// Cheat notices (U9ChtRuntime/cheat_notice.h) on synthetic keymap text only.
#include "cheat_notice.h"
#include <iostream>

using namespace u9cht::cheat;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
}

int main() {
    const std::string kmp =
        "[INTERACTION COMMANDS]\r\nshift+ctrl+1 = not_a_cheat\r\n"
        "[CHEAT COMMANDS]\r\n; test keymap\r\n"
        "shift+ctrl+1 = fill_spellbook\r\n"
        "Shift+Ctrl+5 = Toggle_Avatar_Invulnerable ; comment\r\n"
        "alt+shift+k = toggle_avatar_fly\r\n"
        "ctrl+f12 = recharge_mana\r\n"
        "shift+ctrl+0 = spells_6-8\r\n"
        "ctrl+pageup = zoom_in\r\n"            // unsupported key name: skipped
        "shift+ctrl = toggle_wind\r\n"         // no key: skipped
        "shift+ctrl+2 =\r\n"                   // no command: skipped
        "[CODE TEST COMMANDS]\r\nshift+ctrl+9 = recharge_mana\r\n";
    const auto b = parse_bindings(kmp);
    check(b.size() == 5, "five bindings in the cheat section only");
    if (b.size() == 5) {
        check(b[0].vk == '1' && b[0].mods == (ModCtrl|ModShift) && b[0].command == "fill_spellbook", "digit binding");
        check(b[1].vk == '5' && b[1].command == "toggle_avatar_invulnerable", "case-insensitive, comment stripped");
        check(b[2].vk == 'K' && b[2].mods == (ModAlt|ModShift), "letter with alt");
        check(b[3].vk == 0x7B && b[3].mods == ModCtrl, "function key");
        check(b[4].vk == '0' && b[4].command == "spells_6-8", "zero key");
    }
    check(parse_bindings("[Cheat Commands]\nshift+ctrl+1=recharge_mana").size() == 1, "LF and no spaces");
    check(parse_bindings("").empty() && parse_bindings("[CHEAT COMMANDS]\n").empty(), "empty");
    {   // show_takeable keys: bound, unbound (F), unbound with F taken by another command
        const auto bound = take_keys("[CHEAT COMMANDS]\r\nshift+g = show_takeable\r\n");
        check(bound.size() == 1 && bound[0].vk == 'G' && bound[0].mods == ModShift, "show_takeable bound");
        const auto fallback = take_keys("[INTERACTION COMMANDS]\r\nj = open_journal\r\n");
        check(fallback.size() == 1 && fallback[0].vk == 'F' && fallback[0].mods == 0, "show_takeable falls back to F");
        check(take_keys("[MOVEMENT COMMANDS]\r\nf = forward\r\n").empty(), "no fallback when F is bound");
        check(take_keys("").size() == 1, "empty keymap: F");
    }
    {   // one command's bindings in any section (the map key, for the hover block)
        const char* text = "[INTERACTION COMMANDS]\r\nctrl+m = open_world_map\r\nj = open_journal\r\n"
                           "[CHEAT COMMANDS]\r\nshift+ctrl+1 = fill_spellbook\r\n[OTHER]\r\nF9=Open_World_Map ; second key\r\ntab = open_world_map\r\n";
        const auto maps = command_bindings(text,"open_world_map");
        check(maps.size() == 2 && maps[0].vk == 'M' && maps[0].mods == ModCtrl && maps[1].vk == 0x78 && maps[1].mods == 0,
              "map bindings from every section; unknown key names left out");
        check(command_bindings(text,"no_such_command").empty(), "no binding of an unbound command");
        check(parse_bindings(text).size() == 1 && parse_bindings(text,true).size() == 4, "cheat section only unless asked");
    }
    check(current_keymap("[Controls]\r\nCurrent_Keymap=U9CHT-TEST.kmp\r\n") == "U9CHT-TEST.kmp", "current keymap");
    check(current_keymap("[Controls]\r\n").empty(), "no keymap");

    const auto inv = text_for("toggle_avatar_invulnerable");
    check(inv && inv->kind == Kind::Invulnerable, "toggle kind");
    if (inv) {
        check(toggle_text(*inv,1) == L"無敵（開）" && toggle_text(*inv,0) == L"無敵（關）" &&
              toggle_text(*inv,-1) == L"無敵（切換）", "toggle texts");
    }
    check(text_for("recharge_mana") && std::wstring(text_for("recharge_mana")->text) == L"法力已補滿", "message text");
    check(!text_for("zoom_in"), "unknown command");
    check(all_notice_characters().find(L"補") != std::wstring::npos, "warm set");
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "cheat notice tests passed\n";
    return 0;
}
