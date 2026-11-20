// SPDX-License-Identifier: AGPL-3.0-only
#include "core/recorder.h"

#include <cstdio>

namespace {
constexpr double kPadStickIntervalMs = 50.0;
}

bool h264HasNal(std::span<const std::uint8_t> d, int nalType) {
    for (size_t i = 0; i + 3 < d.size(); ++i) {
        if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
            if ((d[i + 3] & 0x1F) == nalType) return true;
            i += 2;
        }
    }
    return false;
}

bool Recorder::start(const std::filesystem::path& dir, std::string* errorRu) {
    std::lock_guard lock(m_);
    if (recording_) return true;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    video_.open(dir / "video.h264", std::ios::binary | std::ios::trunc);
    frames_.open(dir / "frames.csv", std::ios::binary | std::ios::trunc);
    events_.open(dir / "events.jsonl", std::ios::binary | std::ios::trunc);
    if (ec || !video_ || !frames_ || !events_) {
        video_.close();
        frames_.close();
        events_.close();
        if (errorRu) *errorRu = "Не удалось создать файлы записи в выбранной папке.";
        return false;
    }
    frames_ << "frame,t_ms,bytes,keyframe\n";
    dir_ = dir;
    stats_ = {};
    gotKeyframe_ = false;
    lastPad_ = PadState{};
    lastPadT_ = -1e18;
    recording_ = true;
    return true;
}

void Recorder::stop() {
    std::lock_guard lock(m_);
    if (!recording_) return;
    recording_ = false;
    video_.close();
    frames_.close();
    events_.close();
}

bool Recorder::recording() const {
    std::lock_guard lock(m_);
    return recording_;
}

Recorder::Stats Recorder::stats() const {
    std::lock_guard lock(m_);
    return stats_;
}

std::filesystem::path Recorder::directory() const {
    std::lock_guard lock(m_);
    return dir_;
}

void Recorder::onVideoSample(double tMs, std::span<const std::uint8_t> data) {
    std::lock_guard lock(m_);
    const bool hasSps = h264HasNal(data, 7);
    const bool hasIdr = h264HasNal(data, 5);
    if (hasSps && !hasIdr && !h264HasNal(data, 1)) {  // stream header only
        header_.assign(data.begin(), data.end());
        if (!recording_ || !gotKeyframe_) return;
    }
    if (!recording_) return;
    if (!gotKeyframe_) {
        if (!hasIdr) {
            ++stats_.framesSkipped;
            return;
        }
        if (!hasSps && !header_.empty()) {  // make the file decodable on its own
            video_.write(reinterpret_cast<const char*>(header_.data()), static_cast<std::streamsize>(header_.size()));
            stats_.bytesWritten += header_.size();
        }
        gotKeyframe_ = true;
    }
    video_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    char row[96];
    std::snprintf(row, sizeof(row), "%llu,%.1f,%zu,%d\n", static_cast<unsigned long long>(stats_.framesWritten), tMs,
                  data.size(), hasIdr ? 1 : 0);
    frames_ << row;
    ++stats_.framesWritten;
    stats_.bytesWritten += data.size();
}

void Recorder::writeEvent(double tMs, std::string_view type, std::string_view jsonData) {
    char head[64];
    std::snprintf(head, sizeof(head), "{\"t\":%.1f,\"type\":\"", tMs);
    events_ << head << type << "\",\"data\":" << jsonData << "}\n";
    ++stats_.events;
}

void Recorder::onEvent(double tMs, std::string_view type, std::string_view jsonData) {
    std::lock_guard lock(m_);
    if (recording_) writeEvent(tMs, type, jsonData);
}

void Recorder::onPad(double tMs, const PadState& pad) {
    std::lock_guard lock(m_);
    if (!recording_ || pad == lastPad_) return;
    const bool discrete = pad.buttons != lastPad_.buttons || pad.l2 != lastPad_.l2 || pad.r2 != lastPad_.r2;
    if (!discrete && tMs - lastPadT_ < kPadStickIntervalMs) return;
    char data[192];
    std::snprintf(data, sizeof(data),
                  "{\"buttons\":%u,\"l2\":%u,\"r2\":%u,\"lx\":%.3f,\"ly\":%.3f,\"rx\":%.3f,\"ry\":%.3f}", pad.buttons,
                  pad.l2, pad.r2, pad.left.x, pad.left.y, pad.right.x, pad.right.y);
    writeEvent(tMs, "pad", data);
    lastPad_ = pad;
    lastPadT_ = tMs;
}
