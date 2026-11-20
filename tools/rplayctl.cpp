// SPDX-License-Identifier: AGPL-3.0-only
// rplayctl — console tool to find, pair and poke a PS5 over input-only Remote Play.
#include <windows.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#include "core/account_id.h"
#include "core/bindings.h"
#include "ps5/link.h"
#include "ps5/pairing_store.h"

namespace {

std::filesystem::path pairingFile() { return appDataDir() / L"pairing.bin"; }

int usage() {
    std::puts("Использование:\n"
              "  rplayctl discover                         найти PS5 в сети\n"
              "  rplayctl pair <ip> <account-id> <pin>     привязать PS5 (PIN: Настройки → Система →\n"
              "                                            Дистанционное воспроизведение → Привязать устройство)\n"
              "  rplayctl press <кнопка> [мс=150]          нажать кнопку: cross, circle, square, triangle, ps,\n"
              "                                            options, dpad_up …\n"
              "  rplayctl stick <x> <y> <мс>               держать правый стик (x, y от -1 до 1)");
    return 2;
}

// Connects with the saved pairing, runs body while connected, then disconnects.
int withConnection(const std::function<void(Ps5Link&)>& body) {
    auto pairing = loadPairing(pairingFile());
    if (!pairing) {
        std::puts("PS5 не привязана. Сначала: rplayctl pair <ip> <account-id> <pin>");
        return 1;
    }
    std::mutex m;
    std::condition_variable cv;
    LinkState last = LinkState::Idle;
    bool changed = false;
    Ps5Link link([&](LinkState s, const std::string& msg) {
        std::printf("%s\n", msg.c_str());
        std::lock_guard lock(m);
        last = s;
        changed = true;
        cv.notify_all();
    });
    link.connect(*pairing);
    {
        std::unique_lock lock(m);
        cv.wait(lock, [&] { return changed && (last == LinkState::Connected || last == LinkState::Lost ||
                                               last == LinkState::Idle); });
        if (last != LinkState::Connected) return 1;
    }
    if (auto updated = link.currentPairing(); updated && updated->host != pairing->host)
        savePairing(*updated, pairingFile());  // DHCP moved the console
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    body(link);
    const bool lost = link.state() == LinkState::Lost;
    link.disconnect();
    return lost ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) return usage();
    const std::string cmd = argv[1];

    if (cmd == "discover") {
        Ps5Link link(nullptr);
        auto found = link.discover(std::chrono::milliseconds(2500));
        if (found.empty()) {
            std::puts("PS5 не найдена. Она включена и в той же сети? VPN выключен?");
            return 1;
        }
        for (const auto& c : found)
            std::printf("%s  %s  %s\n", c.host.c_str(), c.name.c_str(), c.ready ? "готова" : "спит");
        return 0;
    }

    if (cmd == "pair" && argc == 5) {
        std::string error;
        auto account = parseAccountId(argv[3], &error);
        if (!account) {
            std::printf("%s\n", error.c_str());
            return 1;
        }
        const std::string pinText = argv[4];
        if (pinText.size() != 8 || pinText.find_first_not_of("0123456789") != std::string::npos) {
            std::puts("PIN — это 8 цифр с экрана PS5.");
            return 1;
        }
        Ps5Link link(nullptr);
        std::puts("Привязываю…");
        auto pairing = link.pair(argv[2], *account, static_cast<std::uint32_t>(std::stoul(pinText)), &error);
        if (!pairing) {
            std::printf("%s\n", error.c_str());
            return 1;
        }
        if (!savePairing(*pairing, pairingFile())) {
            std::puts("Привязка прошла, но сохранить её не удалось.");
            return 1;
        }
        std::printf("Готово: %s (%s). Сохранено в %s\n", pairing->name.c_str(), pairing->host.c_str(),
                    pairingFile().string().c_str());
        return 0;
    }

    if (cmd == "press" && (argc == 3 || argc == 4)) {
        const auto action = parseAction(argv[2]);
        if (!action || static_cast<size_t>(*action) >= static_cast<size_t>(PadButton::Count)) {
            std::printf("Неизвестная кнопка «%s».\n", argv[2]);
            return usage();
        }
        const int ms = argc == 4 ? std::atoi(argv[3]) : 150;
        return withConnection([&](Ps5Link& link) {
            PadState pad;
            pad.set(static_cast<PadButton>(*action), true);
            link.submit(pad);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            link.submit(PadState{});
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::puts("Нажато.");
        });
    }

    if (cmd == "stick" && argc == 5) {
        PadState pad;
        pad.right = {std::strtof(argv[2], nullptr), std::strtof(argv[3], nullptr)};
        const int ms = std::atoi(argv[4]);
        return withConnection([&](Ps5Link& link) {
            const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
            while (std::chrono::steady_clock::now() < end && link.state() == LinkState::Connected) {
                link.submit(pad);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            link.submit(PadState{});
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        });
    }

    return usage();
}
