#include "front_core.h"
#include "platform.h"
#include "protocol.h"
#include "version_manifest.h"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace u9cht::front {
namespace {
constexpr wchar_t Receipt[] = L"installed.txt";

std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring out(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), n);
    return out;
}
std::wstring hash_or_empty(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) return {};
    try { return sha256(p); } catch (...) { return {}; }
}
std::wstring read_ini(const fs::path& file, const wchar_t* section, const wchar_t* key) {
    wchar_t buffer[2048]{};
    GetPrivateProfileStringW(section, key, L"\x01", buffer, DWORD(std::size(buffer)), file.c_str());
    return buffer[0] == L'\x01' ? std::wstring(L"\x01") : std::wstring(buffer);
}
bool read_number(const fs::path& file, const wchar_t* section, const wchar_t* key, uint32_t& out) {
    const auto text = read_ini(file, section, key);
    if (text == L"\x01" || text.empty() || text.size() > 6 || !std::all_of(text.begin(), text.end(), iswdigit)) return false;
    out = uint32_t(std::stoul(text)); return true;
}
void write_utf16(const fs::path& file, const std::wstring& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + utf8(file.wstring()));
    const unsigned char bom[] = {0xFF, 0xFE};
    out.write(reinterpret_cast<const char*>(bom), 2);
    out.write(reinterpret_cast<const char*>(text.data()), std::streamsize(text.size() * sizeof(wchar_t)));
    if (!out) throw std::runtime_error("cannot write " + utf8(file.wstring()));
}
bool copy_verified(const fs::path& from, const fs::path& to, const std::wstring& expected) {
    std::error_code ec;
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return !ec && hash_or_empty(to) == expected;
}
struct Entry { std::wstring name, original, installed; };
std::vector<Entry> read_receipt(const fs::path& file) {
    std::vector<Entry> entries;
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto a = line.find('\t'), b = a == std::string::npos ? a : line.find('\t', a + 1);
        if (b == std::string::npos) continue;
        entries.push_back({widen(line.substr(0, a)), widen(line.substr(a + 1, b - a - 1)), widen(line.substr(b + 1))});
    }
    return entries;
}
// Only plain file names inside <game>/static are ever touched.
bool safe_name(const std::wstring& name) {
    return !name.empty() && name.size() < 64 && name.find_first_of(L"/\\:") == std::wstring::npos && name != L"." && name != L"..";
}
}

Settings default_settings() {
    Settings s;
    s.features = DefaultFeatures;
    s.hover_delay_ms = DefaultHoverDelayMs;
    return s;
}

Settings load_settings(const fs::path& launcher_ini, const fs::path& front_ini) {
    auto s = default_settings();
    std::error_code ec;
    if (fs::is_regular_file(front_ini, ec)) {
        const auto dir = read_ini(front_ini, L"game", L"dir");
        if (dir != L"\x01") s.game_dir = dir;
        uint32_t w = 0, h = 0;
        if (read_number(front_ini, L"display", L"width", w) && read_number(front_ini, L"display", L"height", h))
            for (const auto& r : Resolutions) if (r.width == w && r.height == h) { s.width = w; s.height = h; }
        uint32_t windowed = 0;
        if (read_number(front_ini, L"display", L"windowed", windowed)) s.windowed = windowed == 1;
        // Values the front page itself copied out of dxcfg.ini; anything unexpected is dropped.
        const auto plain = [](const std::wstring& v) {
            return !v.empty() && v.size() <= 32 &&
                   std::all_of(v.begin(), v.end(), [](wchar_t c) { return c < 128 && (iswalnum(c) || c == L'@' || c == L'_' || c == L'-' || c == L'.'); });
        };
        const auto presentation = read_ini(front_ini, L"display", L"saved_presentation"), display = read_ini(front_ini, L"display", L"saved_display");
        if (plain(presentation)) { s.saved_presentation = presentation; if (plain(display)) s.saved_display = display; }
    }
    if (!fs::is_regular_file(launcher_ini, ec)) return s;
    const auto font = read_ini(launcher_ini, L"font", L"file");
    if (font != L"\x01") s.font = font;
    uint32_t v = 0;
    if (read_number(launcher_ini, L"journal", L"letter_spacing", v)) s.letter_spacing = std::min(v, MaxJournalLetterSpacing);
    if (read_number(launcher_ini, L"journal", L"line_spacing", v)) s.line_spacing = std::min(v, MaxJournalLineSpacing);
    if (read_number(launcher_ini, L"dialogue", L"letter_spacing", v)) s.dialogue_letter = std::min(v, MaxDialogueLetterSpacing);
    if (read_number(launcher_ini, L"dialogue", L"line_spacing", v)) s.dialogue_line = std::min(v, MaxDialogueLineSpacing);
    const std::pair<const wchar_t*, uint32_t> bits[] = {
        {L"hover_names", FeatureHoverNames}, {L"wheel_pages", FeatureWheelPages}, {L"wheel_zoom", FeatureWheelZoom},
        {L"movie_subtitles", FeatureMovieSubtitles}, {L"input_probe", FeatureInputProbe},
        {L"status_numbers", FeatureStatusNumbers}, {L"floating_numbers", FeatureFloatingNumbers},
        {L"target_bar", FeatureTargetBar}, {L"smart_jump", FeatureSmartJump}};
    for (const auto& [key, bit] : bits)
        if (read_number(launcher_ini, L"features", key, v) && v <= 1) s.features = v ? (s.features | bit) : (s.features & ~bit);
    if (read_number(launcher_ini, L"features", L"hover_delay_ms", v))
        s.hover_delay_ms = std::clamp(v, MinHoverDelayMs, MaxHoverDelayMs);
    return s;
}

void save_settings(const Settings& s, const fs::path& launcher_ini, const fs::path& front_ini) {
    const auto flag = [&](uint32_t bit) { return (s.features & bit) ? L"1" : L"0"; };
    std::wostringstream l;
    l << L"; Written by U9Cht.exe (the front page). Read by U9ChtLauncher.exe.\r\n";
    if (!s.font.empty()) l << L"[font]\r\nfile = " << s.font << L"\r\n";
    l << L"[journal]\r\nletter_spacing = " << s.letter_spacing << L"\r\nline_spacing = " << s.line_spacing << L"\r\n";
    l << L"[dialogue]\r\nletter_spacing = " << s.dialogue_letter << L"\r\nline_spacing = " << s.dialogue_line << L"\r\n";
    l << L"[features]\r\nhover_names = " << flag(FeatureHoverNames) << L"\r\nhover_delay_ms = " << s.hover_delay_ms
      << L"\r\nwheel_pages = " << flag(FeatureWheelPages) << L"\r\nwheel_zoom = " << flag(FeatureWheelZoom)
      << L"\r\nmovie_subtitles = " << flag(FeatureMovieSubtitles) << L"\r\ninput_probe = " << flag(FeatureInputProbe)
      << L"\r\nstatus_numbers = " << flag(FeatureStatusNumbers)
      << L"\r\nfloating_numbers = " << flag(FeatureFloatingNumbers)
      << L"\r\ntarget_bar = " << flag(FeatureTargetBar)
      << L"\r\nsmart_jump = " << flag(FeatureSmartJump) << L"\r\n";
    write_utf16(launcher_ini, l.str());
    std::wostringstream f;
    f << L"; Written by U9Cht.exe (the front page).\r\n[game]\r\ndir = " << s.game_dir << L"\r\n";
    f << L"[display]\r\nwidth = " << s.width << L"\r\nheight = " << s.height << L"\r\nwindowed = " << (s.windowed ? 1 : 0) << L"\r\n";
    if (!s.saved_presentation.empty()) {
        f << L"saved_presentation = " << s.saved_presentation << L"\r\n";
        if (!s.saved_display.empty()) f << L"saved_display = " << s.saved_display << L"\r\n";
    }
    write_utf16(front_ini, f.str());
}

std::map<std::wstring, std::wstring> json_hash_object(const std::string& json, const std::string& key) {
    std::map<std::wstring, std::wstring> out;
    const auto at = json.find("\"" + key + "\"");
    if (at == std::string::npos) return out;
    auto i = json.find_first_not_of(" \t\r\n", at + key.size() + 2);
    if (i == std::string::npos || json[i] != ':') return out;
    i = json.find_first_not_of(" \t\r\n", i + 1);
    if (i == std::string::npos || json[i] != '{') return out;
    const auto end = json.find('}', i);
    if (end == std::string::npos) return out;
    const auto body = json.substr(i + 1, end - i - 1);
    size_t p = 0;
    while (true) {
        const auto k0 = body.find('"', p); if (k0 == std::string::npos) break;
        const auto k1 = body.find('"', k0 + 1); if (k1 == std::string::npos) return {};
        const auto colon = body.find_first_not_of(" \t\r\n", k1 + 1);
        if (colon == std::string::npos || body[colon] != ':') return {};
        const auto v0 = body.find_first_not_of(" \t\r\n", colon + 1);
        if (v0 == std::string::npos || body[v0] != '"') return {};
        const auto v1 = body.find('"', v0 + 1); if (v1 == std::string::npos) return {};
        const auto value = body.substr(v0 + 1, v1 - v0 - 1);
        if (value.size() != 64 || value.find_first_not_of("0123456789abcdef") != std::string::npos) return {};
        out[widen(body.substr(k0 + 1, k1 - k0 - 1))] = widen(value);
        p = v1 + 1;
    }
    return out;
}

Manifest read_manifest(const fs::path& file) {
    Manifest m;
    std::ifstream in(file, std::ios::binary);
    if (!in) return m;
    std::ostringstream data; data << in.rdbuf();
    const auto json = data.str();
    m.source = json_hash_object(json, "source_game_sha256");
    m.output = json_hash_object(json, "resource_output_sha256");
    const auto at = json.find("\"runtime_plan_count\"");
    if (at != std::string::npos) {
        auto i = json.find_first_of("0123456789", at + 20);
        while (i != std::string::npos && i < json.size() && json[i] >= '0' && json[i] <= '9') m.plans = m.plans * 10 + uint64_t(json[i++] - '0');
    }
    m.ok = !m.output.empty() && m.output.size() == m.source.size();
    for (const auto& [name, hash] : m.output) if (!m.source.count(name) || !safe_name(name)) m.ok = false;
    return m;
}

GameStatus check_game(const fs::path& dir, const Manifest& m, std::wstring* detail) {
    std::error_code ec;
    if (dir.empty() || !fs::is_regular_file(dir / L"u9.exe", ec)) return GameStatus::NotFound;
    if (!known_exe(hash_or_empty(dir / L"u9.exe"))) return GameStatus::ExeMismatch;
    if (hash_or_empty(dir / L"ddraw.dll") != WrapperHash) return GameStatus::WrapperMismatch;
    for (const auto& [name, source] : m.source) {
        const auto now = hash_or_empty(dir / L"static" / name);
        const auto out = m.output.count(name) ? m.output.at(name) : std::wstring();
        if (now != source && now != out) { if (detail) *detail = name; return GameStatus::ResourceMismatch; }
    }
    return GameStatus::Ok;
}

std::wstring game_edition(const fs::path& dir) { return exe_edition(hash_or_empty(dir / L"u9.exe")); }

std::vector<std::wstring> steam_library_paths(const std::string& vdf) {
    std::vector<std::wstring> out;
    size_t at = 0;
    while ((at = vdf.find("\"path\"", at)) != std::string::npos) {
        at += 6;
        const auto open = vdf.find('"', at);
        if (open == std::string::npos) break;
        std::string text;
        size_t i = open + 1;
        for (; i < vdf.size() && vdf[i] != '"'; ++i) {
            if (vdf[i] == '\\' && i + 1 < vdf.size()) ++i;   // VDF escapes: a doubled backslash, an escaped quote
            text += vdf[i];
        }
        if (i >= vdf.size()) break;
        if (!text.empty()) out.push_back(widen(text));
        at = i + 1;
    }
    return out;
}

std::vector<fs::path> find_game_candidates() {
    std::vector<fs::path> found;
    const auto add = [&](const std::wstring& dir) {
        std::error_code ec;
        if (dir.empty() || !fs::is_regular_file(fs::path(dir) / L"u9.exe", ec)) return;
        const auto canon = fs::weakly_canonical(dir, ec);
        for (const auto& f : found) if (fs::equivalent(f, canon, ec)) return;
        found.push_back(canon);
    };
    const auto value = [](HKEY key, const wchar_t* name) {
        wchar_t buffer[1024]{}; DWORD size = sizeof(buffer) - sizeof(wchar_t), type = 0;
        if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size) != ERROR_SUCCESS ||
            (type != REG_SZ && type != REG_EXPAND_SZ)) return std::wstring();
        return std::wstring(buffer);
    };
    const auto is_u9 = [](std::wstring name) {
        std::transform(name.begin(), name.end(), name.begin(), towlower);
        return name.find(L"ultima 9") != std::wstring::npos || name.find(L"ultima ix") != std::wstring::npos;
    };
    const auto scan = [&](HKEY root, const wchar_t* path, REGSAM view, const wchar_t* name_value, const wchar_t* path_value) {
        HKEY parent = nullptr;
        if (RegOpenKeyExW(root, path, 0, KEY_READ | view, &parent) != ERROR_SUCCESS) return;
        wchar_t sub[256];
        for (DWORD i = 0;; ++i) {
            DWORD n = DWORD(std::size(sub));
            if (RegEnumKeyExW(parent, i, sub, &n, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY child = nullptr;
            if (RegOpenKeyExW(parent, sub, 0, KEY_READ | view, &child) != ERROR_SUCCESS) continue;
            if (is_u9(value(child, name_value))) add(value(child, path_value));
            RegCloseKey(child);
        }
        RegCloseKey(parent);
    };
    for (const REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        scan(HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games", view, L"gameName", L"path");
        scan(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", view, L"DisplayName", L"InstallLocation");
    }
    scan(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, L"DisplayName", L"InstallLocation");
    {   // Steam: every library in libraryfolders.vdf, at the release's install folder.
        std::vector<std::wstring> steam;
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam", 0, KEY_READ, &key) == ERROR_SUCCESS) {
            steam.push_back(value(key, L"SteamPath")); RegCloseKey(key);
        }
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", 0, KEY_READ | KEY_WOW64_32KEY, &key) == ERROR_SUCCESS) {
            steam.push_back(value(key, L"InstallPath")); RegCloseKey(key);
        }
        for (const auto& root : steam) {
            if (root.empty()) continue;
            std::ifstream in(fs::path(root) / L"steamapps" / L"libraryfolders.vdf", std::ios::binary);
            std::ostringstream text; text << in.rdbuf();
            auto libraries = steam_library_paths(text.str());
            libraries.push_back(root);
            for (const auto& lib : libraries) add((fs::path(lib) / L"steamapps" / L"common" / L"Ultima IX").wstring());
        }
    }
    for (const wchar_t* dir : {L"C:\\GOG Games\\Ultima 9", L"C:\\GOG Games\\Ultima IX",
                               L"C:\\Program Files (x86)\\GOG Galaxy\\Games\\Ultima 9",
                               L"C:\\Program Files (x86)\\GOG.com\\Ultima 9"}) add(dir);
    std::stable_sort(found.begin(), found.end(), [](const fs::path& a, const fs::path& b) {
        return !game_edition(a).empty() > !game_edition(b).empty();
    });
    return found;
}

bool read_bytes(const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::ostringstream data; data << in.rdbuf(); out = data.str(); return true;
}
bool write_bytes(const fs::path& p, const std::string& data) {
    const auto temp = fs::path(p.wstring() + L".u9cht-tmp");
    {   std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(data.data(), std::streamsize(data.size()));
        if (!out) return false; }
    return MoveFileExW(temp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

Resolution game_resolution(const fs::path& game) {
    std::string text;
    if (!read_bytes(game / L"Options.ini", text)) return {};
    std::smatch w, h;
    if (!std::regex_search(text, w, std::regex("GameScreenWidth=(\\d{1,5})")) ||
        !std::regex_search(text, h, std::regex("GameScreenHeight=(\\d{1,5})"))) return {};
    return {uint32_t(std::stoul(w[1].str())), uint32_t(std::stoul(h[1].str()))};
}

Result apply_resolution(const fs::path& game, Resolution r) {
    if (!r.width || !r.height) return {};
    std::string options, wrapper;
    if (!read_bytes(game / L"Options.ini", options) || !read_bytes(game / L"dxcfg.ini", wrapper))
        return {false, L"找不到 Options.ini 或 dxcfg.ini，無法設定解析度。"};
    const std::regex width("GameScreenWidth=\\d{1,5}"), height("GameScreenHeight=\\d{1,5}"), display("display=\\d{1,5}x\\d{1,5}");
    if (!std::regex_search(options, width) || !std::regex_search(options, height))
        return {false, L"Options.ini 的格式和預期不同，沒有修改解析度。"};
    // The game renders at the Options.ini size. dxcfg.ini follows only when it names a size: the Steam release
    // ships display=desktop (the wrapper scales to the desktop), which is left as it is.
    const auto w = std::to_string(r.width), h = std::to_string(r.height);
    const auto first = std::regex_constants::format_first_only;
    const auto new_options = std::regex_replace(std::regex_replace(options, width, "GameScreenWidth=" + w, first),
                                                height, "GameScreenHeight=" + h, first);
    const auto new_wrapper = std::regex_replace(wrapper, display, "display=" + w + "x" + h, first);
    if (new_options != options && !write_bytes(game / L"Options.ini", new_options)) return {false, L"無法寫入 Options.ini。"};
    if (new_wrapper != wrapper && !write_bytes(game / L"dxcfg.ini", new_wrapper)) return {false, L"無法寫入 dxcfg.ini。"};
    return {};
}

Result apply_presentation(const fs::path& game, Settings& s, uint32_t refresh) {
    if (!s.windowed && s.saved_presentation.empty()) return {};   // nothing of ours to put back
    std::string wrapper;
    if (!read_bytes(game / L"dxcfg.ini", wrapper)) return {false, L"找不到 dxcfg.ini，無法設定視窗化。"};
    const std::regex presentation("presentation=([A-Za-z0-9_]+)"), display("display=([A-Za-z0-9_@.x-]+)");
    std::smatch p, d;
    if (!std::regex_search(wrapper, p, presentation)) return {false, L"dxcfg.ini 裡找不到 presentation，沒有修改視窗設定。"};
    const std::string now = p[1].str(), shown = std::regex_search(wrapper, d, display) ? d[1].str() : std::string();
    const auto first = std::regex_constants::format_first_only;
    const auto narrow = [](const std::wstring& v) { std::string out; for (const auto c : v) out.push_back(char(c)); return out; };   // ASCII by load_settings
    std::string updated = wrapper;
    if (s.windowed) {
        if (now != "windowed") { s.saved_presentation.assign(now.begin(), now.end()); s.saved_display.assign(shown.begin(), shown.end()); }
        updated = std::regex_replace(updated, presentation, "presentation=windowed", first);
        // A window needs a size: display=desktop (the Steam release) would fill the screen.
        if (!shown.empty() && !std::regex_match(shown, std::regex("\\d{1,5}x\\d{1,5}(@\\d{1,3})?"))) {
            const Resolution r = s.width ? Resolution{s.width, s.height} : game_resolution(game);
            if (!r.width || !r.height) return {false, L"讀不到遊戲的解析度，無法設定視窗大小。"};
            updated = std::regex_replace(updated, display, "display=" + std::to_string(r.width) + "x" + std::to_string(r.height) +
                                                           "@" + std::to_string(refresh ? refresh : 60), first);
        }
    } else {
        // Only undo our own change: a player who chose a window in the wrapper's own tool keeps it.
        if (now == "windowed") {
            updated = std::regex_replace(updated, presentation, "presentation=" + narrow(s.saved_presentation), first);
            if (!s.saved_display.empty()) updated = std::regex_replace(updated, display, "display=" + narrow(s.saved_display), first);
        }
        s.saved_presentation.clear(); s.saved_display.clear();
    }
    if (updated != wrapper && !write_bytes(game / L"dxcfg.ini", updated)) return {false, L"無法寫入 dxcfg.ini。"};
    return {};
}

bool is_proxy(const fs::path& file) {
    std::string bytes;
    return read_bytes(file, bytes) && bytes.find(U9CHT_PROXY_MARKER) != std::string::npos;
}

Result install_proxy(const fs::path& game, const fs::path& proxy) {
    if (!is_proxy(proxy)) return {false, L"找不到 U9ChtProxy.dll，或檔案不正確。"};
    const auto target = game / ProxyInstalledName;
    std::error_code ec;
    if (fs::exists(target, ec) && !is_proxy(target))
        return {false, L"遊戲資料夾裡已經有別的 dsound.dll（不是中文化程式放的），為了不覆蓋它，沒有啟動遊戲。"};
    if (!copy_verified(proxy, target, hash_or_empty(proxy))) return {false, L"無法把 dsound.dll 放進遊戲資料夾。"};
    return {};
}

bool remove_proxy(const fs::path& game) {
    const auto target = game / ProxyInstalledName;
    std::error_code ec;
    if (!fs::exists(target, ec)) return true;
    if (!is_proxy(target)) return true;   // the player's own file: not ours to remove
    return fs::remove(target, ec) && !ec;
}

Result recover(const fs::path& game, const fs::path& backup) {
    const auto receipt = backup / Receipt;
    std::error_code ec;
    if (!fs::is_regular_file(receipt, ec)) return {};
    std::wstring conflicts;
    bool restored_all = true;
    for (const auto& e : read_receipt(receipt)) {
        if (!safe_name(e.name)) { restored_all = false; conflicts += L" " + e.name; continue; }
        const auto target = game / L"static" / e.name;
        const auto now = hash_or_empty(target);
        if (now == e.original) continue;
        if (now == e.installed && copy_verified(backup / e.name, target, e.original)) continue;
        restored_all = false; conflicts += L" " + e.name;
    }
    if (!restored_all) return {false, L"無法還原這些遊戲檔案：" + conflicts + L"（備份保留在 " + backup.wstring() + L"）"};
    for (const auto& e : read_receipt(receipt)) if (safe_name(e.name)) fs::remove(backup / e.name, ec);
    fs::remove(receipt, ec);
    return {true, L"已還原遊戲原本的檔案。"};
}

Result install(const fs::path& game, const fs::path& data, const Manifest& m, const fs::path& backup) {
    if (!m.ok) return {false, L"中文化資料不完整（build-manifest.json）。"};
    std::error_code ec;
    if (fs::is_regular_file(backup / Receipt, ec)) return {false, L"上次的安裝還沒還原，請重新開啟本程式。"};
    for (const auto& [name, out] : m.output) {
        if (hash_or_empty(data / L"static" / name) != out) return {false, L"中文化資料損毀：" + name};
        if (hash_or_empty(game / L"static" / name) != m.source.at(name))
            return {false, L"遊戲檔案和中文化資料的版本不符：" + name};
    }
    fs::create_directories(backup, ec);
    if (ec) return {false, L"無法建立備份資料夾：" + backup.wstring()};
    for (const auto& [name, out] : m.output)
        if (!copy_verified(game / L"static" / name, backup / name, m.source.at(name)))
            return {false, L"備份失敗：" + name};
    {   // The receipt exists before the first original is replaced.
        std::ofstream r(backup / Receipt, std::ios::binary | std::ios::trunc);
        for (const auto& [name, out] : m.output)
            r << utf8(name) << '\t' << utf8(m.source.at(name)) << '\t' << utf8(out) << "\r\n";
        r.flush();
        if (!r) return {false, L"無法寫入安裝紀錄。"};
    }
    for (const auto& [name, out] : m.output) {
        if (!copy_verified(data / L"static" / name, game / L"static" / name, out)) {
            const auto undo = recover(game, backup);
            return {false, L"無法寫入遊戲資料夾（" + name + L"）。可能需要以系統管理員身分執行。" +
                           (undo.ok ? L"" : L"\n" + undo.message)};
        }
    }
    return {true, L"已套用中文化資料。"};
}
}
