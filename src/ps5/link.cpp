// SPDX-License-Identifier: AGPL-3.0-only
// Based on PS5-DirectView (AGPL-3.0), https://github.com/babymonie/PS5-DirectView
// (src/RemotePlayClient.cpp: discovery, pairing, wake-up and input-only session).
#include "ps5/link.h"
#include "ps5/liveness.h"
#include "core/quality.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

extern "C" {
#include <chiaki/common.h>
#include <chiaki/discovery.h>
#include <chiaki/discoveryservice.h>
#include <chiaki/log.h>
#include <chiaki/opusdecoder.h>
#include <chiaki/regist.h>
#include <chiaki/session.h>
}

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

std::int16_t stickAxis(float v) {
    const float c = std::clamp(v, -1.0f, 1.0f);
    return static_cast<std::int16_t>(std::lround(c * 32767.0f));
}

std::string quitText(ChiakiQuitReason reason, const std::string& remote) {
    std::string text;
    switch (reason) {
        case CHIAKI_QUIT_REASON_STOPPED: text = "Отключено."; break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_CONNECTION_REFUSED:
            text = "PS5 отказала в подключении. Проверьте: Настройки → Система → Дистанционное воспроизведение → Включить.";
            break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE:
            text = "Дистанционное воспроизведение уже занято. Закройте PS Remote Play и chiaki на других устройствах.";
            break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_CRASH:
            text = "Сбой дистанционного воспроизведения на PS5. Подождите несколько секунд и подключитесь снова.";
            break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_VERSION_MISMATCH:
            text = "Прошивка PS5 использует версию протокола, которую встроенное ядро chiaki-ng не знает. Нужно обновить ядро.";
            break;
        case CHIAKI_QUIT_REASON_SESSION_REQUEST_UNKNOWN:
            text = "PS5 отклонила сеанс. Если вы заново привязывали устройства на PS5, привяжите RPlay K&M ещё раз.";
            break;
        case CHIAKI_QUIT_REASON_CTRL_CONNECT_FAILED:
        case CHIAKI_QUIT_REASON_CTRL_CONNECTION_REFUSED:
            text = "Не удалось подключиться к PS5 (TCP 9295). PS5 включена и в этой сети?";
            break;
        case CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_DISCONNECTED:
            text = "PS5 завершила сеанс.";
            break;
        case CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_SHUTDOWN:
            text = "PS5 выключается или уходит в режим покоя.";
            break;
        case CHIAKI_QUIT_REASON_STREAM_CONNECTION_UNKNOWN:
            text = "Связь с PS5 потеряна (UDP 9296). Проверьте сеть и брандмауэр.";
            break;
        default: text = std::string("Связь с PS5 потеряна: ") + chiaki_quit_reason_string(reason); break;
    }
    if (!remote.empty()) text += " (" + remote + ")";
    return text;
}

std::vector<sockaddr_storage> directedBroadcasts() {
    std::vector<sockaddr_storage> out;
    ULONG size = 16 * 1024;
    std::vector<std::uint8_t> buffer(size);
    auto* list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG r = GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size);
    if (r == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        r = GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size);
    }
    if (r != NO_ERROR) return out;
    for (auto* a = list; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
            const UINT8 prefix = u->OnLinkPrefixLength;
            if (prefix == 0 || prefix >= 31) continue;
            sockaddr_in in = *reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            const std::uint32_t ip = ntohl(in.sin_addr.s_addr);
            const std::uint32_t mask = ~((1u << (32 - prefix)) - 1u);
            in.sin_addr.s_addr = htonl(ip | ~mask);
            in.sin_port = 0;
            sockaddr_storage ss{};
            std::memcpy(&ss, &in, sizeof(in));
            out.push_back(ss);
        }
    }
    return out;
}

// Wake-on-LAN "user credential": the regist key's hex digits.
std::uint64_t wakeCredential(const Pairing& p) {
    const char* key = reinterpret_cast<const char*>(p.registKey.data());
    std::string hex(key, strnlen(key, p.registKey.size()));
    if (hex.empty() || hex.size() > 16) return 0;
    return std::strtoull(hex.c_str(), nullptr, 16);
}

void logCb(ChiakiLogLevel level, const char* msg, void*) {
    std::string line = level == CHIAKI_LOG_ERROR ? "[chiaki] E " : level == CHIAKI_LOG_WARNING ? "[chiaki] W " : "[chiaki] I ";
    line += msg;
    line += "\n";
    OutputDebugStringA(line.c_str());
}

}  // namespace

ChiakiControllerState padToChiaki(const PadState& pad) {
    static constexpr std::uint32_t kMap[static_cast<size_t>(PadButton::Count)] = {
        CHIAKI_CONTROLLER_BUTTON_CROSS,    CHIAKI_CONTROLLER_BUTTON_MOON,
        CHIAKI_CONTROLLER_BUTTON_BOX,      CHIAKI_CONTROLLER_BUTTON_PYRAMID,
        CHIAKI_CONTROLLER_BUTTON_DPAD_UP,  CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN,
        CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT, CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT,
        CHIAKI_CONTROLLER_BUTTON_L1,       CHIAKI_CONTROLLER_BUTTON_R1,
        CHIAKI_CONTROLLER_BUTTON_L3,       CHIAKI_CONTROLLER_BUTTON_R3,
        CHIAKI_CONTROLLER_BUTTON_OPTIONS,  CHIAKI_CONTROLLER_BUTTON_SHARE,
        CHIAKI_CONTROLLER_BUTTON_TOUCHPAD, CHIAKI_CONTROLLER_BUTTON_PS,
    };
    ChiakiControllerState s;
    chiaki_controller_state_set_idle(&s);
    for (size_t i = 0; i < static_cast<size_t>(PadButton::Count); ++i)
        if (pad.pressed(static_cast<PadButton>(i))) s.buttons |= kMap[i];
    s.l2_state = pad.l2;
    s.r2_state = pad.r2;
    s.left_x = stickAxis(pad.left.x);
    s.left_y = stickAxis(pad.left.y);
    s.right_x = stickAxis(pad.right.x);
    s.right_y = stickAxis(pad.right.y);
    return s;
}

struct Ps5Link::Impl {
    StateFn onState;
    Feedback feedback;  // set before connect()
    mutable std::mutex qualityMutex;
    QualityMeter quality;  // ping / jitter / loss while connected
    ChiakiLog log{};
    bool libReady = false;

    mutable std::mutex m;  // guards everything below except the pad path
    std::condition_variable cv;
    bool stopping = false;
    bool wantConnected = false;
    bool evConnected = false, evQuit = false, evLoginPin = false;
    ChiakiQuitReason quitReason = CHIAKI_QUIT_REASON_NONE;
    std::string quitRemote;
    std::optional<Pairing> pairing;
    std::thread worker;
    std::atomic<LinkState> state{LinkState::Idle};

    std::mutex padMutex;
    ChiakiSession* activeSession = nullptr;  // non-null only while connected

    void setState(LinkState s, const std::string& message) {
        state = s;
        if (onState) onState(s, message);
    }

    bool keepGoing() {
        std::lock_guard lock(m);
        return wantConnected && !stopping;
    }

    // ------------------------------------------------------------- discovery
    struct ProbeCtx {
        std::mutex m;
        std::vector<FoundConsole> found;
        std::vector<bool> ps5;
        bool replied = false;
        std::chrono::steady_clock::time_point firstReply;
    };

    static void discoveryCb(ChiakiDiscoveryHost* hosts, size_t count, void* user) {
        auto* ctx = static_cast<ProbeCtx*>(user);
        std::lock_guard lock(ctx->m);
        if (count > 0 && !ctx->replied) {
            ctx->replied = true;
            ctx->firstReply = std::chrono::steady_clock::now();
        }
        ctx->found.clear();
        ctx->ps5.clear();
        for (size_t i = 0; i < count; ++i) {
            const ChiakiDiscoveryHost& h = hosts[i];
            FoundConsole c;
            c.host = h.host_addr ? h.host_addr : "";
            c.name = h.host_name ? h.host_name : "";
            c.id = h.host_id ? h.host_id : "";
            c.ready = h.state == CHIAKI_DISCOVERY_HOST_STATE_READY;
            ctx->found.push_back(std::move(c));
            ctx->ps5.push_back(chiaki_discovery_host_is_ps5(const_cast<ChiakiDiscoveryHost*>(&h)));
        }
    }

    // Unicast to host (or broadcast on every local subnet when empty); collects PS5 replies
    // until done() says so, the timeout passes, or (forConnect) the user cancels.
    std::vector<FoundConsole> probe(const std::string& host, int timeoutMs,
                                    const std::function<bool(const std::vector<FoundConsole>&)>& done,
                                    bool forConnect, double* rttMs = nullptr) {
        ProbeCtx ctx;
        sockaddr_storage send{};
        auto* in = reinterpret_cast<sockaddr_in*>(&send);
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = INADDR_BROADCAST;
        std::vector<sockaddr_storage> broadcasts;
        ChiakiDiscoveryServiceOptions options{};
        options.hosts_max = 16;
        options.host_drop_pings = 3;
        options.ping_ms = 500;
        options.ping_initial_ms = 0;
        options.send_addr = &send;
        options.send_addr_size = sizeof(sockaddr_in);
        options.cb = &Impl::discoveryCb;
        options.cb_user = &ctx;
        std::string hostCopy = host;
        if (!host.empty()) {
            options.send_host = hostCopy.data();
        } else {
            broadcasts = directedBroadcasts();
            options.broadcast_addrs = broadcasts.empty() ? nullptr : broadcasts.data();
            options.broadcast_num = broadcasts.size();
        }
        ChiakiDiscoveryService service;
        if (chiaki_discovery_service_init(&service, &options, &log) != CHIAKI_ERR_SUCCESS) return {};
        const auto sent = std::chrono::steady_clock::now();  // first ping goes out at once (ping_initial_ms = 0)
        auto ps5Only = [&] {
            std::vector<FoundConsole> out;
            std::lock_guard lock(ctx.m);
            for (size_t i = 0; i < ctx.found.size(); ++i)
                if (ctx.ps5[i]) out.push_back(ctx.found[i]);
            return out;
        };
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (done && done(ps5Only())) break;
            {
                std::lock_guard lock(m);
                if (stopping || (forConnect && !wantConnected)) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        chiaki_discovery_service_fini(&service);
        if (rttMs) {
            std::lock_guard lock(ctx.m);
            if (ctx.replied) *rttMs = std::chrono::duration<double, std::milli>(ctx.firstReply - sent).count();
        }
        return ps5Only();
    }

    std::optional<FoundConsole> locate(const Pairing& p) {
        auto matches = [&](const FoundConsole& c) { return p.id.empty() || c.id == p.id; };
        auto hasMatch = [&](const std::vector<FoundConsole>& v) { return std::any_of(v.begin(), v.end(), matches); };
        std::vector<std::string> attempts;
        if (!p.host.empty()) attempts.push_back(p.host);
        attempts.emplace_back();  // then every local subnet (DHCP may have moved it)
        for (const std::string& host : attempts) {
            if (!keepGoing()) break;
            for (auto& c : probe(host, host.empty() ? 3000 : 2000, hasMatch, true))
                if (matches(c)) return c;
        }
        return std::nullopt;
    }

    // ------------------------------------------------------------- session
    static void audioFormatCb(uint32_t channels, uint32_t rate, void* user) {
        auto* self = static_cast<Impl*>(user);
        if (self->feedback.onAudioFormat) self->feedback.onAudioFormat(channels, rate);
    }

    static void audioFrameCb(int16_t* buf, size_t frames, void* user) {
        auto* self = static_cast<Impl*>(user);
        if (self->feedback.onAudio) self->feedback.onAudio(buf, frames);
    }

    static bool videoCb(uint8_t* buf, size_t size, int32_t framesLost, bool, void* user) {
        auto* q = static_cast<Impl*>(user);
        {
            std::lock_guard qlock(q->qualityMutex);
            q->quality.addFrames(1, framesLost > 0 ? static_cast<std::uint64_t>(framesLost) : 0);
        }
        auto* self = static_cast<Impl*>(user);
        if (self->feedback.onVideo) self->feedback.onVideo(buf, size);
        return true;
    }

    static void eventCb(ChiakiEvent* e, void* user) {
        auto* self = static_cast<Impl*>(user);
        if (self->feedback.onEvent) {  // game feedback: forwarded outside the lock
            char json[256];
            if (e->type == CHIAKI_EVENT_RUMBLE) {
                std::snprintf(json, sizeof(json), "{\"left\":%u,\"right\":%u}", e->rumble.left, e->rumble.right);
                self->feedback.onEvent("rumble", json);
                return;
            }
            if (e->type == CHIAKI_EVENT_TRIGGER_EFFECTS) {
                const auto& t = e->trigger_effects;
                std::string s = "{\"type_left\":" + std::to_string(t.type_left) +
                                ",\"type_right\":" + std::to_string(t.type_right) + ",\"left\":[";
                for (int i = 0; i < 10; ++i) s += (i ? "," : "") + std::to_string(t.left[i]);
                s += "],\"right\":[";
                for (int i = 0; i < 10; ++i) s += (i ? "," : "") + std::to_string(t.right[i]);
                s += "]}";
                self->feedback.onEvent("trigger_effects", s);
                return;
            }
        }
        std::lock_guard lock(self->m);
        switch (e->type) {
            case CHIAKI_EVENT_CONNECTED: self->evConnected = true; break;
            case CHIAKI_EVENT_QUIT:
                self->evQuit = true;
                self->quitReason = e->quit.reason;
                self->quitRemote = e->quit.reason_str ? e->quit.reason_str : "";
                break;
            case CHIAKI_EVENT_LOGIN_PIN_REQUEST: self->evLoginPin = true; break;
            default: break;  // rumble, LED, trigger effects: no physical pad
        }
        self->cv.notify_all();
    }

    void sendNeutral() {
        std::lock_guard lock(padMutex);
        if (!activeSession) return;
        ChiakiControllerState idle;
        chiaki_controller_state_set_idle(&idle);
        chiaki_session_set_controller_state(activeSession, &idle);
    }

    void run(Pairing p) {
        setState(LinkState::Searching, "Ищу " + (p.name.empty() ? std::string("PS5") : p.name) + "…");
        auto console = locate(p);
        if (!keepGoing()) return finish(LinkState::Idle, "Отключено.");
        if (!console)
            return finish(LinkState::Lost,
                          "PS5 не найдена в сети. Она включена (или в режиме покоя с доступом к сети) и в той же сети?");
        if (console->host != p.host) {  // DHCP moved it
            p.host = console->host;
            std::lock_guard lock(m);
            pairing = p;
        }

        if (!console->ready) {
            setState(LinkState::Waking, "Бужу PS5…");
            if (chiaki_discovery_wakeup(&log, nullptr, p.host.c_str(), wakeCredential(p), true) != CHIAKI_ERR_SUCCESS)
                return finish(LinkState::Lost, "Не удалось отправить сигнал пробуждения.");
            auto ready = probe(p.host, 45000, [](const std::vector<FoundConsole>& v) { return !v.empty() && v.front().ready; },
                               true);
            if (!keepGoing()) return finish(LinkState::Idle, "Отключено.");
            if (ready.empty() || !ready.front().ready)
                return finish(LinkState::Lost,
                              "PS5 не проснулась. Включите: Настройки → Система → Энергосбережение → Функции в режиме "
                              "покоя → Разрешить включение PS5 по сети.");
        }

        setState(LinkState::Connecting, "Подключаюсь (только управление)…");
        ChiakiConnectInfo info{};
        info.ps5 = true;
        info.host = p.host.c_str();
        static_assert(sizeof(info.regist_key) == 16 && sizeof(info.morning) == 16);
        std::memcpy(info.regist_key, p.registKey.data(), sizeof(info.regist_key));
        std::memcpy(info.morning, p.morning.data(), sizeof(info.morning));
        chiaki_connect_video_profile_preset(&info.video_profile, CHIAKI_VIDEO_RESOLUTION_PRESET_360p,
                                            CHIAKI_VIDEO_FPS_PRESET_30);
        info.video_profile.codec = CHIAKI_CODEC_H264;
        info.video_profile_auto_downgrade = true;
        // Keep 360p video (recorder / vehicle detector). While Remote Play runs the PS5 mutes its own
        // outputs (TV, controller headset), so the sound comes here and is played on the PC.
        const bool wantAudio = static_cast<bool>(feedback.onAudio);
        info.audio_video_disabled = wantAudio ? CHIAKI_NONE_DISABLED : CHIAKI_AUDIO_DISABLED;
        info.enable_keyboard = false;
        info.enable_dualsense = true;  // the game then sends adaptive-trigger effects
        info.auto_regist = false;
        info.holepunch_session = nullptr;  // LAN only
        info.rudp_sock = nullptr;
        info.packet_loss_max = 0.05;
        info.enable_idr_on_fec_failure = false;

        struct OpusGuard {  // outlives the session that feeds it
            ChiakiOpusDecoder d;
            ~OpusGuard() { chiaki_opus_decoder_fini(&d); }
        } opus;
        chiaki_opus_decoder_init(&opus.d, &log);
        chiaki_opus_decoder_set_cb(&opus.d, &Impl::audioFormatCb, &Impl::audioFrameCb, this);
        auto session = std::make_unique<ChiakiSession>();
        std::memset(session.get(), 0, sizeof(ChiakiSession));
        {
            std::lock_guard lock(m);
            evConnected = evQuit = evLoginPin = false;
            std::lock_guard qlock(qualityMutex);
            quality.reset();
            quitReason = CHIAKI_QUIT_REASON_NONE;
            quitRemote.clear();
        }
        ChiakiErrorCode err = chiaki_session_init(session.get(), &info, &log);
        if (err != CHIAKI_ERR_SUCCESS)
            return finish(LinkState::Lost, std::string("Ошибка запуска Remote Play: ") + chiaki_error_string(err));
        chiaki_session_set_event_cb(session.get(), &Impl::eventCb, this);
        chiaki_session_set_video_sample_cb(session.get(), &Impl::videoCb, this);
        if (wantAudio) {
            ChiakiAudioSink sink;
            chiaki_opus_decoder_get_sink(&opus.d, &sink);
            chiaki_session_set_audio_sink(session.get(), &sink);
        }
        err = chiaki_session_start(session.get());
        if (err != CHIAKI_ERR_SUCCESS) {
            chiaki_session_fini(session.get());
            return finish(LinkState::Lost, std::string("Ошибка запуска Remote Play: ") + chiaki_error_string(err));
        }

        bool connected = false, userStop = false, silentDrop = false, wentToSleep = false;
        {
            std::unique_lock lock(m);
            cv.wait_for(lock, std::chrono::seconds(40),
                        [&] { return stopping || evConnected || evQuit || evLoginPin || !wantConnected; });
            connected = evConnected && !evQuit;
            userStop = stopping || !wantConnected;
        }
        if (connected && !userStop) {
            {
                std::lock_guard lock(padMutex);
                activeSession = session.get();
            }
            sendNeutral();  // never resume buttons held before a reconnect
            setState(LinkState::Connected, "Подключено к " + (p.name.empty() ? std::string("PS5") : p.name) + ".");
            // chiaki-ng never notices a silently dead peer on the LAN, so probe the console.
            Liveness live;
            for (;;) {
                {
                    std::unique_lock lock(m);
                    if (cv.wait_for(lock, std::chrono::seconds(2),
                                    [&] { return stopping || evQuit || evLoginPin || !wantConnected; }))
                        break;
                }
                double pingMs = -1;
                const auto seen = probe(p.host, 1000, [](const std::vector<FoundConsole>& v) { return !v.empty(); }, true, &pingMs);
                {
                    std::lock_guard qlock(qualityMutex);
                    if (seen.empty() || pingMs < 0) quality.addMiss(); else quality.addPing(pingMs);
                }
                if (seen.empty())
                    live.missed();
                else
                    live.replied(seen.front().ready);
                if (live.dead()) {
                    silentDrop = true;
                    wentToSleep = !seen.empty();
                    break;
                }
            }
            std::lock_guard lock(m);
            userStop = stopping || !wantConnected;
        }

        // Teardown: release everything, then stop. Never leave a button held.
        sendNeutral();
        {
            std::lock_guard lock(padMutex);
            activeSession = nullptr;
        }
        bool quit, loginPin, timedOut;
        ChiakiQuitReason reason;
        std::string remote;
        {
            std::lock_guard lock(m);
            quit = evQuit;
            loginPin = evLoginPin;
            timedOut = !evConnected && !evQuit && !evLoginPin && !userStop;
            reason = quitReason;
            remote = quitRemote;
        }
        if (!quit) std::this_thread::sleep_for(std::chrono::milliseconds(60));  // let the release go out
        chiaki_session_stop(session.get());
        chiaki_session_join(session.get());
        chiaki_session_fini(session.get());

        if (userStop) return finish(LinkState::Idle, "Отключено.");
        if (silentDrop && !quit)
            return finish(LinkState::Lost, wentToSleep ? "PS5 ушла в режим покоя."
                                                       : "PS5 перестала отвечать (сеть или питание). Нажмите F8, "
                                                         "чтобы подключиться снова.");
        if (loginPin)
            return finish(LinkState::Lost,
                          "У пользователя PS5 включён код входа. Отключите его: Настройки → Пользователи и учётные "
                          "записи → Настройки входа.");
        if (timedOut) return finish(LinkState::Lost, "PS5 не ответила на запрос Remote Play вовремя.");
        finish(LinkState::Lost, quitText(reason, remote));
    }

    void finish(LinkState s, const std::string& message) {
        {
            std::lock_guard lock(m);
            wantConnected = false;
        }
        setState(s, message);
    }
};

Ps5Link::Ps5Link(StateFn onState) : impl_(std::make_unique<Impl>()) {
    impl_->onState = std::move(onState);
    chiaki_log_init(&impl_->log, CHIAKI_LOG_ERROR | CHIAKI_LOG_WARNING, &logCb, nullptr);
    impl_->libReady = chiaki_lib_init() == CHIAKI_ERR_SUCCESS;
}

Ps5Link::~Ps5Link() {
    {
        std::lock_guard lock(impl_->m);
        impl_->stopping = true;
    }
    disconnect();
}

std::vector<FoundConsole> Ps5Link::discover(std::chrono::milliseconds timeout) {
    if (!impl_->libReady) return {};
    return impl_->probe({}, static_cast<int>(timeout.count()), nullptr, false);
}

std::optional<Pairing> Ps5Link::pair(const std::string& host, const AccountId& account, std::uint32_t pin,
                                     std::string* errorRu) {
    auto fail = [&](const char* text) -> std::optional<Pairing> {
        if (errorRu) *errorRu = text;
        return std::nullopt;
    };
    if (!impl_->libReady) return fail("Не удалось запустить сетевую часть (Winsock).");
    if (host.empty()) return fail("Укажите IP-адрес PS5.");
    if (pin > 99999999) return fail("PIN — это 8 цифр с экрана PS5.");

    auto found = impl_->probe(host, 2500, [](const std::vector<FoundConsole>& v) { return !v.empty(); }, false);
    if (found.empty()) return fail("PS5 не ответила по этому адресу. Проверьте IP и что PS5 включена.");
    const FoundConsole console = found.front();
    if (!console.ready) return fail("PS5 в режиме покоя. Включите её полностью, чтобы привязать.");

    ChiakiRegistInfo info{};
    info.target = CHIAKI_TARGET_PS5_1;
    info.host = host.c_str();
    info.broadcast = false;
    info.psn_online_id = nullptr;
    static_assert(sizeof(info.psn_account_id) == 8);
    std::memcpy(info.psn_account_id, account.data(), account.size());
    info.pin = pin;
    info.console_pin = 0;
    info.holepunch_info = nullptr;
    info.rudp = nullptr;

    struct RegistCtx {
        std::mutex m;
        std::condition_variable cv;
        bool done = false;
        ChiakiRegistEventType type = CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED;
        ChiakiRegisteredHost host{};
    } ctx;
    auto cb = [](ChiakiRegistEvent* e, void* user) {
        auto* c = static_cast<RegistCtx*>(user);
        std::lock_guard lock(c->m);
        c->type = e->type;
        if (e->type == CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS && e->registered_host) c->host = *e->registered_host;
        c->done = true;
        c->cv.notify_all();
    };
    ChiakiRegist regist;
    if (chiaki_regist_start(&regist, &impl_->log, &info, cb, &ctx) != CHIAKI_ERR_SUCCESS)
        return fail("Не удалось начать привязку.");
    bool done;
    {
        std::unique_lock lock(ctx.m);
        ctx.cv.wait_for(lock, std::chrono::seconds(30), [&] { return ctx.done; });
        done = ctx.done;
    }
    if (!done) chiaki_regist_stop(&regist);
    chiaki_regist_fini(&regist);
    if (!done || ctx.type != CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS)
        return fail("Привязка не удалась. Проверьте PIN (он действует ~5 минут), account ID и что "
                    "дистанционное воспроизведение включено на PS5.");

    Pairing p;
    p.target = static_cast<std::uint32_t>(ctx.host.target ? ctx.host.target : CHIAKI_TARGET_PS5_1);
    static_assert(sizeof(ctx.host.rp_regist_key) == 16 && sizeof(ctx.host.rp_key) == 16);
    std::memcpy(p.registKey.data(), ctx.host.rp_regist_key, p.registKey.size());
    std::memcpy(p.morning.data(), ctx.host.rp_key, p.morning.size());
    p.host = host;
    p.name = ctx.host.server_nickname[0]
                 ? std::string(ctx.host.server_nickname, strnlen(ctx.host.server_nickname, sizeof(ctx.host.server_nickname)))
                 : console.name;
    p.id = console.id;
    SecureZeroMemory(&ctx.host, sizeof(ctx.host));
    return p;
}

void Ps5Link::connect(const Pairing& pairing) {
    Impl& d = *impl_;
    if (!d.libReady) {
        d.setState(LinkState::Lost, "Не удалось запустить сетевую часть (Winsock).");
        return;
    }
    {
        std::lock_guard lock(d.m);
        if (d.wantConnected || d.stopping) return;  // already connecting/connected
        d.wantConnected = true;
        d.pairing = pairing;
    }
    if (d.worker.joinable()) d.worker.join();
    d.worker = std::thread([&d, pairing] { d.run(pairing); });
}

void Ps5Link::disconnect() {
    Impl& d = *impl_;
    d.sendNeutral();
    {
        std::lock_guard lock(d.m);
        d.wantConnected = false;
    }
    d.cv.notify_all();
    if (d.worker.joinable()) d.worker.join();
}

void Ps5Link::submit(const PadState& pad) {
    Impl& d = *impl_;
    ChiakiControllerState s = padToChiaki(pad);
    std::lock_guard lock(d.padMutex);
    if (d.activeSession) chiaki_session_set_controller_state(d.activeSession, &s);
}

LinkState Ps5Link::state() const { return impl_->state.load(); }

std::optional<Pairing> Ps5Link::currentPairing() const {
    std::lock_guard lock(impl_->m);
    return impl_->pairing;
}

void Ps5Link::setFeedback(Feedback fb) { impl_->feedback = std::move(fb); }

void Ps5Link::requestKeyframe() {
    std::lock_guard lock(impl_->padMutex);
    if (impl_->activeSession) chiaki_session_request_idr(impl_->activeSession);
}

NetQuality Ps5Link::quality() const {
    std::lock_guard lock(impl_->qualityMutex);
    return impl_->quality.snapshot();
}
