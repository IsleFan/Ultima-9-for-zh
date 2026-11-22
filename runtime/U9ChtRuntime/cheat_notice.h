#pragma once
// Developer cheat notices (requested 2026-11-12, session-29). Header-only and game-free: tested by
// tests/cheat_notice_tests.cpp.
//
// The game runs a cheat command from the active keymap's [CHEAT COMMANDS] section silently. When the active
// keymap holds such bindings (only the developer test keymap U9CHT-TEST.kmp does; a retail Default.kmp has
// none), the runtime shows a short Chinese line at the top of the screen, like a narration subtitle.
//
// Toggle state is read from the game, not counted (u9.exe handlers, CONFIRMED by disassembly 2026-11-12):
//   toggle_avatar_invulnerable 0x49B7C0: bit 0 of [[0x98F31C]+0x48]
//   toggle_avatar_fly          0x49B7F0: bit 9 of [[[[0x850228]+0xD0]+0xE8]+0xD8]
//   toggle_avatar_fast         0x584720: float [0x7944EC] != 1.0
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace u9cht::cheat {
inline constexpr uint32_t ModCtrl = 1, ModShift = 2, ModAlt = 4;
struct Binding { uint32_t vk = 0, mods = 0; std::string command; };
enum class Kind { Message, Fast, Fly, Invulnerable, AddGold };
struct Text { Kind kind = Kind::Message; const wchar_t* text = L""; };

inline std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}
inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}
// "1".."9", "0", "a".."z" and "f1".."f12"; 0 for any other key name.
inline uint32_t key_code(std::string_view name) {
    const auto n = lower(name);
    if (n.size() == 1 && ((n[0] >= '0' && n[0] <= '9') || (n[0] >= 'a' && n[0] <= 'z'))) return uint32_t(std::toupper(n[0]));
    if (n.size() >= 2 && n.size() <= 3 && n[0] == 'f') {
        int v = 0;
        for (size_t i = 1; i < n.size(); ++i) { if (n[i] < '0' || n[i] > '9') return 0; v = v*10 + (n[i]-'0'); }
        if (v >= 1 && v <= 12) return 0x70 + uint32_t(v-1);   // VK_F1..VK_F12
    }
    return 0;
}
// The [CHEAT COMMANDS] bindings of a keymap text ("shift+ctrl+1 = fill_spellbook"), or with `every_section` the
// bindings of all sections. Comments start with ';'.
inline std::vector<Binding> parse_bindings(std::string_view kmp, bool every_section = false) {
    std::vector<Binding> out;
    bool inside = every_section;
    while (!kmp.empty()) {
        const auto end = kmp.find('\n');
        auto line = trim(kmp.substr(0,end));
        kmp = end == std::string_view::npos ? std::string_view{} : kmp.substr(end+1);
        if (const auto c = line.find(';'); c != std::string_view::npos) line = trim(line.substr(0,c));
        if (line.empty()) continue;
        if (line.front() == '[') { inside = every_section || lower(line) == "[cheat commands]"; continue; }
        const auto eq = line.find('=');
        if (!inside || eq == std::string_view::npos) continue;
        const auto keys = lower(trim(line.substr(0,eq)));
        const auto command = lower(trim(line.substr(eq+1)));
        Binding b; b.command = command;
        std::string_view rest = keys;
        bool ok = !command.empty();
        while (ok && !rest.empty()) {
            const auto plus = rest.find('+');
            const auto token = trim(rest.substr(0,plus));
            rest = plus == std::string_view::npos ? std::string_view{} : rest.substr(plus+1);
            if (token == "ctrl") b.mods |= ModCtrl;
            else if (token == "shift") b.mods |= ModShift;
            else if (token == "alt") b.mods |= ModAlt;
            else if (!b.vk && (b.vk = key_code(token)) != 0) continue;
            else ok = false;
        }
        if (ok && b.vk) out.push_back(std::move(b));
    }
    return out;
}
// Every binding of one command, whatever its section. Keys key_code() does not know are left out.
inline std::vector<Binding> command_bindings(std::string_view kmp, std::string_view command) {
    auto all = parse_bindings(kmp,true);
    std::erase_if(all,[&](const Binding& b) { return b.command != command; });
    return all;
}
// The keys of show_takeable (2026-11-22, docs/research/U9_OBJECT_PROBE.md). Not a game command: the game files it with
// no handler, like add_gold; the runtime marks takeable objects while one of its keys is held. When the keymap binds
// neither show_takeable nor a plain F to anything, F is used.
inline std::vector<Binding> take_keys(std::string_view kmp) {
    auto keys = command_bindings(kmp,"show_takeable");
    if (keys.empty()) {
        bool f_used = false;
        for (const auto& b : parse_bindings(kmp,true)) if (b.vk == 'F' && !b.mods) f_used = true;
        if (!f_used) keys.push_back({uint32_t('F'),0,"show_takeable"});
    }
    return keys;
}
// Current_Keymap=<file> in Options.ini, or "" when absent.
inline std::string current_keymap(std::string_view options) {
    while (!options.empty()) {
        const auto end = options.find('\n');
        const auto line = trim(options.substr(0,end));
        options = end == std::string_view::npos ? std::string_view{} : options.substr(end+1);
        if (lower(line.substr(0,15)) == "current_keymap=") return std::string(trim(line.substr(15)));
    }
    return {};
}
inline std::optional<Text> text_for(std::string_view command) {
    struct Row { std::string_view command; Text text; };
    static constexpr Row rows[] = {
        {"fill_spellbook", {Kind::Message, L"法術書已填滿所有法術"}},
        {"recharge_mana", {Kind::Message, L"法力已補滿"}},
        {"toggle_avatar_fast", {Kind::Fast, L"加速"}},
        {"toggle_avatar_fly", {Kind::Fly, L"飛行"}},
        {"toggle_avatar_invulnerable", {Kind::Invulnerable, L"無敵"}},
        {"pass_one_hour", {Kind::Message, L"時間快轉一小時"}},
        {"unpass_one_hour", {Kind::Message, L"時間倒退一小時"}},
        {"pass_one_minute", {Kind::Message, L"時間快轉一分鐘"}},
        {"unpass_one_minute", {Kind::Message, L"時間倒退一分鐘"}},
        {"sunrise_sunset", {Kind::Message, L"切換日出／日落"}},
        {"toggle_sun", {Kind::Message, L"切換太陽"}},
        {"toggle_wind", {Kind::Message, L"切換風"}},
        {"toggle_storms", {Kind::Message, L"切換暴風雨"}},
        {"spells_0-2", {Kind::Message, L"已加入線性至第二環法術"}},
        {"spells_3-5", {Kind::Message, L"已加入第三至第五環法術"}},
        {"spells_6-8", {Kind::Message, L"已加入第六至第八環法術"}},
        // Not a game command: the game keeps the binding with no handler; the runtime acts on it (U9_GOLD.md).
        {"add_gold", {Kind::AddGold, L"金幣增加一千"}},
    };
    for (const auto& r : rows) if (r.command == command) return r.text;
    return std::nullopt;
}
// "無敵（開）", "無敵（關）"; state < 0 (unreadable) gives "無敵（切換）".
inline std::wstring toggle_text(const Text& t, int state) {
    return std::wstring(t.text) + (state < 0 ? L"（切換）" : state ? L"（開）" : L"（關）");
}
// add_gold outcomes. Digits do not warm for notices (HANDOFF), so amounts are written in Chinese numerals.
inline constexpr uint32_t GoldStep = 1000, GoldLimit = 9999;   // 9999: the most the game lets the Avatar carry
inline constexpr const wchar_t* GoldAdded = L"金幣增加一千";
inline constexpr const wchar_t* GoldCapped = L"金幣已補到上限九千九百九十九";
inline constexpr const wchar_t* GoldFull = L"金幣已達上限九千九百九十九";
inline constexpr const wchar_t* GoldNoPurse = L"身上要先有金幣才能增加";
inline constexpr const wchar_t* GoldFailed = L"金幣增加失敗";
// The new amount and the notice for `before` gold.
struct GoldStepResult { uint32_t after; const wchar_t* text; };
inline constexpr GoldStepResult gold_step(uint32_t before) noexcept {
    if (before >= GoldLimit) return {before, GoldFull};
    if (before + GoldStep > GoldLimit) return {GoldLimit, GoldCapped};
    return {before + GoldStep, GoldAdded};
}
static_assert(gold_step(0).after == 1000 && gold_step(8999).after == 9999 && gold_step(9000).after == 9999 &&
              gold_step(9999).after == 9999 && gold_step(12000).after == 12000);
// Every character a notice can show, warmed into the glyph cache at start.
inline std::wstring all_notice_characters() {
    std::wstring out = L"（開關切換）「」";
    for (const auto* t : {GoldAdded, GoldCapped, GoldFull, GoldNoPurse, GoldFailed}) out += t;
    for (const auto* c : {"fill_spellbook","recharge_mana","toggle_avatar_fast","toggle_avatar_fly",
                          "toggle_avatar_invulnerable","pass_one_hour","unpass_one_hour","pass_one_minute",
                          "unpass_one_minute","sunrise_sunset","toggle_sun","toggle_wind","toggle_storms",
                          "spells_0-2","spells_3-5","spells_6-8"})
        out += text_for(c)->text;
    return out;
}
}
