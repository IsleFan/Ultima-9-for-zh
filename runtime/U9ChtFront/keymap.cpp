#include "keymap.h"
#include <algorithm>
#include <regex>

namespace u9cht::front::keymap {
const wchar_t* const PageNames[Pages] = {L"移動與戰鬥", L"互動與日誌", L"法術書", L"一般", L"鏡頭", L"引擎與作弊"};

namespace {
struct KeyName { std::string name; uint8_t vk; bool extended; };
// u9.exe's key table at 0x788E00 (112 entries of 22 bytes), in its order, without the entries for Shift, Ctrl
// and Alt alone (the game never dispatches them). A name lookup takes the first match, as the game does.
const std::vector<KeyName>& key_table() {
    static const std::vector<KeyName> table = [] {
        std::vector<KeyName> t = {
            {"Backspace", 0x08, false}, {"Tab", 0x09, false}, {"Clear", 0x0C, false}, {"Enter", 0x0D, false},
            {"KeypadEnter", 0x0D, true}, {"Pause", 0x13, false}, {"CapsLock", 0x14, false}, {"ESC", 0x1B, false},
            {"ESCAPE", 0x1B, false}, {"Space", 0x20, false},
            {"PageUp", 0x21, true}, {"KeyPadPageUp", 0x21, false}, {"PageDown", 0x22, true}, {"KeyPadPageDown", 0x22, false},
            {"End", 0x23, true}, {"KeyPadEnd", 0x23, false}, {"Home", 0x24, true}, {"KeyPadHome", 0x24, false},
            {"Left", 0x25, true}, {"KeyPadLeft", 0x25, false}, {"Up", 0x26, true}, {"KeyPadUp", 0x26, false},
            {"Right", 0x27, true}, {"KeyPadRight", 0x27, false}, {"Down", 0x28, true}, {"KeyPadDown", 0x28, false},
            {"PrintScrn", 0x2C, false}, {"Insert", 0x2D, true}, {"KeyPadInsert", 0x2D, false},
            {"Delete", 0x2E, true}, {"KeyPadDelete", 0x2E, false}};
        for (char c = '0'; c <= '9'; ++c) t.push_back({std::string(1, c), uint8_t(c), false});
        for (char c = 'a'; c <= 'z'; ++c) t.push_back({std::string(1, c), uint8_t(c - 'a' + 'A'), false});
        for (int i = 0; i <= 9; ++i) t.push_back({"Keypad" + std::to_string(i), uint8_t(0x60 + i), false});
        t.insert(t.end(), {{"Keypad*", 0x6A, false}, {"Keypad+", 0x6B, false}, {"Keypad-", 0x6D, false},
                           {"Keypad.", 0x6E, false}, {"Keypad/", 0x6F, false}});
        for (int i = 1; i <= 12; ++i) t.push_back({"F" + std::to_string(i), uint8_t(0x6F + i), false});
        t.insert(t.end(), {{"NumLock", 0x90, true}, {"ScrollLock", 0x91, false}, {";", 0xBA, false}, {"=", 0xBB, false},
                           {",", 0xBC, false}, {"-", 0xBD, false}, {".", 0xBE, false}, {"/", 0xBF, false},
                           {"`", 0xC0, false}, {"LeftBracket", 0xDB, false}, {"\\", 0xDC, false},
                           {"RightBracket", 0xDD, false}, {"'", 0xDE, false}});
        return t;
    }();
    return table;
}
// u9.exe 0x788D50: a single shifted symbol becomes Shift plus its base key.
constexpr std::pair<char, char> ShiftedSymbols[] = {
    {'!', '1'}, {'@', '2'}, {'#', '3'}, {'$', '4'}, {'%', '5'}, {'^', '6'}, {'&', '7'}, {'*', '8'}, {'(', '9'}, {')', '0'},
    {':', ';'}, {'+', '='}, {'<', ','}, {'_', '-'}, {'>', '.'}, {'?', '/'}, {'~', '`'}, {'{', '['}, {'|', '\\'}, {'}', ']'},
    {'"', '\''}};

char upper(char c) { return c >= 'a' && c <= 'z' ? char(c - 32) : c; }
char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c + 32) : c; }
std::string upper(std::string_view s) { std::string o(s); std::transform(o.begin(), o.end(), o.begin(), [](char c) { return upper(c); }); return o; }
std::string lower(std::string_view s) { std::string o(s); std::transform(o.begin(), o.end(), o.begin(), [](char c) { return lower(c); }); return o; }
bool same(std::string_view a, std::string_view b) { return a.size() == b.size() && lower(a) == lower(b); }

// Sections u9.exe reads (0x4D25E0).
constexpr const char* ReadSections[] = {
    "movement commands", "interaction commands", "combat commands", "magic commands", "general options commands",
    "camera control commands", "h3d control commands", "engine tuning commands", "cheat commands",
    "development control commands", "code test commands"};
enum Out { OutMovement, OutInteraction, OutCombat, OutGeneral, OutCamera, OutEngine, OutCheat, OutOther, OutCount };
constexpr const char* OutNames[OutCount] = {
    "MOVEMENT COMMANDS", "INTERACTION COMMANDS", "COMBAT COMMANDS", "GENERAL OPTIONS COMMANDS",
    "CAMERA CONTROL COMMANDS", "ENGINE TUNING COMMANDS", "CHEAT COMMANDS", "DEVELOPMENT CONTROL COMMANDS"};
// Categories 8 and 10 both go under [CHEAT COMMANDS]: the runtime's cheat notices look only there.
Out section_of(int category) {
    switch (category) {
    case 0: return OutMovement; case 1: return OutInteraction; case 2: return OutCombat; case 4: return OutGeneral;
    case 5: return OutCamera; case 7: return OutEngine; case 8: case 10: return OutCheat; default: return OutOther;
    }
}
int find_command(std::string_view name) {
    const auto& c = commands();
    for (size_t i = 0; i < c.size(); ++i) if (same(c[i].name, name)) return int(i);
    return -1;
}
bool contains(const std::vector<uint16_t>& v, uint16_t code) { return std::find(v.begin(), v.end(), code) != v.end(); }
}

const std::vector<Command>& commands() {
    static const std::vector<Command> table = {
        // page 0: movement (category 0) and combat (2)
        {"forward", 0, 0, L"前進"}, {"backward", 0, 0, L"後退"},
        {"sidestep_left", 0, 0, L"向左平移"}, {"sidestep_right", 0, 0, L"向右平移"},
        {"jump", 0, 0, L"跳躍"}, {"climb", 0, 0, L"攀爬"}, {"toggle_movement_speed", 0, 0, L"切換走路／跑步"},
        {"mouse_button1", 0, 0, L"代替滑鼠左鍵"}, {"mouse_button2", 0, 0, L"代替滑鼠右鍵"},
        {"mouse_left", 0, 0, L"游標左移"}, {"mouse_right", 0, 0, L"游標右移"},
        {"mouse_up", 0, 0, L"游標上移"}, {"mouse_down", 0, 0, L"游標下移"},
        {"enter_combat", 2, 0, L"進入／離開戰鬥"}, {"defend", 2, 0, L"防禦"},
        {"target_lock", 2, 0, L"鎖定目標（開／關）"}, {"get_target_lock", 2, 0, L"切換下一個目標"},
        // page 1: interaction (1)
        {"activate_hand", 1, 1, L"手模式（拿取、移動物品）"}, {"activate_inventory", 1, 1, L"背包"},
        {"examine", 1, 1, L"顯示物件名稱（按住）"}, {"toggle_mainUI", 1, 1, L"切換主介面顯示"},
        {"toggle_equipmentUI", 1, 1, L"開關裝備介面"}, {"pause", 1, 1, L"暫停"},
        {"open_journal", 1, 1, L"日誌"}, {"open_world_map", 1, 1, L"世界地圖"},
        {"journal_save", 1, 1, L"存檔"}, {"journal_load", 1, 1, L"讀檔"}, {"journal_options", 1, 1, L"選項"},
        {"journal_stats", 1, 1, L"屬性"}, {"journal_history", 1, 1, L"歷史"}, {"journal_bestiary", 1, 1, L"怪物圖鑑"},
        {"journal_diary_first_page", 1, 1, L"日誌第一頁"}, {"journal_diary_last_page", 1, 1, L"日誌最後一頁"},
        {"book_next_page", 1, 1, L"書本下一頁"}, {"book_previous_page", 1, 1, L"書本上一頁"},
        {"exit_game", 1, 1, L"離開遊戲"},
        // page 2: spellbook (1)
        {"open_spellbook", 1, 2, L"法術書"}, {"spellbook_circle_0", 1, 2, L"基礎法術頁"},
        {"spellbook_circle_1", 1, 2, L"第一環"}, {"spellbook_circle_2", 1, 2, L"第二環"},
        {"spellbook_circle_3", 1, 2, L"第三環"}, {"spellbook_circle_4", 1, 2, L"第四環"},
        {"spellbook_circle_5", 1, 2, L"第五環"}, {"spellbook_circle_6", 1, 2, L"第六環"},
        {"spellbook_circle_7", 1, 2, L"第七環"}, {"spellbook_circle_8", 1, 2, L"第八環"},
        {"spellbook_rituals", 1, 2, L"儀式"}, {"spellbook_reagents", 1, 2, L"秘藥頁"},
        {"spellbook_glossary", 1, 2, L"力量之語"},
        // page 3: general options (4)
        {"take_screenshot", 4, 3, L"遊戲內截圖"}, {"momentary_first_person", 4, 3, L"暫時第一人稱（按住）"},
        {"invert_mouse", 4, 3, L"滑鼠上下反轉"}, {"mouseSensitivity_increase", 4, 3, L"提高滑鼠靈敏度"},
        {"mouseSensitivity_decrease", 4, 3, L"降低滑鼠靈敏度"}, {"pitchlocked", 4, 3, L"鎖定俯仰角"},
        {"toggle_screen_center", 4, 3, L"切換畫面中心"}, {"exit_to_new_camera", 4, 3, L"離開並採用新鏡頭"},
        {"exit_to_old_camera", 4, 3, L"離開並回到舊鏡頭"}, {"flush_music", 4, 3, L"重新載入音樂"},
        {"flush_spells", 4, 3, L"重新載入法術"},
        // page 4: camera (5)
        {"camera_mode", 5, 4, L"切換鏡頭模式"}, {"momentary_camera_mode", 5, 4, L"暫時切換鏡頭模式"},
        {"camera_in", 5, 4, L"鏡頭拉近"}, {"camera_out", 5, 4, L"鏡頭拉遠"},
        {"camera_way_in", 5, 4, L"鏡頭拉到最近"}, {"camera_way_out", 5, 4, L"鏡頭拉到最遠"},
        {"zoom_in", 5, 4, L"視野放大"}, {"zoom_out", 5, 4, L"視野縮小"}, {"zoom_reset", 5, 4, L"視野重設"},
        {"yaw_left", 5, 4, L"鏡頭左轉"}, {"yaw_right", 5, 4, L"鏡頭右轉"},
        {"pitch_up", 5, 4, L"鏡頭上仰"}, {"pitch_down", 5, 4, L"鏡頭下俯"}, {"spin_camera", 5, 4, L"旋轉鏡頭"},
        // page 5: engine tuning (7) and cheats (8, 10)
        {"clipping_plane_in", 7, 5, L"遠景距離縮短"}, {"clipping_plane_out", 7, 5, L"遠景距離拉長"},
        {"clipping_plane_reset", 7, 5, L"遠景距離重設"}, {"middle_plane_in", 7, 5, L"中景距離縮短"},
        {"middle_plane_out", 7, 5, L"中景距離拉長"}, {"middle_plane_reset", 7, 5, L"中景距離重設"},
        {"toggle_avatar_invulnerable", 8, 5, L"作弊：無敵（開／關）"}, {"toggle_avatar_fly", 8, 5, L"作弊：飛行（開／關）"},
        {"toggle_avatar_fast", 8, 5, L"作弊：加速移動（開／關）"}, {"avatar_pathfind", 8, 5, L"作弊：聖者自動尋路"},
        {"pass_one_hour", 8, 5, L"作弊：時間快轉一小時"}, {"unpass_one_hour", 8, 5, L"作弊：時間倒退一小時"},
        {"pass_one_minute", 8, 5, L"作弊：時間快轉一分鐘"}, {"unpass_one_minute", 8, 5, L"作弊：時間倒退一分鐘"},
        {"sunrise_sunset", 8, 5, L"作弊：日出／日落"}, {"toggle_sun", 8, 5, L"作弊：太陽（開／關）"},
        {"toggle_wind", 8, 5, L"作弊：風（開／關）"}, {"toggle_storms", 8, 5, L"作弊：暴風雨（開／關）"},
        {"recharge_mana", 10, 5, L"作弊：補滿法力"}, {"fill_spellbook", 10, 5, L"作弊：法術書填滿"},
        {"spells_0-2", 10, 5, L"作弊：取得 0～2 環法術 ⚠"}, {"spells_3-5", 10, 5, L"作弊：取得 3～5 環法術 ⚠"},
        {"spells_6-8", 10, 5, L"作弊：取得 6～8 環法術 ⚠"},
    };
    return table;
}

Keymap::Keymap() : keys(commands().size()) {}

bool reserved_text(std::string_view text) {
    const auto s = upper(text);
    if (s == "ESC" || s == "ESCAPE") return true;
    for (int i = 1; i <= 12; ++i) if (s == "F" + std::to_string(i)) return true;
    return false;
}

uint16_t parse_key(std::string_view text) {
    auto s = upper(text);
    if (s == "UNDEFINED") return 0;
    uint16_t code = 0;
    const std::pair<const char*, uint16_t> modifiers[] = {{"SHIFT+", Shift}, {"ALT+", Alt}, {"CTRL+", Ctrl}};
    for (const auto& [name, bit] : modifiers) {
        const auto at = s.find(name);
        if (at != std::string::npos) { code |= bit; s.erase(at, std::char_traits<char>::length(name)); }
    }
    if (s.size() == 1)
        for (const auto& [shifted, base] : ShiftedSymbols) if (s[0] == shifted) { code |= Shift; s[0] = base; }
    for (const auto& k : key_table())
        if (same(k.name, s)) return uint16_t(code | k.vk | (k.extended ? Extended : 0));
    return 0;
}

namespace {
const KeyName* name_of(uint16_t code) {
    for (const auto& k : key_table())
        if (k.vk == (code & 0xFF) && k.extended == ((code & Extended) != 0)) return &k;
    return nullptr;
}
}

std::string key_text(uint16_t code) {
    const auto* k = name_of(code);
    if (!k) return {};
    return std::string(code & Ctrl ? "ctrl+" : "") + (code & Alt ? "alt+" : "") + (code & Shift ? "shift+" : "") + k->name;
}

std::wstring key_label(uint16_t code) {
    const auto* k = name_of(code);
    if (!k) return L"?";
    std::wstring name(k->name.begin(), k->name.end());
    const std::pair<const wchar_t*, const wchar_t*> friendly[] = {
        {L"Left", L"←"}, {L"Right", L"→"}, {L"Up", L"↑"}, {L"Down", L"↓"}, {L"Space", L"空白鍵"},
        {L"LeftBracket", L"["}, {L"RightBracket", L"]"}, {L"KeyPadLeft", L"數字鍵 ←"}, {L"KeyPadRight", L"數字鍵 →"},
        {L"KeyPadUp", L"數字鍵 ↑"}, {L"KeyPadDown", L"數字鍵 ↓"}, {L"KeypadEnter", L"數字鍵 Enter"},
        {L"KeyPadPageUp", L"數字鍵 PageUp"}, {L"KeyPadPageDown", L"數字鍵 PageDown"}, {L"KeyPadEnd", L"數字鍵 End"},
        {L"KeyPadHome", L"數字鍵 Home"}, {L"KeyPadInsert", L"數字鍵 Insert"}, {L"KeyPadDelete", L"數字鍵 Delete"},
        {L"Clear", L"數字鍵 5（NumLock 關）"}};
    bool mapped = false;
    for (const auto& [from, to] : friendly) if (name == from) { name = to; mapped = true; break; }
    if (!mapped && name.rfind(L"Keypad", 0) == 0) name = L"數字鍵 " + name.substr(6);
    else if (!mapped && name.size() == 1) name[0] = wchar_t(upper(char(name[0])));
    return std::wstring(code & Ctrl ? L"Ctrl+" : L"") + (code & Alt ? L"Alt+" : L"") + (code & Shift ? L"Shift+" : L"") + name;
}

uint16_t code_from_press(unsigned vk, bool extended, bool shift, bool ctrl, bool alt, KeyError* error) {
    const auto fail = [&](KeyError e) { if (error) *error = e; return uint16_t(0); };
    if (error) *error = KeyError::None;
    if (vk == 0x10 || vk == 0x11 || vk == 0x12 || (vk >= 0xA0 && vk <= 0xA5) || vk == 0x5B || vk == 0x5C)
        return fail(KeyError::Modifier);
    if (vk > 0xFF) return fail(KeyError::Unsupported);
    const bool modified = shift || ctrl || alt;
    // The game skips ESC and F1..F12 only as the whole key text; Esc is kept out with modifiers too, because
    // shift+escape is a built-in binding.
    if (vk == 0x1B || (!modified && vk >= 0x70 && vk <= 0x7B)) return fail(KeyError::Reserved);
    const auto code = uint16_t(vk | (extended ? Extended : 0));
    if (!name_of(code) || vk == 0x2C) return fail(KeyError::Unsupported);
    return uint16_t(code | (shift ? Shift : 0) | (ctrl ? Ctrl : 0) | (alt ? Alt : 0));
}

Keymap parse(std::string_view text) {
    Keymap k;
    bool inside = false;
    size_t at = 0;
    while (at <= text.size()) {
        auto end = text.find('\n', at);
        if (end == std::string_view::npos) end = text.size();
        auto line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string_view::npos) continue;
        if (line[first] == '[') {
            const auto close = line.find(']', first);
            const auto name = close == std::string_view::npos ? std::string() : lower(line.substr(first + 1, close - first - 1));
            inside = std::any_of(std::begin(ReadSections), std::end(ReadSections), [&](const char* s) { return name == s; });
            continue;
        }
        if (!inside) continue;
        // The game's own stripping (0x4D26AC): no spaces or tabs; ';' is a comment unless it is ";=x".
        std::string token;
        for (size_t i = 0; i < line.size(); ++i) {
            const char c = line[i];
            if (c == ' ' || c == '\t') continue;
            if (c == ';') {
                const char n1 = i + 1 < line.size() ? line[i + 1] : '\0', n2 = i + 2 < line.size() ? line[i + 2] : '\0';
                if (n1 == '=' && n2 != '=') { token += ';'; continue; }
                break;
            }
            token += c;
        }
        if (token.empty()) continue;
        const auto eq = token.find('=');
        if (eq == std::string::npos || token.size() >= 64) { ++k.skipped; continue; }
        auto key = token.substr(0, eq);
        size_t rest = eq + 1;
        if (rest < token.size() && token[rest] == '=') { key += '='; ++rest; }   // "==x" binds the '=' key
        const auto name = token.substr(rest);
        if (key.empty() || name.empty() || key.size() >= 32 || name.size() >= 32) { ++k.skipped; continue; }
        if (reserved_text(key)) { ++k.skipped; continue; }
        const auto code = parse_key(key);
        if (!code || key_text(code).empty()) { ++k.skipped; continue; }
        const int index = find_command(name);
        if (index < 0) {
            if (std::find(k.extra.begin(), k.extra.end(), std::make_pair(name, code)) == k.extra.end()) k.extra.push_back({name, code});
        } else if (!contains(k.keys[size_t(index)], code)) k.keys[size_t(index)].push_back(code);
    }
    return k;
}

bool serialize(const Keymap& k, std::string& out, std::wstring* error) {
    const auto& c = commands();
    std::string body[OutCount];
    const auto add = [&](Out section, uint16_t code, const std::string& name) {
        const auto key = key_text(code);
        // Stripped line = key + '=' + name; the game's buffers are 32, 32 and 64 bytes.
        if (key.empty() || key.size() >= 32 || name.size() >= 32 || key.size() + 1 + name.size() >= 64) return;
        // No spaces: the game tests the raw character after ';', so "; = x" would be a comment.
        body[section] += key + "=" + name + "\r\n";
    };
    for (size_t i = 0; i < c.size() && i < k.keys.size(); ++i)
        for (const auto code : k.keys[i]) add(section_of(c[i].category), code, c[i].name);
    for (const auto& [name, code] : k.extra) add(OutOther, code, name);
    out = "; Ultima IX keymap written by U9Cht.exe (U9-CHT front page).\r\n"
          "; Change it on the front page. The game's own Default.kmp is not modified.\r\n\r\n";
    for (int s = 0; s < OutCount; ++s) {
        if (body[s].size() > SectionLimit) {
            if (error) *error = L"「" + std::wstring(OutNames[s], OutNames[s] + std::char_traits<char>::length(OutNames[s])) +
                                L"」這一類的按鍵太多，遊戲讀不完，請刪掉幾個。";
            return false;
        }
        out += std::string("[") + OutNames[s] + "]\r\n" + body[s] + "\r\n";
    }
    return true;
}

int owner(const Keymap& k, uint16_t code) {
    const auto& c = commands();
    int best = -1;
    for (size_t i = 0; i < c.size() && i < k.keys.size(); ++i) {
        if (!contains(k.keys[i], code)) continue;
        if (best < 0 || c[i].category < c[size_t(best)].category ||
            (c[i].category == c[size_t(best)].category && lower(c[i].name) < lower(c[size_t(best)].name))) best = int(i);
    }
    return best;
}

int conflicts(const Keymap& k) {
    int n = 0;
    for (size_t i = 0; i < k.keys.size(); ++i)
        for (const auto code : k.keys[i]) if (owner(k, code) != int(i)) ++n;
    return n;
}

namespace {
const std::regex CurrentKeymap("Current_Keymap=([^\r\n]*)");
bool plain_name(const std::string& name) {
    return !name.empty() && name.size() < 64 && name.find_first_of("/\\:") == std::string::npos && name != "." && name != "..";
}
}

std::string current_keymap(const fs::path& game) {
    std::string options;
    std::smatch m;
    if (!read_bytes(game / L"Options.ini", options) || !std::regex_search(options, m, CurrentKeymap)) return {};
    auto name = m[1].str();
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
    return plain_name(name) ? name : std::string();
}

bool load(const fs::path& file, Keymap& out) {
    std::string text;
    if (!read_bytes(file, text)) return false;
    out = parse(text);
    return true;
}

Result save(const fs::path& game, const Keymap& k) {
    std::string text, options;
    std::wstring error;
    if (!serialize(k, text, &error)) return {false, error};
    if (!read_bytes(game / L"Options.ini", options) || !std::regex_search(options, CurrentKeymap))
        return {false, L"Options.ini 裡找不到 Current_Keymap，沒有儲存按鍵。"};
    if (!write_bytes(game / PlayerFile, text)) return {false, L"無法寫入 U9CHT.kmp。"};
    const auto updated = std::regex_replace(options, CurrentKeymap, std::string("Current_Keymap=") + PlayerFile,
                                            std::regex_constants::format_first_only);
    if (updated != options && !write_bytes(game / L"Options.ini", updated)) return {false, L"無法寫入 Options.ini。"};
    return {};
}
}
