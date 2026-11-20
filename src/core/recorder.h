// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/pad.h"

// True if the Annex-B H.264 buffer contains a NAL unit of the given type (5 = IDR, 7 = SPS, 8 = PPS).
bool h264HasNal(std::span<const std::uint8_t> data, int nalType);

// Records a session for training the vehicle detector:
//   video.h264    raw H.264 stream (starts with SPS/PPS + a keyframe, so ffmpeg can decode it)
//   frames.csv    frame,t_ms,bytes,keyframe
//   events.jsonl  {"t":ms,"type":"rumble|trigger_effects|pad|…","data":{…}} one per line
// Thread-safe: video and events arrive from chiaki threads, start/stop from the UI.
class Recorder {
public:
    struct Stats {
        std::uint64_t framesWritten = 0, framesSkipped = 0, bytesWritten = 0, events = 0;
    };

    bool start(const std::filesystem::path& dir, std::string* errorRu);
    void stop();
    bool recording() const;
    Stats stats() const;
    std::filesystem::path directory() const;

    void onVideoSample(double tMs, std::span<const std::uint8_t> data);
    void onEvent(double tMs, std::string_view type, std::string_view jsonData);
    // Pad changes: buttons/triggers always; stick-only changes at most every 50 ms.
    void onPad(double tMs, const PadState& pad);

private:
    void writeEvent(double tMs, std::string_view type, std::string_view jsonData);

    mutable std::mutex m_;
    bool recording_ = false;
    bool gotKeyframe_ = false;
    std::vector<std::uint8_t> header_;  // latest SPS/PPS, kept even while not recording
    std::filesystem::path dir_;
    std::ofstream video_, frames_, events_;
    Stats stats_;
    PadState lastPad_;
    double lastPadT_ = -1e18;
};
