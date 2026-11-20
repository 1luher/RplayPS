// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/recorder.h"

namespace {

using Bytes = std::vector<std::uint8_t>;
const Bytes kSpsPps = {0, 0, 0, 1, 0x67, 0x42, 0x00, 0x1F, 0, 0, 0, 1, 0x68, 0xCE, 0x38, 0x80};
const Bytes kIdr = {0, 0, 0, 1, 0x65, 0x88, 0x84, 0x00};
const Bytes kP = {0, 0, 0, 1, 0x41, 0x9A, 0x02, 0x00};

std::filesystem::path freshDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(dir);
    return dir;
}

std::string readAll(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

std::vector<std::string> lines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string line; std::getline(in, line);)
        if (!line.empty()) out.push_back(line);
    return out;
}

}  // namespace

TEST_CASE("h264_nal_detection") {
    CHECK(h264HasNal(kIdr, 5));
    CHECK_FALSE(h264HasNal(kP, 5));
    CHECK(h264HasNal(kP, 1));
    CHECK(h264HasNal(kSpsPps, 7));
    CHECK(h264HasNal(kSpsPps, 8));
    const Bytes shortStart = {0, 0, 1, 0x65, 0x11};  // 3-byte start code
    CHECK(h264HasNal(shortStart, 5));
    CHECK_FALSE(h264HasNal(Bytes{}, 5));
    CHECK_FALSE(h264HasNal(Bytes{0, 0, 0, 1}, 5));
}

TEST_CASE("recorder_waits_for_keyframe_and_writes_header_first") {
    const auto dir = freshDir("rplay_rec_test1");
    Recorder r;
    r.onVideoSample(0, kSpsPps);  // stream header arrives before recording starts
    r.onVideoSample(1, kIdr);
    REQUIRE(r.start(dir, nullptr));
    r.onVideoSample(10, kP);  // not decodable on its own: skipped
    r.onVideoSample(20, kIdr);
    r.onVideoSample(30, kP);
    r.stop();
    const std::string video = readAll(dir / "video.h264");
    Bytes expected = kSpsPps;
    expected.insert(expected.end(), kIdr.begin(), kIdr.end());
    expected.insert(expected.end(), kP.begin(), kP.end());
    CHECK(video == std::string(expected.begin(), expected.end()));
    const auto rows = lines(readAll(dir / "frames.csv"));
    REQUIRE(rows.size() == 3);  // header row + 2 frames
    CHECK(rows[0] == "frame,t_ms,bytes,keyframe");
    CHECK(rows[1].rfind("0,20.0,", 0) == 0);
    CHECK(rows[1].back() == '1');
    CHECK(rows[2].back() == '0');
    CHECK(r.stats().framesSkipped == 1);
    CHECK(r.stats().framesWritten == 2);
}

TEST_CASE("recorder_ignores_samples_when_stopped") {
    const auto dir = freshDir("rplay_rec_test2");
    Recorder r;
    REQUIRE(r.start(dir, nullptr));
    r.onVideoSample(0, kSpsPps);
    r.onVideoSample(1, kIdr);
    r.stop();
    r.onVideoSample(2, kP);
    r.onEvent(3, "rumble", R"({"left":1})");
    CHECK(r.stats().framesWritten == 1);
    CHECK(lines(readAll(dir / "events.jsonl")).empty());
}

TEST_CASE("recorder_events_are_json_lines") {
    const auto dir = freshDir("rplay_rec_test3");
    Recorder r;
    REQUIRE(r.start(dir, nullptr));
    r.onEvent(12.5, "rumble", R"({"left":200,"right":10})");
    r.onEvent(13, "trigger_effects", R"({"type_left":2,"type_right":5})");
    r.stop();
    const auto rows = lines(readAll(dir / "events.jsonl"));
    REQUIRE(rows.size() == 2);
    const auto first = nlohmann::json::parse(rows[0]);
    CHECK(first["t"].get<double>() == doctest::Approx(12.5));
    CHECK(first["type"] == "rumble");
    CHECK(first["data"]["left"] == 200);
    CHECK(nlohmann::json::parse(rows[1])["type"] == "trigger_effects");
}

TEST_CASE("recorder_throttles_stick_only_pad_changes") {
    const auto dir = freshDir("rplay_rec_test4");
    Recorder r;
    REQUIRE(r.start(dir, nullptr));
    PadState p;
    p.right = {0.1f, 0};
    r.onPad(0, p);
    p.right = {0.2f, 0};
    r.onPad(10, p);  // stick-only change within 50 ms: dropped
    p.set(PadButton::Cross, true);
    r.onPad(20, p);  // button change: always logged
    p.right = {0.3f, 0};
    r.onPad(80, p);  // stick change after 50 ms: logged
    r.stop();
    const auto rows = lines(readAll(dir / "events.jsonl"));
    REQUIRE(rows.size() == 3);
    const auto btn = nlohmann::json::parse(rows[1]);
    CHECK(btn["type"] == "pad");
    CHECK(btn["data"]["buttons"] == 1);
}

TEST_CASE("recorder_start_fails_cleanly_on_bad_path") {
    Recorder r;
    std::string err;
    CHECK_FALSE(r.start("Z:\\no\\such\\drive\\rec", &err));
    CHECK_FALSE(err.empty());
    CHECK_FALSE(r.recording());
}
