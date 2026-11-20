// SPDX-License-Identifier: AGPL-3.0-only
#include "win/audio_out.h"

#include <windows.h>
#include <mmsystem.h>

#include <array>
#include <mutex>
#include <vector>

namespace {
// The PS5 sends 10 ms Opus frames, so these are roughly milliseconds / 10.
constexpr int kBlocks = 16;   // ring of waveOut blocks
constexpr int kPrime = 3;     // start (or restart after running dry) once this many are queued
constexpr int kMaxQueued = 6; // more than this queued: drop the frame to keep latency down
}  // namespace

// ponytail: waveOut adds ~40 ms on top of the network; switch to low-latency WASAPI if sound
// lags the picture too much.
struct AudioOut::Impl {
    std::mutex m;
    HWAVEOUT out = nullptr;
    unsigned channels = 0, rate = 0;
    std::array<WAVEHDR, kBlocks> hdr{};
    std::array<std::vector<std::int16_t>, kBlocks> data;
    int next = 0;
    bool priming = true;

    int queued() const {
        int n = 0;
        for (const WAVEHDR& h : hdr)
            if ((h.dwFlags & WHDR_PREPARED) && !(h.dwFlags & WHDR_DONE)) ++n;
        return n;
    }

    void closeLocked() {
        if (!out) return;
        waveOutReset(out);
        for (WAVEHDR& h : hdr)
            if (h.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(out, &h, sizeof(h));
        waveOutClose(out);
        out = nullptr;
        hdr = {};
        next = 0;
    }
};

AudioOut::AudioOut() : impl_(std::make_unique<Impl>()) {}

AudioOut::~AudioOut() { close(); }

void AudioOut::close() {
    std::lock_guard lock(impl_->m);
    impl_->closeLocked();
}

void AudioOut::setFormat(unsigned channels, unsigned rate) {
    Impl& s = *impl_;
    std::lock_guard lock(s.m);
    if (s.out && s.channels == channels && s.rate == rate) return;
    s.closeLocked();
    s.channels = channels;
    s.rate = rate;
    if (channels == 0 || channels > 8 || rate == 0) return;
    WAVEFORMATEX f{};
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = static_cast<WORD>(channels);
    f.nSamplesPerSec = rate;
    f.wBitsPerSample = 16;
    f.nBlockAlign = static_cast<WORD>(channels * 2);
    f.nAvgBytesPerSec = rate * f.nBlockAlign;
    if (waveOutOpen(&s.out, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        s.out = nullptr;
        return;
    }
    waveOutPause(s.out);  // play() restarts it once a small cushion is queued
    s.priming = true;
}

void AudioOut::play(const std::int16_t* samples, std::size_t frames) {
    Impl& s = *impl_;
    std::lock_guard lock(s.m);
    if (!s.out || frames == 0) return;
    const int queued = s.queued();
    if (queued >= kMaxQueued) return;  // behind: skip rather than drift further from the picture
    if (queued == 0 && !s.priming) {   // ran dry: pause and refill a small cushion first
        waveOutPause(s.out);
        s.priming = true;
    }
    WAVEHDR& h = s.hdr[s.next];
    if ((h.dwFlags & WHDR_PREPARED) && !(h.dwFlags & WHDR_DONE)) return;  // ring full (cannot happen)
    if (h.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(s.out, &h, sizeof(h));
    std::vector<std::int16_t>& buf = s.data[s.next];
    buf.assign(samples, samples + frames * s.channels);
    h = WAVEHDR{};
    h.lpData = reinterpret_cast<LPSTR>(buf.data());
    h.dwBufferLength = static_cast<DWORD>(buf.size() * sizeof(std::int16_t));
    if (waveOutPrepareHeader(s.out, &h, sizeof(h)) != MMSYSERR_NOERROR) return;
    if (waveOutWrite(s.out, &h, sizeof(h)) != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(s.out, &h, sizeof(h));
        h = WAVEHDR{};
        return;
    }
    s.next = (s.next + 1) % kBlocks;
    if (s.priming && queued + 1 >= kPrime) {
        waveOutRestart(s.out);
        s.priming = false;
    }
}
