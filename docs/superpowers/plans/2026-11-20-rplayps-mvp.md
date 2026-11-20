# RplayPS MVP — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Играть в GTA 6 на PS5 клавиатурой и мышью. Windows-программа передаёт нажатия по
Remote Play, видео не принимает. Это этапы 1–2 из [roadmap.md](../../roadmap.md).

**Architecture:**
- Ядро chiaki-ng (только сетевая часть, без видео) отвечает за связь с PS5.
- Чистая C++-библиотека `rplay_core` превращает ввод в состояние геймпада. Она не зависит
  от Windows и покрыта тестами.
- Поток ввода Windows (Raw Input, тик 1 мс) кормит `rplay_core`. Результат уходит на PS5.
- Окно Dear ImGui служит для привязки, настроек и визуализатора. Во время игры обратная
  связь только звуком.

**Tech Stack:** C++20, CMake ≥ 3.24, MSVC (Visual Studio 2022 Build Tools), ядро chiaki-ng
(через `third_party` из PS5-DirectView), Mbed TLS 2.28, Dear ImGui (D3D11), nlohmann/json, doctest.

**Spec:** [docs/architecture.md](../../architecture.md), [docs/best-of.md](../../best-of.md)

## Global Constraints
- Только Windows 10/11 x64. Лицензия AGPL-3.0 для всего репозитория (файл `LICENSE`).
- Все сторонние библиотеки лежат в `third_party/` с закреплёнными версиями. **Во время сборки
  ничего не скачивается.**
- Код, перенесённый из PS5-DirectView (AGPL-3.0), сохраняет авторство: строка
  `// Based on PS5-DirectView (AGPL-3.0), https://github.com/babymonie/PS5-DirectView` в начале файла.
- Данные пользователя хранятся в `%APPDATA%\RplayPS\`: `pairing.bin` (защищён DPAPI) и `profiles\*.json`.
- Только локальная сеть. Интернет-режим Remote Play не используется. Видео и звук отключены
  (`CHIAKI_AUDIO_VIDEO_DISABLED`).
- Клавиша захвата — **F8**. F8 никогда не передаётся на PS5.
- Интерфейс и сообщения пользователю на русском. Идентификаторы в коде на английском.
- Координаты стика: `[-1, 1]`, `+y` вниз.
- Никогда не оставлять кнопку зажатой на PS5. При потере фокуса, снятии захвата, отключении
  или выходе на PS5 отправляется нейтральное состояние.
- Сборка: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`, затем
  `cmake --build build --config Release`. Тесты: `ctest --test-dir build -C Release --output-on-failure`.

## Review Focus
1. **Окно потеряло фокус посреди боя** (Alt+Tab, всплывающее окно Windows, сбой). Ожидается:
   все кнопки отпущены, стики в центре, звучит сигнал. Тест `release_all_gives_neutral_pad`
   в задаче 4, ручная проверка в задаче 7.
2. **Выброс мыши.** Сразу после захвата или от мыши на 8 кГц может прийти огромная
   единичная дельта. Ожидается: она игнорируется, стик не дёргается. Тест
   `huge_single_delta_is_ignored` в задаче 3.
3. **Испорченный или вручную отредактированный профиль.** Ожидается: неизвестная клавиша и
   значения вне диапазона не роняют программу. Значения ограничиваются диапазоном, ошибка
   показывается текстом, при полном сбое берётся профиль по умолчанию. Тесты в задаче 5.
4. **Account ID в неожиданном виде** (пробелы, base64 без `=`, мусор). Ожидается: понятная
   ошибка, без падения. Тесты в задаче 2.
5. **PS5 уснула или пропала сеть посреди игры.** Ожидается: сигнал «связь потеряна», захват
   снят, и повторное F8 переподключает. Ручная проверка в задачах 6 и 8.

## Структура файлов
```
CMakeLists.txt                 корневой: rplay_core, rplay_win, rplay_ps5, rplayps.exe, rplayctl.exe, tests
LICENSE                        AGPL-3.0
third_party/                   chiaki, mbedtls, nanopb, jerasure, gf-complete, shims, patches (из PS5-DirectView)
third_party/imgui/             Dear ImGui v1.91.9
third_party/nlohmann/json.hpp  v3.11.3
third_party/doctest/doctest.h  v2.4.11
src/core/pad.h                 PadButton, Stick, PadState
src/core/account_id.{h,cpp}    разбор PSN account ID
src/core/mouse_stick.{h,cpp}   MouseSettings, MouseStick
src/core/bindings.{h,cpp}      Action, InputCode, Bindings, имена клавиш, PadBuilder
src/core/profile.{h,cpp}       Profile, JSON, defaultGtaProfile()
src/ps5/link.{h,cpp}           Ps5Link — обёртка ядра chiaki-ng
src/ps5/pairing_store.{h,cpp}  Pairing + DPAPI
src/win/input_thread.{h,cpp}   Raw Input, тик 1 мс, захват, защита фокуса
src/win/sound.{h,cpp}          звуковые сигналы
src/app/main.cpp, ui.{h,cpp}   окно ImGui
tools/rplayctl.cpp             консольная утилита
tests/test_*.cpp               тесты doctest (один исполняемый файл rplay_tests)
```

---

### Task 1: Каркас, сторонние библиотеки, сборка

**Files:**
- Create: `CMakeLists.txt`, `LICENSE`, `third_party/**`, `tests/test_main.cpp`, `.gitignore` (дополнить `build/`)

**Interfaces:**
- Produces: CMake-цели:
  - `chiaki_lan` (статическая библиотека ядра — та же, что `third_party/CMakeLists.txt` из PS5-DirectView);
  - `rplay_core` (пока пустая, `src/core/*.cpp`);
  - `rplay_tests` (doctest, зарегистрирован в CTest).

- [ ] **Step 1:** Скопировать `third_party/` из PS5-DirectView, коммит `c109fec`
  (`git clone https://github.com/babymonie/PS5-DirectView` → `git checkout c109fec` →
  скопировать папку целиком вместе с `README.md`, лицензиями и `patches/`). Скачать
  ImGui v1.91.9 (файлы ядра + `backends/imgui_impl_win32.*`, `imgui_impl_dx11.*`),
  nlohmann/json v3.11.3 (`single_include/nlohmann/json.hpp`) и doctest v2.4.11
  (`doctest/doctest.h`) в указанные папки. В `third_party/README.md` дописать строки
  с версиями этих трёх библиотек.
- [ ] **Step 2:** `LICENSE` — полный текст AGPL-3.0 (https://www.gnu.org/licenses/agpl-3.0.txt).
- [ ] **Step 3:** Корневой `CMakeLists.txt`:
  - C++20, `/W4 /permissive- /utf-8`, определения `UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00`;
  - `add_subdirectory(third_party)`;
  - `rplay_core` (STATIC), `rplay_tests` (doctest + `rplay_core`), `enable_testing()` + `add_test`.
- [ ] **Step 4:** `tests/test_main.cpp`: `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` и тест `TEST_CASE("smoke") { CHECK(1 + 1 == 2); }`.
- [ ] **Step 5:** Сборка и тесты (команды из Global Constraints). Ожидается: сборка без
  ошибок, `chiaki_lan` собрана, `100% tests passed`.
- [ ] **Step 6: Commit** — `git commit -m "Каркас: CMake, third_party, тесты"`

---

### Task 2: Состояние геймпада и account ID

**Files:**
- Create: `src/core/pad.h`, `src/core/account_id.h`, `src/core/account_id.cpp`
- Test: `tests/test_account_id.cpp`, `tests/test_pad.cpp`

**Interfaces:**
- Produces:
```cpp
enum class PadButton : std::uint8_t { Cross, Circle, Square, Triangle, DpadUp, DpadDown, DpadLeft,
    DpadRight, L1, R1, L3, R3, Options, Create, Touchpad, PS, Count };
struct Stick { float x = 0, y = 0; bool operator==(const Stick&) const = default; };
struct PadState {
    std::uint32_t buttons = 0; std::uint8_t l2 = 0, r2 = 0; Stick left, right;
    void set(PadButton b, bool on); bool pressed(PadButton b) const;
    bool operator==(const PadState&) const = default; };
using AccountId = std::array<std::uint8_t, 8>;
std::optional<AccountId> parseAccountId(std::string_view text, std::string* error);
std::string accountIdToBase64(const AccountId& id);
```

- [ ] **Step 1: Тесты**
```cpp
TEST_CASE("pad_set_and_pressed") { PadState p; p.set(PadButton::Cross, true);
  CHECK(p.pressed(PadButton::Cross)); CHECK_FALSE(p.pressed(PadButton::Circle));
  p.set(PadButton::Cross, false); CHECK(p == PadState{}); }
TEST_CASE("account_id_decimal_is_little_endian") {
  auto id = parseAccountId("1234567890123456789", nullptr); REQUIRE(id);
  CHECK((*id)[0] == 0x15); CHECK((*id)[7] == 0x11); }      // 0x112210F47DE98115
TEST_CASE("account_id_base64_roundtrip") {
  auto id = parseAccountId("1234567890123456789", nullptr);
  auto again = parseAccountId(accountIdToBase64(*id), nullptr); CHECK(again == id); }
TEST_CASE("account_id_trims_spaces_and_accepts_missing_padding") {
  auto b64 = accountIdToBase64(*parseAccountId("42", nullptr));      // ends with '='
  CHECK(parseAccountId("  " + b64 + " ", nullptr));
  CHECK(parseAccountId(b64.substr(0, b64.size() - 1), nullptr)); }
TEST_CASE("account_id_garbage_gives_error") { std::string e;
  CHECK_FALSE(parseAccountId("hello world", &e)); CHECK_FALSE(e.empty());
  CHECK_FALSE(parseAccountId("", &e)); CHECK_FALSE(parseAccountId("99999999999999999999999", &e)); }
```
- [ ] **Step 2:** Запустить — FAIL (нет символов).
- [ ] **Step 3:** Реализовать. Только цифры → `uint64` (переполнение = ошибка) → 8 байт
  little-endian. Иначе base64 (стандартный алфавит, `=` можно опустить) → ровно 8 байт.
  Текст ошибки на русском, например «Account ID: ожидается число или 12 символов base64».
- [ ] **Step 4:** Тесты — PASS.
- [ ] **Step 5: Commit** — `"core: PadState и разбор account ID"`

---

### Task 3: Мышь → стик (`MouseStick`)

**Files:**
- Create: `src/core/mouse_stick.h`, `src/core/mouse_stick.cpp`
- Test: `tests/test_mouse_stick.cpp`

**Interfaces:**
- Consumes: `Stick` (Task 2)
- Produces:
```cpp
struct MouseSettings {
    float sensitivity = 1.0f;   // 0.05..10   1.0 = полный наклон при 4 отсчётах/мс
    float yRatio = 1.0f;        // 0.25..3    вертикаль относительно горизонтали
    bool  invertY = false;
    float exponent = 1.0f;      // 0.5..3     степенная кривая
    float antiDeadzone = 0.12f; // 0..0.5     прыжок за мёртвую зону игры
    float outerLimit = 1.0f;    // 0.5..1     максимальный наклон
    int   windowMs = 8;         // 2..32      окно измерения скорости
    int   recenterMs = 10;      // 2..100     центр после стольких мс без движения
    void clamp(); };
class MouseStick { public:
    void configure(const MouseSettings& s);
    void addMotion(double tMs, int dx, int dy);
    Stick evaluate(double tMs);
    void reset();
    static float shape(float speedCountsPerMs, const MouseSettings& s); };
constexpr int kMaxMouseEvent = 2000;  // одиночная дельта больше этого — выброс
```
Формула (порядок важен):
```
v       = сумма dx,dy за последние windowMs / windowMs            (отсчёты/мс)
v.y    *= yRatio;  if invertY: v.y = -v.y
d       = min(|v| * sensitivity / 4.0, 1)
d       = d ^ exponent
out     = d > 0 ? antiDeadzone + (outerLimit - antiDeadzone) * d : 0
result  = v/|v| * out;  если |result| > 1 → нормировать
нет движения дольше recenterMs → ровно {0,0}
```

- [ ] **Step 1: Тесты** (время в мс; при `windowMs = 8`, `antiDeadzone = 0.12`)
```cpp
TEST_CASE("no_motion_is_centre") { MouseStick m; m.configure({}); CHECK(m.evaluate(100) == Stick{}); }
TEST_CASE("one_count_per_ms_right") { MouseStick m; m.configure({});
  for (int t = 1; t <= 8; ++t) m.addMotion(t, 1, 0);
  auto s = m.evaluate(8); CHECK(s.x == doctest::Approx(0.12f + 0.88f * 0.25f)); CHECK(s.y == 0); }
TEST_CASE("fast_motion_clamps_to_outer_limit") { MouseStick m; MouseSettings s; s.outerLimit = 0.9f; m.configure(s);
  m.addMotion(8, 400, 0); CHECK(m.evaluate(8).x == doctest::Approx(0.9f)); }
TEST_CASE("exponent_two") { CHECK(MouseStick::shape(1.0f, MouseSettings{.exponent = 2.0f})
  == doctest::Approx(0.12f + 0.88f * 0.0625f)); }
TEST_CASE("y_ratio_and_invert") { MouseStick m; MouseSettings s; s.yRatio = 2; s.invertY = true; m.configure(s);
  m.addMotion(8, 0, 4); auto r = m.evaluate(8); CHECK(r.y < 0); CHECK(r.x == 0);
  CHECK(-r.y == doctest::Approx(MouseStick::shape(1.0f, s))); }        // 4/8*2 = 1 отсчёт/мс
TEST_CASE("diagonal_length_never_exceeds_one") { MouseStick m; m.configure({});
  m.addMotion(8, 900, 900); auto r = m.evaluate(8); CHECK(std::hypot(r.x, r.y) <= 1.0001f); }
TEST_CASE("recentres_after_recenter_ms") { MouseStick m; m.configure({}); m.addMotion(8, 8, 0);
  CHECK(m.evaluate(8).x > 0); CHECK(m.evaluate(8 + 11) == Stick{}); }
TEST_CASE("huge_single_delta_is_ignored") { MouseStick m; m.configure({});
  m.addMotion(8, 5000, 0); CHECK(m.evaluate(8) == Stick{}); }
TEST_CASE("settings_clamp") { MouseSettings s; s.windowMs = 0; s.sensitivity = 99; s.clamp();
  CHECK(s.windowMs == 2); CHECK(s.sensitivity == 10.0f); }
```
- [ ] **Step 2:** Тесты — FAIL.
- [ ] **Step 3:** Реализовать. Кольцевой буфер на 2048 отсчётов с текущими суммами X/Y,
  устаревшие записи (старше `t − windowMs`) выбрасываются в `evaluate`. Если `|dx|` или
  `|dy| > kMaxMouseEvent`, событие отбрасывается целиком.
- [ ] **Step 4:** Тесты — PASS.
- [ ] **Step 5: Commit** — `"core: перевод мыши в стик"`

---

### Task 4: Раскладка и сборка состояния геймпада (`PadBuilder`)

**Files:**
- Create: `src/core/bindings.h`, `src/core/bindings.cpp`
- Test: `tests/test_bindings.cpp`

**Interfaces:**
- Consumes: `PadState`, `PadButton`, `Stick` (Task 2)
- Produces:
```cpp
enum class Action : std::uint8_t { /* 0..15 в порядке PadButton */ Cross, Circle, Square, Triangle,
    DpadUp, DpadDown, DpadLeft, DpadRight, L1, R1, L3, R3, Options, Create, Touchpad, PS,
    L2, R2, MoveUp, MoveDown, MoveLeft, MoveRight, Walk, Count };
using InputCode = std::uint16_t;   // 1..255 = Windows VK (левый/правый Shift/Ctrl/Alt раздельно)
namespace code { constexpr InputCode None = 0, MouseLeft = 0x101, MouseRight = 0x102,
    MouseMiddle = 0x103, MouseX1 = 0x104, MouseX2 = 0x105, WheelUp = 0x106, WheelDown = 0x107, Max = 0x108; }
namespace vk {   // значения Windows VK, чтобы rplay_core и тесты не зависели от windows.h
    constexpr InputCode Tab = 0x09, Enter = 0x0D, Esc = 0x1B, Space = 0x20, Left = 0x25, Up = 0x26,
    Right = 0x27, Down = 0x28, F8 = 0x77, LShift = 0xA0, RShift = 0xA1, LCtrl = 0xA2, RCtrl = 0xA3,
    LAlt = 0xA4, RAlt = 0xA5; }
struct Bindings { std::array<std::array<InputCode, 2>, (size_t)Action::Count> slots{}; };
std::string inputCodeName(InputCode c);                  // "W", "Space", "LShift", "Mouse1", "WheelUp", "F5" …
std::optional<InputCode> parseInputCode(std::string_view name);
std::string_view actionName(Action a);                   // "cross", "l2", "move_up", "walk" …
std::optional<Action> parseAction(std::string_view name);
class PadBuilder { public:
    PadBuilder(const Bindings& b, float walkScale);       // walkScale 0.1..1
    void setInput(InputCode c, bool down, double tMs);
    void releaseAll();
    PadState build(double tMs, Stick right) const; };
constexpr double kWheelPulseMs = 60.0;
```
Правила:
- клавиша на `L2`/`R2` даёт 255;
- `WheelUp`/`WheelDown` — `setInput(code, true, t)` «нажимает» на `kWheelPulseMs`;
- противоположные направления — побеждает нажатое последним;
- диагональ нормируется до длины 1, `Walk` умножает левый стик на `walkScale`;
- `F8` (`vk::F8`) нельзя назначить: `parseInputCode("F8")` → `nullopt`.

- [ ] **Step 1: Тесты**
```cpp
// helper: Bindings with W/A/S/D→Move*, LAlt→Walk, Mouse1→R2, Space→Square
TEST_CASE("w_moves_up") { auto pb = makeBuilder(); pb.setInput('W', true, 0); CHECK(pb.build(1, {}).left.y == -1.0f); }
TEST_CASE("opposite_keys_last_wins") { auto pb = makeBuilder(); pb.setInput('W', true, 0); pb.setInput('S', true, 5);
  CHECK(pb.build(6, {}).left.y == 1.0f); pb.setInput('S', false, 7); CHECK(pb.build(8, {}).left.y == -1.0f); }
TEST_CASE("diagonal_is_normalised") { auto pb = makeBuilder(); pb.setInput('W', true, 0); pb.setInput('A', true, 0);
  auto l = pb.build(1, {}).left; CHECK(std::hypot(l.x, l.y) == doctest::Approx(1.0f)); }
TEST_CASE("walk_scales_left_stick") { auto pb = makeBuilder(0.5f); pb.setInput('W', true, 0);
  pb.setInput(vk::LAlt, true, 0); CHECK(pb.build(1, {}).left.y == doctest::Approx(-0.5f)); }
TEST_CASE("mouse_left_is_full_r2") { auto pb = makeBuilder(); pb.setInput(code::MouseLeft, true, 0); CHECK(pb.build(1, {}).r2 == 255); }
TEST_CASE("wheel_is_a_60ms_pulse") { /* WheelUp→Triangle */ pb.setInput(code::WheelUp, true, 100);
  CHECK(pb.build(159, {}).pressed(PadButton::Triangle)); CHECK_FALSE(pb.build(161, {}).pressed(PadButton::Triangle)); }
TEST_CASE("right_stick_passes_through") { auto pb = makeBuilder(); CHECK(pb.build(1, {0.3f, -0.2f}).right == Stick{0.3f, -0.2f}); }
TEST_CASE("release_all_gives_neutral_pad") { auto pb = makeBuilder(); pb.setInput('W', true, 0); pb.setInput(' ', true, 0);
  pb.releaseAll(); CHECK(pb.build(1, {}) == PadState{}); }
TEST_CASE("code_names_roundtrip") { for (InputCode c : {InputCode('W'), vk::Space, vk::LShift,
  code::MouseRight, code::WheelDown}) CHECK(parseInputCode(inputCodeName(c)) == c);
  CHECK_FALSE(parseInputCode("F8")); CHECK_FALSE(parseInputCode("Banana")); }
```
(в тестах используются константы `vk::`, `windows.h` не подключается)
- [ ] **Step 2:** FAIL. **Step 3:** Реализовать. Таблица имён: буквы и цифры как есть, плюс
  `Space Enter Esc Tab Backspace LShift RShift LCtrl RCtrl LAlt RAlt Up Down Left Right
  F1..F12(кроме F8) Mouse1..Mouse5 WheelUp WheelDown`. **Step 4:** PASS.
- [ ] **Step 5: Commit** — `"core: раскладка и PadBuilder"`

---

### Task 5: Профили в JSON и профиль GTA по умолчанию

**Files:**
- Create: `src/core/profile.h`, `src/core/profile.cpp`
- Test: `tests/test_profile.cpp`

**Interfaces:**
- Consumes: `Bindings`, `inputCodeName/parseInputCode`, `actionName/parseAction` (Task 4), `MouseSettings` (Task 3)
- Produces:
```cpp
struct Profile { std::string name = "GTA 6"; Bindings bindings; MouseSettings mouse; float walkScale = 0.5f; };
Profile defaultGtaProfile();
std::string profileToJson(const Profile& p);
std::optional<Profile> profileFromJson(std::string_view json, std::vector<std::string>* warnings);
```
Формат:
```json
{ "name": "GTA 6", "walk_scale": 0.5,
  "mouse": { "sensitivity": 1.0, "y_ratio": 1.0, "invert_y": false, "exponent": 1.0,
             "anti_deadzone": 0.12, "outer_limit": 1.0, "window_ms": 8, "recenter_ms": 10 },
  "bindings": { "move_up": ["W"], "r2": ["Mouse1"], "square": ["Space"] } }
```
`defaultGtaProfile()` — стартовая раскладка по образцу GTA V. **Проверить в GTA 6 дома.**
- `move_* WASD`, `walk LAlt`;
- `cross LShift` (бег), `square Space` (прыжок), `triangle F` (сесть в машину), `circle R` (перезарядка);
- `r2 Mouse1`, `l2 Mouse2`, `l1 Tab` (колесо оружия), `r1 Q` (укрытие), `l3 LCtrl` (присесть), `r3 C` (взгляд назад);
- `dpad_up Up` (телефон), `dpad_down Down`, `dpad_left Left`, `dpad_right Right`;
- `touchpad M` (карта), `options Esc`, `triangle` второй слот — `WheelUp`.

- [ ] **Step 1: Тесты**
```cpp
TEST_CASE("default_profile_roundtrip") { auto p = defaultGtaProfile(); auto back = profileFromJson(profileToJson(p), nullptr);
  REQUIRE(back); CHECK(back->bindings.slots == p.bindings.slots); CHECK(back->mouse.windowMs == p.mouse.windowMs); }
TEST_CASE("missing_fields_take_defaults") { auto p = profileFromJson(R"({"name":"x"})", nullptr);
  REQUIRE(p); CHECK(p->mouse.sensitivity == 1.0f); CHECK(p->walkScale == 0.5f); }
TEST_CASE("unknown_key_is_skipped_with_warning") { std::vector<std::string> w;
  auto p = profileFromJson(R"({"bindings":{"cross":["Banana","Space"]}})", &w);
  REQUIRE(p); CHECK(p->bindings.slots[(int)Action::Cross][0] == vk::Space); CHECK(w.size() == 1);
  CHECK(w[0].find("Banana") != std::string::npos); }
TEST_CASE("out_of_range_values_are_clamped") { auto p = profileFromJson(R"({"mouse":{"window_ms":500,"sensitivity":-3}})", nullptr);
  REQUIRE(p); CHECK(p->mouse.windowMs == 32); CHECK(p->mouse.sensitivity == 0.05f); }
TEST_CASE("broken_json_returns_nullopt") { std::vector<std::string> w; CHECK_FALSE(profileFromJson("{oops", &w)); CHECK_FALSE(w.empty()); }
TEST_CASE("more_than_two_keys_keeps_first_two") { std::vector<std::string> w;
  auto p = profileFromJson(R"({"bindings":{"cross":["A","B","C"]}})", &w); REQUIRE(p); CHECK(w.size() == 1); }
```
- [ ] **Step 2:** FAIL. **Step 3:** Реализовать на nlohmann/json, без исключений наружу.
  `profileFromJson` ловит ошибку разбора и возвращает `nullopt`. После чтения вызывается
  `mouse.clamp()`, а `walkScale` ограничивается диапазоном 0.1..1. **Step 4:** PASS.
- [ ] **Step 5: Commit** — `"core: профили JSON и раскладка GTA по умолчанию"`

---

### Task 6: Связь с PS5 (`Ps5Link`), хранение привязки, `rplayctl`

**Files:**
- Create: `src/ps5/link.h`, `src/ps5/link.cpp`, `src/ps5/pairing_store.h`, `src/ps5/pairing_store.cpp`, `tools/rplayctl.cpp`
- Modify: `CMakeLists.txt` (цели `rplay_ps5`, `rplayctl`)
- Test: `tests/test_pairing_store.cpp`

**Interfaces:**
- Consumes: `PadState` (Task 2), `AccountId`/`parseAccountId` (Task 2)
- Produces:
```cpp
struct FoundConsole { std::string name, host; bool ready = false; };   // ready = не в режиме покоя
struct Pairing { std::string host, name; std::array<std::uint8_t,16> registKey{}, morning{};
                 std::uint32_t target = 0; };
std::vector<std::uint8_t> serializePairing(const Pairing&);
std::optional<Pairing> deserializePairing(std::span<const std::uint8_t>);
bool savePairing(const Pairing&, const std::filesystem::path&);   // DPAPI CryptProtectData
std::optional<Pairing> loadPairing(const std::filesystem::path&); // nullopt если нет файла / испорчен
std::filesystem::path appDataDir();                                // %APPDATA%\RplayPS (создаёт)

enum class LinkState { Idle, Waking, Connecting, Connected, Lost };
class Ps5Link { public:
    using StateFn = std::function<void(LinkState, const std::string& messageRu)>;  // любой поток
    explicit Ps5Link(StateFn onState); ~Ps5Link();
    std::vector<FoundConsole> discover(std::chrono::milliseconds timeout);
    std::optional<Pairing> pair(const std::string& host, const AccountId&, std::uint32_t pin, std::string* errorRu);
    void connect(const Pairing&);     // асинхронно: будит при необходимости, затем подключается
    void disconnect();                // сначала нейтральное состояние, затем стоп
    void submit(const PadState&);     // потокобезопасно; вне Connected игнорируется
    LinkState state() const; };
```
Переносится из PS5-DirectView `src/RemotePlayClient.cpp` с указанием авторства. Ключевые значения:
- **Привязка:** `ChiakiRegistInfo`: `target = CHIAKI_TARGET_PS5_1`, `broadcast = false`,
  `psn_online_id = nullptr`, `console_pin = 0`, `holepunch_info = nullptr`, `rudp = nullptr`.
- **Пробуждение:** `chiaki_discovery_wakeup(..., credential, true)`, затем ожидание `ready`
  до 45 с.
- **Подключение:** `ChiakiConnectInfo`: `ps5 = true`, пресет видео 360p H264,
  `audio_video_disabled = CHIAKI_AUDIO_VIDEO_DISABLED`, `enable_keyboard = false`,
  `enable_dualsense = false`, `auto_regist = false`, `holepunch_session = nullptr`,
  `packet_loss_max = 0.05`.
- **Отправка:** `PadState` → `ChiakiControllerState`, сопоставление кнопок как в
  `RemotePlayClient::SubmitPad`, стики переводятся в `int16` (−32768..32767).
- **Сообщения:** на русском. Если не открылся TCP 9295 — «Не удалось подключиться к PS5
  (TCP 9295). PS5 включена и в этой сети?». Если не проснулась — подсказка про «Разрешить
  включение PS5 по сети».

- [ ] **Step 1: Тест хранилища**
```cpp
TEST_CASE("pairing_roundtrip_dpapi") { Pairing p{"192.168.1.50", "PS5-123", {}, {}, 1000100};
  p.registKey[0] = 7; auto path = std::filesystem::temp_directory_path() / "rplay_test_pairing.bin";
  REQUIRE(savePairing(p, path)); auto back = loadPairing(path); REQUIRE(back);
  CHECK(back->host == p.host); CHECK(back->registKey == p.registKey); CHECK(back->target == p.target); }
TEST_CASE("pairing_tampered_file_is_rejected") { /* сохранить, испортить байт в середине, */ CHECK_FALSE(loadPairing(path)); }
TEST_CASE("pairing_missing_file") { CHECK_FALSE(loadPairing("Z:/nope/none.bin")); }
```
- [ ] **Step 2:** FAIL. **Step 3:** Реализовать `pairing_store` (формат: версия 1 байт + поля
  с длинами) и `Ps5Link`. **Step 4:** Тесты — PASS.
- [ ] **Step 5:** `rplayctl` — команды (вывод на русском, код возврата 0 при успехе):
  - `rplayctl discover` — список консолей: имя, IP, «готова» или «спит»;
  - `rplayctl pair <ip> <account-id> <pin>` — привязать и сохранить в `%APPDATA%\RplayPS\pairing.bin`;
  - `rplayctl press <button> [мс=150]` — подключиться, нажать, отпустить, отключиться (`cross`, `circle`, `ps`, …);
  - `rplayctl stick <x> <y> <мс>` — держать правый стик (понадобится для калибровки в следующем плане).
- [ ] **Step 6: Ручная проверка дома (нужна PS5).**
  - `rplayctl discover` находит PS5.
  - `pair` с PIN из **Настройки → Система → Дистанционное воспроизведение → Привязать
    устройство** проходит.
  - `rplayctl press ps` открывает главное меню PS5.
  - Если PS5 в режиме покоя, `press` будит её.
  - Выключить роутер посреди `stick … 10000` → сообщение «связь потеряна», программа не висит.
- [ ] **Step 7: Commit** — `"ps5: связь через ядро chiaki-ng, хранение привязки, rplayctl"`

---

### Task 7: Поток ввода и звуки

**Files:**
- Create: `src/win/input_thread.h`, `src/win/input_thread.cpp`, `src/win/sound.h`, `src/win/sound.cpp`
- Modify: `CMakeLists.txt` (цель `rplay_win`, линковка `avrt winmm`)

**Interfaces:**
- Consumes: `Profile` (Task 5), `PadBuilder` (Task 4), `MouseStick` (Task 3), `PadState`
- Produces:
```cpp
enum class Cue { CaptureOn, CaptureOff, FocusLost, LinkUp, LinkLost };
void playCue(Cue c);   // асинхронно, не блокирует вызывающий поток
enum class CaptureEvent { Captured, Released, FocusLost };
struct InputStats { double mouseHz = 0, tickJitterUs = 0; PadState lastPad; };
class InputThread { public:
    struct Callbacks { std::function<void(const PadState&)> onPad;        // вызывается из потока ввода
                       std::function<void(CaptureEvent)> onCapture; };
    InputThread(HWND mainWindow, Callbacks cb); ~InputThread();
    void start(); void stop();
    void setCapture(bool on);           // из любого потока
    bool captured() const;
    void setProfile(const Profile& p);  // из любого потока, применяется на следующем тике
    InputStats stats() const; };
```
Поведение (как PS5-DirectView `InputManager`):
- **Поток:** окно `HWND_MESSAGE`, `AvSetMmThreadCharacteristicsW(L"Games")`,
  `THREAD_PRIORITY_HIGHEST`, высокоточный таймер на 1 мс, `MsgWaitForMultipleObjectsEx`.
- **Захват:**
  - Raw Input для мыши и клавиатуры (`RIDEV_NOLEGACY | RIDEV_INPUTSINK`);
  - `ClipCursor` по окну и скрытый курсор;
  - `F8` в Raw Input снимает захват и на PS5 не передаётся.
- **На каждом тике и после каждой пачки `WM_INPUT`:**
  - если `GetForegroundWindow() != mainWindow` → снять захват и сообщить `FocusLost`;
  - иначе `pad = builder.build(now, mouse.evaluate(now))`;
  - `onPad(pad)` вызывается только при изменении состояния.
- **При снятии захвата** `onPad(PadState{})` вызывается обязательно.
- **Звуки** (WAV-синус в памяти, `PlaySoundW(..., SND_MEMORY | SND_ASYNC)`):
  - `CaptureOn` — 600→900 Гц по 70 мс;
  - `CaptureOff` — 900→600 Гц;
  - `FocusLost` и `LinkLost` — три раза по 300 Гц по 80 мс;
  - `LinkUp` — 1000 Гц, 120 мс.

- [ ] **Step 1:** Реализовать `sound` и `InputThread`.
- [ ] **Step 2:** Временный режим `rplayps.exe --console-only` (без PS5): окно-заглушка выводит
  в заголовок `L(x,y) R(x,y) mouseHz`. Собрать.
- [ ] **Step 3: Ручная проверка (можно без PS5):**
  - F8 включает захват (звук «вверх»), курсор пропадает;
  - WASD меняют `L`, движение мыши меняет `R` и возвращает его в 0 через ~10 мс после остановки;
  - `mouseHz` близок к частоте опроса мыши (500–1000);
  - Alt+Tab → тройной сигнал, захват снят, `L` и `R` = 0;
  - F8 снова → захват снят, звук «вниз».
- [ ] **Step 4: Commit** — `"win: поток ввода Raw Input, захват, звуки"`

---

### Task 8: Окно программы и сборка всего вместе

**Files:**
- Create: `src/app/main.cpp`, `src/app/ui.h`, `src/app/ui.cpp`
- Modify: `CMakeLists.txt` (цель `rplayps` WIN32, ImGui win32 + dx11 бэкенды, линковка `d3d11 dxgi`)

**Interfaces:**
- Consumes: `Ps5Link`, `loadPairing/savePairing/appDataDir` (Task 6), `InputThread`, `playCue` (Task 7),
  `Profile`, `profileFromJson/profileToJson/defaultGtaProfile` (Task 5), `parseAccountId` (Task 2)
- Produces: `rplayps.exe`

Поведение:
- **Запуск:**
  - читается `%APPDATA%\RplayPS\profiles\gta6.json`; если файла нет, он создаётся из
    `defaultGtaProfile()`; при ошибке берётся профиль по умолчанию, а предупреждения
    показываются в окне;
  - если есть привязка, сразу `link.connect(pairing)`.
- **`onPad` → `link.submit`.**
- **Состояния связи:**
  - `Connected` → `playCue(LinkUp)`;
  - `Lost` → `playCue(LinkLost)` и `input.setCapture(false)`;
  - F8 в состоянии `Lost` или `Idle` → снова `connect`.
- **Окно ImGui** (заголовок «RplayPS»), русские подписи, шрифт с кириллицей (`Segoe UI`
  из `C:\Windows\Fonts\segoeui.ttf`, диапазон `GetGlyphRangesCyrillic`):
  1. **PS5:**
     - статус и сообщение;
     - если привязки нет — поля «IP», «Account ID», «PIN» и кнопка «Привязать», а также
       кнопка «Найти PS5» (заполняет IP);
     - если привязка есть — «Подключить», «Отключить», «Забыть PS5».
  2. **Мышь:** ползунки всех полей `MouseSettings` с диапазонами из Task 3. Изменение сразу
     уходит в `input.setProfile`. Кнопка «Сохранить профиль».
  3. **Визуализатор:** два круга (левый и правый стик) с точкой, `mouseHz`, дрожание тика.
  4. Подсказка: «F8 — захват. Затем переключите монитор на вход PS5».
- **`WM_KEYDOWN VK_F8` без захвата** → `input.setCapture(true)`, `playCue(CaptureOn)`.
- **Выход:** `input.stop()`, `link.disconnect()` (нейтральное состояние уходит раньше стопа).

- [ ] **Step 1:** Реализовать `main.cpp` (WinMain, D3D11 + ImGui по образцу
  `examples/example_win32_directx11`) и `ui.cpp`. Убрать временный `--console-only`
  заголовок из Task 7: визуализатор его заменяет. Флаг `--console-only` оставить, он
  запускает программу без подключения к PS5.
- [ ] **Step 2:** Собрать, прогнать `ctest` — PASS.
- [ ] **Step 3: Ручная проверка дома (PS5 + GTA 6):**
  - привязка через окно;
  - F8 → переключить монитор на PS5 → персонаж ходит на WASD, камера крутится мышью,
    ЛКМ стреляет, ПКМ целится;
  - Alt+Tab вслепую → тройной сигнал, персонаж останавливается;
  - выключить PS5 посреди игры → сигнал «связь потеряна»; включить → F8 переподключает;
  - закрыть программу посреди бега → персонаж останавливается.
- [ ] **Step 4:** Записать впечатления и найденные значения настроек мыши в
  `docs/notes-gta6.md` — это вход для следующего плана (калибровка).
- [ ] **Step 5: Commit** — `"app: окно RplayPS, привязка, настройки, визуализатор"`

---

## Дальше (отдельные планы)
- **План 2 — калибровка** (этап 3):
  - адаптивное сглаживание, форма мёртвой зоны, перенос остатка;
  - живая настройка с голосом SAPI;
  - тесты «круг» и «сдвиг»;
  - LUT.
- **План 3 — профили GTA 6** (этап 4): прицел, машина, колесо оружия; раскладка по итогам `notes-gta6.md`.
- **План 4:** переключение входа монитора через DDC/CI, замер задержки.
