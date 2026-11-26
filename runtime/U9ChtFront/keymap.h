#pragma once
// Key bindings page of the front page: reads and writes an Ultima IX keymap (.kmp) the way u9.exe reads it.
// UI-free; tested by tests/keymap_tests.cpp on synthetic text.
//
// The rules mirror the game's own reader (docs/research/U9_EXTENDED_SETUP_TOOL.md, "Parser trace"):
//   - only 11 named sections are read; the section a line sits in does not matter;
//   - spaces and tabs are dropped, ';' starts a comment (except ";=x"), the key is the text before the first '=';
//   - the game has no length checks: 64 bytes for a stripped line, 32 for the key, 32 for the command, and
//     4,096 bytes for a section. This writer stays inside all of them and never writes a line without '='.
// The player's file is written as U9CHT.kmp beside u9.exe and selected with Options.ini Current_Keymap.
// Default.kmp is never modified.
#include "front_core.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace u9cht::front::keymap {
inline constexpr char PlayerFile[] = "U9CHT.kmp";
inline constexpr char ShippedFile[] = "Default.kmp";
inline constexpr size_t MaxKeys = 4;          // per command, in the page
inline constexpr size_t SectionLimit = 3800;  // bytes; the game's buffer is 4,096
inline constexpr int Pages = 6;
extern const wchar_t* const PageNames[Pages];

// Key code as u9.exe builds it (0x4D3C30 / 0x4D3430): low byte = virtual key.
inline constexpr uint16_t Shift = 0x100, Alt = 0x200, Ctrl = 0x400, Extended = 0x800;
// open_key_bindings brings up this page from the game (protocol.h). It is a runtime command the game ignores,
// like show_takeable; with no key of its own it uses Ctrl+Shift+K, unless another command already has that key.
inline constexpr char PageCommand[] = "open_key_bindings";
inline constexpr uint16_t DefaultPageHotkey = Ctrl | Shift | 0x4B;

// The category u9.exe gives a command it does not know (0x4D25E0).
inline constexpr int UnknownCategory = 0xFFFF;

struct Command {
    const char* name;      // as the game registers it
    int category;          // the game's category: with the name it decides which command wins a shared key
    int page;              // tab on the page
    const wchar_t* label;
};
// The commands a player can bind. Internal ones (shortcut_beltF1..F12, shift_*, the developer's mono screen
// and camera log commands) are left out; when a file binds them the lines are kept as `extra`.
const std::vector<Command>& commands();

// Key text of a keymap line -> code; 0 when the game would not know it (it shows a message box and ignores it).
uint16_t parse_key(std::string_view text);
// ESC, ESCAPE, F1..F12 as the whole key text: the game skips such a line without a message.
bool reserved_text(std::string_view text);
// Code -> key text for a keymap line ("ctrl+shift+KeyPadLeft"); "" for a code with no name.
std::string key_text(uint16_t code);
// Code -> text for the page ("Ctrl+Shift+數字鍵 ←").
std::wstring key_label(uint16_t code);

enum class KeyError { None, Modifier, Reserved, Unsupported };
// A key press on the page (WM_KEYDOWN virtual key and its extended bit, plus the modifier state) -> code.
// 0 with *error set when it cannot be a binding.
uint16_t code_from_press(unsigned vk, bool extended, bool shift, bool ctrl, bool alt, KeyError* error = nullptr);

struct Keymap {
    std::vector<std::vector<uint16_t>> keys;                 // one list per commands() entry
    std::vector<std::pair<std::string, uint16_t>> extra;     // bindings of commands not in commands(), kept as read
    int skipped = 0;                                         // lines the game would ignore or choke on
    Keymap();
};
Keymap parse(std::string_view text);
// False with *error when a section would pass SectionLimit.
bool serialize(const Keymap& k, std::string& out, std::wstring* error = nullptr);
// The command the game runs for `code` when several bind it (-1: none). The game walks its command list,
// sorted by category and then by name, and stops at the first owner.
int owner(const Keymap& k, uint16_t code);
// Number of bindings that lose to another command.
int conflicts(const Keymap& k);
// The keys that bring up the page: those of open_key_bindings, or DefaultPageHotkey when it has none and no other
// command binds that key; empty when neither.
std::vector<uint16_t> page_keys(const Keymap& k);
// Gives open_key_bindings the default key when page_keys would fall back to it, so the page shows and saves it.
void fill_page_default(Keymap& k);

// Current_Keymap=<name> in <game>/Options.ini; "" when absent or not a plain file name.
std::string current_keymap(const fs::path& game);
bool load(const fs::path& file, Keymap& out);
// Writes <game>/U9CHT.kmp and points Current_Keymap at it; every other byte of Options.ini is kept.
Result save(const fs::path& game, const Keymap& k);
// Points Current_Keymap at U9CHT.kmp (the Options.ini half of save). The front page runs it again when a game
// it saved bindings during has ended, in case the game wrote its own value back on the way out.
Result select(const fs::path& game);
}
