// Key bindings core (U9ChtFront/keymap.h) on synthetic text only: no game asset is read.
// LOCAL-ONLY part, SKIPPED WHEN ASSETS ABSENT: with U9_GAME_DIR set, the game's own Default.kmp must survive
// parse -> serialize -> parse unchanged.
#include "keymap.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>

using namespace u9cht::front;
using namespace u9cht::front::keymap;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
int index_of(const char* name) {
    const auto& c = commands();
    for (size_t i = 0; i < c.size(); ++i) if (std::string(c[i].name) == name) return int(i);
    return -1;
}
const std::vector<uint16_t>& keys_of(const Keymap& k, const char* name) { return k.keys[size_t(index_of(name))]; }
void write(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary | std::ios::trunc) << data;
}
std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {});
}
}

int main() {
    {   // the command table
        std::set<std::string> names;
        for (const auto& c : commands()) {
            check(names.insert(c.name).second, "command names are unique");
            check(std::string(c.name).size() < 32 && c.page >= 0 && c.page < Pages && c.label && *c.label, "command entry sane");
        }
        check(commands().size() == 97, "97 bindable commands");
        check(index_of("abort_targeting") < 0 && index_of("toggle_movement_speed") >= 0, "only commands the game registers");
        int rows[Pages]{};
        for (const auto& c : commands()) ++rows[c.page];
        for (int n : rows) check(n > 0 && n <= 23, "a tab holds at most 23 rows");
    }
    {   // key text -> code, as u9.exe 0x4D3C30
        check(parse_key("a") == 0x41 && parse_key("A") == 0x41, "letters, any case");
        check(parse_key("ctrl+m") == (Ctrl | 0x4D) && parse_key("CTRL+M") == (Ctrl | 0x4D), "modifier, any case");
        check(parse_key("shift+ctrl+m") == parse_key("ctrl+shift+m"), "modifier order is free");
        check(parse_key("shift+alt+m") == (Shift | Alt | 0x4D), "two modifiers");
        check(parse_key("Left") == (Extended | 0x25) && parse_key("KeyPadLeft") == 0x25, "grey and keypad arrows differ");
        check(parse_key("pageup") == (Extended | 0x21) && parse_key("delete") == (Extended | 0x2E), "extended keys");
        check(parse_key("Keypad5") == 0x65 && parse_key("Keypad+") == 0x6B, "keypad with NumLock on");
        check(parse_key("!") == (Shift | 0x31) && parse_key("?") == (Shift | 0xBF), "shifted symbol");
        check(parse_key("`") == 0xC0 && parse_key("LeftBracket") == 0xDB && parse_key("=") == 0xBB, "punctuation");
        check(parse_key("alt+f4") == (Alt | 0x73) && parse_key("CapsLock") == 0x14 && parse_key("space") == 0x20, "named keys");
        check(parse_key("undefined") == 0 && parse_key("") == 0 && parse_key("RightClick") == 0 && parse_key("ctrl+") == 0, "not keys");
        check(parse_key("{") == 0, "'{' maps to '[', which the game's table does not name");
        check(parse_key("shift") == 0 && parse_key("ctrl+alt") == 0, "a modifier alone is not a binding");
    }
    {   // reserved texts
        check(reserved_text("esc") && reserved_text("ESCAPE") && reserved_text("f1") && reserved_text("F12"), "reserved");
        check(!reserved_text("alt+f4") && !reserved_text("shift+escape") && !reserved_text("f13") && !reserved_text("f"), "not reserved");
    }
    {   // code -> text, and back, for every key the table names
        for (unsigned vk = 1; vk < 256; ++vk)
            for (const bool ext : {false, true})
                for (const uint16_t mods : {uint16_t(0), Shift, Ctrl, Alt, uint16_t(Shift | Ctrl | Alt)}) {
                    const auto code = uint16_t(vk | (ext ? Extended : 0) | mods);
                    const auto text = key_text(code);
                    if (text.empty()) continue;
                    check(text.size() < 32, "key text fits the game's 32-byte buffer");
                    check(parse_key(text) == code, "key text round trip");
                    check(!key_label(code).empty(), "label");
                }
        check(key_text(Ctrl | Shift | 0x25) == "ctrl+shift+KeyPadLeft" && key_text(Extended | 0x25) == "Left", "canonical text");
        check(key_text(0x07).empty() && key_text(Extended | 0x41).empty(), "codes without a name");
        check(key_label(Ctrl | 0x4D) == L"Ctrl+M" && key_label(Extended | 0x25) == L"←" && key_label(0x20) == L"空白鍵", "labels");
    }
    {   // a key press on the page
        KeyError e{};
        check(code_from_press(0x4D, false, false, true, false, &e) == (Ctrl | 0x4D) && e == KeyError::None, "ctrl+m");
        check(code_from_press(0x25, true, false, false, false, &e) == (Extended | 0x25), "grey left");
        check(code_from_press(0x25, false, false, false, false, &e) == 0x25, "keypad left");
        check(code_from_press(0x11, false, false, true, false, &e) == 0 && e == KeyError::Modifier, "ctrl alone");
        check(code_from_press(0x1B, false, false, false, false, &e) == 0 && e == KeyError::Reserved, "esc");
        check(code_from_press(0x1B, false, true, false, false, &e) == 0 && e == KeyError::Reserved, "shift+esc is built in");
        check(code_from_press(0x70, false, false, false, false, &e) == 0 && e == KeyError::Reserved, "f1");
        check(code_from_press(0x73, false, false, false, true, &e) == (Alt | 0x73), "alt+f4 is allowed");
        check(code_from_press(0x6F, true, false, false, false, &e) == 0 && e == KeyError::Unsupported, "keypad / arrives extended");
        check(code_from_press(0x2C, false, false, false, false, &e) == 0 && e == KeyError::Unsupported, "print screen is built in");
        check(code_from_press(0xE2, false, false, false, false, &e) == 0 && e == KeyError::Unsupported, "a key the game does not name");
        check(code_from_press(0x4B, false, true, true, false, &e) == 0 && e == KeyError::Hotkey, "ctrl+shift+k opens the page");
        check(code_from_press(0x4B, false, false, true, false, &e) == (Ctrl | 0x4B) && code_from_press(0x4B, false, true, true, true, &e) != 0,
              "ctrl+k and ctrl+shift+alt+k stay bindable");
    }
    {   // reading, with the game's line rules
        const std::string text =
            "; header = not a binding\r\n"
            "forward = up\r\n"                                   // before any section: not read
            "[MOVEMENT COMMANDS]\r\n"
            "; RightClick = Move\r\n"
            "CapsLock    = toggle_movement_speed\r\n"
            "alt+CapsLock \t= toggle_movement_speed\r\n"
            "Up = forward ; with a comment = here\r\n"
            "KeyPadUp=FORWARD\r\n"
            "Up = forward\r\n"                                   // the same key again
            "f1 = jump\r\n"                                      // reserved: skipped
            "banana = jump\r\n"                                  // not a key: skipped
            "climb\r\n"                                          // no '=': skipped
            "[Interaction Commands]\n"
            "q = activate_hand\n"
            "` = activate_hand\n"
            ";=examine\n"                                        // binds the ';' key
            "==pause\n"                                          // binds the '=' key
            "a = abort_targeting\n"                              // a name the game does not register: kept
            "[COMMENTS]\r\n"
            "b = jump\r\n"                                       // a section the game never reads
            "[cheat commands]\r\n"
            "shift+ctrl+1=fill_spellbook\r\n";
        const auto k = parse(text);
        check(keys_of(k, "toggle_movement_speed") == std::vector<uint16_t>({0x14, uint16_t(Alt | 0x14)}), "two keys for one command");
        check(keys_of(k, "forward") == std::vector<uint16_t>({uint16_t(Extended | 0x26), 0x26}), "comment, case, duplicate");
        check(keys_of(k, "jump").empty(), "reserved, unknown and unread lines bind nothing");
        check(keys_of(k, "activate_hand") == std::vector<uint16_t>({0x51, 0xC0}), "LF lines");
        check(keys_of(k, "examine") == std::vector<uint16_t>({0xBA}) && keys_of(k, "pause") == std::vector<uint16_t>({0xBB}), "';' and '=' keys");
        check(keys_of(k, "fill_spellbook") == std::vector<uint16_t>({uint16_t(Shift | Ctrl | 0x31)}), "cheat section");
        check(k.extra.size() == 1 && k.extra[0].first == "abort_targeting" && k.extra[0].second == 0x41, "unknown command kept");
        check(k.skipped == 3, "three lines skipped");

        // writing, then reading again
        std::string out;
        check(serialize(k, out), "serialize");
        const auto again = parse(out);
        check(again.keys == k.keys && again.extra == k.extra && again.skipped == 0, "round trip");
        check(out.find("[CHEAT COMMANDS]\r\nctrl+shift+1=fill_spellbook\r\n") != std::string::npos, "cheats under [CHEAT COMMANDS]");
        check(out.find(";=examine\r\n") != std::string::npos && out.find("==pause\r\n") != std::string::npos, "';' and '=' lines");
        size_t at = 0;
        while (at < out.size()) {   // every line stays inside the game's buffers and has '=' unless it is a comment
            const auto end = out.find("\r\n", at);
            const auto line = out.substr(at, end - at);
            at = end + 2;
            if (line.empty() || line[0] == '[' || (line[0] == ';' && line.rfind(";=", 0) != 0)) continue;
            check(line.size() < 64 && line.find('=') != std::string::npos && line.find(' ') == std::string::npos, "line form");
        }
    }
    {   // the longest possible line still fits
        Keymap k;
        const auto longest = uint16_t(Ctrl | Alt | Shift | 0x22);   // ctrl+alt+shift+KeyPadPageDown
        for (auto& keys : k.keys) keys = {longest};
        std::string out;
        std::wstring error;
        check(serialize(k, out, &error), "one long key per command fits every section");
        check(parse(out).keys == k.keys, "long lines round trip");
        for (auto& keys : k.keys) keys = {longest, uint16_t(longest ^ Shift), uint16_t(longest ^ Alt), uint16_t(longest ^ Ctrl)};
        check(!serialize(k, out, &error) && !error.empty(), "a section over the game's buffer is refused");
    }
    {   // which command wins a shared key: category first, then name
        Keymap k;
        const uint16_t key = 0x54;
        k.keys[size_t(index_of("target_lock"))] = {key};        // category 2
        k.keys[size_t(index_of("journal_history"))] = {key};    // category 1
        k.keys[size_t(index_of("examine"))] = {key};            // category 1, earlier name
        check(owner(k, key) == index_of("examine"), "category, then name");
        check(owner(k, 0x55) == -1 && conflicts(k) == 2, "conflict count");
        k.keys[size_t(index_of("jump"))] = {key};               // category 0
        check(owner(k, key) == index_of("jump") && conflicts(k) == 3, "movement wins");
    }
    {   // saving beside a synthetic Options.ini
        const auto game = fs::temp_directory_path() / L"u9cht-keymap-tests";
        std::error_code ec;
        fs::remove_all(game, ec);
        const std::string options = "[Interface]\r\n\r\n; Current keymap\r\nCurrent_Keymap=default.kmp\r\n\r\nQuickSpell=0\r\n";
        write(game / L"Options.ini", options);
        write(game / L"Default.kmp", "[MOVEMENT COMMANDS]\r\nspace = jump\r\n");
        check(current_keymap(game) == "default.kmp", "current keymap");
        Keymap k;
        check(load(game / L"Default.kmp", k) && keys_of(k, "jump") == std::vector<uint16_t>({0x20}), "load");
        check(!load(game / L"absent.kmp", k), "absent file");
        k.keys[size_t(index_of("climb"))] = {0x43};
        const auto shipped = read(game / L"Default.kmp");
        check(save(game, k).ok, "save");
        check(current_keymap(game) == PlayerFile, "Options.ini points at the player's file");
        check(read(game / L"Options.ini") == "[Interface]\r\n\r\n; Current keymap\r\nCurrent_Keymap=U9CHT.kmp\r\n\r\nQuickSpell=0\r\n",
              "every other byte of Options.ini is kept");
        check(read(game / L"Default.kmp") == shipped, "Default.kmp is not modified");
        Keymap saved;
        check(load(game / PlayerFile, saved) && saved.keys == k.keys, "saved file reads back");
        check(save(game, k).ok && current_keymap(game) == PlayerFile, "saving again");
        write(game / L"Options.ini", options);   // as if the game wrote its own value back on exit
        check(select(game).ok && current_keymap(game) == PlayerFile, "select points Current_Keymap back");
        check(read(game / L"Options.ini") == "[Interface]\r\n\r\n; Current keymap\r\nCurrent_Keymap=U9CHT.kmp\r\n\r\nQuickSpell=0\r\n",
              "select keeps every other byte");
        write(game / L"Options.ini", "[Interface]\r\n");
        check(!save(game, k).ok && current_keymap(game).empty(), "no Current_Keymap: refused");
        check(!select(game).ok, "select without Current_Keymap: refused");
        write(game / L"Options.ini", "Current_Keymap=..\\x.kmp\r\n");
        check(current_keymap(game).empty(), "a path is not accepted as a keymap name");
        fs::remove_all(game, ec);
    }
    {   // LOCAL-ONLY, SKIPPED WHEN ASSETS ABSENT
        char* dir = nullptr; size_t n = 0;
        if (_dupenv_s(&dir, &n, "U9_GAME_DIR") == 0 && dir && *dir) {
            Keymap k;
            if (load(fs::path(dir) / L"Default.kmp", k)) {
                std::string out;
                check(serialize(k, out) && parse(out).keys == k.keys && parse(out).extra == k.extra, "LOCAL: Default.kmp round trip");
                int bound = 0;
                for (const auto& keys : k.keys) bound += int(keys.size());
                std::cout << "LOCAL Default.kmp: " << bound << " bindings, " << k.extra.size() << " extra, " << k.skipped
                          << " skipped, " << conflicts(k) << " conflicts\n";
            }
        } else std::cout << "LOCAL-ONLY Default.kmp check skipped (U9_GAME_DIR not set)\n";
        free(dir);
    }
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "keymap tests passed\n";
    return 0;
}
