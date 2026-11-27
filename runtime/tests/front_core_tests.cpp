// Front page core (U9ChtFront/front_core.h) on synthetic files only: no game asset is read.
#include "front_core.h"
#include "sign_patch.h"
#include "report.h"
#include "platform.h"
#include "protocol.h"
#include "version_manifest.h"
#include <fstream>
#include <iostream>

using namespace u9cht;
using namespace u9cht::front;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
void write(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary | std::ios::trunc) << data;
}
std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {});
}
std::string hex(const fs::path& p) { return utf8(sha256(p)); }
}

int main() {
    const auto root = fs::temp_directory_path() / L"u9cht-front-tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    {   // proxy dsound.dll: recognized by its marker, never replacing or removing a file that is not ours
        const auto game = root / L"proxy-game";
        const std::string ours = std::string("MZ...") + U9CHT_PROXY_MARKER + "...build A", newer = std::string("MZ") + U9CHT_PROXY_MARKER + " build B";
        write(root / L"U9ChtProxy.dll", ours);
        write(root / L"other.dll", "MZ somebody else's dsound wrapper");
        fs::create_directories(game);
        check(is_proxy(root / L"U9ChtProxy.dll") && !is_proxy(root / L"other.dll") && !is_proxy(root / L"absent.dll"), "marker");
        check(remove_proxy(game), "nothing to remove");
        check(install_proxy(game, root / L"U9ChtProxy.dll").ok && read(game / L"dsound.dll") == ours, "installed");
        write(root / L"U9ChtProxy.dll", newer);
        check(install_proxy(game, root / L"U9ChtProxy.dll").ok && read(game / L"dsound.dll") == newer, "a leftover of ours from another build is replaced");
        check(remove_proxy(game) && !fs::exists(game / L"dsound.dll"), "removed");
        write(game / L"dsound.dll", "MZ somebody else's dsound wrapper");
        check(!install_proxy(game, root / L"U9ChtProxy.dll").ok, "the player's own dsound.dll is not overwritten");
        check(remove_proxy(game) && read(game / L"dsound.dll") == "MZ somebody else's dsound wrapper", "and not removed");
        check(!install_proxy(game, root / L"other.dll").ok && !install_proxy(game, root / L"absent.dll").ok, "only our proxy is installed");
    }

    {   // JSON hash objects
        const std::string a(64, 'a'), b(64, 'b');
        const std::string json = "{\"x\": 1, \"source_game_sha256\": {\"text.flx\": \"" + a + "\", \"BOOKS-EN.FLX\": \"" + b +
                                 "\"}, \"runtime_plan_count\": 10083}";
        const auto m = json_hash_object(json, "source_game_sha256");
        check(m.size() == 2 && m.at(L"text.flx") == std::wstring(64, L'a'), "hash object parsed");
        check(json_hash_object(json, "missing").empty(), "missing key");
        check(json_hash_object("{\"k\": {\"a\": \"xyz\"}}", "k").empty(), "non-hash value rejected");
        check(json_hash_object("{\"k\": {\"a\": \"" + std::string(64, 'A') + "\"}}", "k").empty(), "uppercase hash rejected");
        write(root / L"m.json", "{\"source_game_sha256\": {\"text.flx\": \"" + a + "\"}, \"resource_output_sha256\": {\"text.flx\": \"" +
              b + "\"}, \"runtime_plan_count\": 10083}");
        const auto man = read_manifest(root / L"m.json");
        check(man.ok && man.plans == 10083, "manifest read");
        write(root / L"bad.json", "{\"source_game_sha256\": {\"a\": \"" + a + "\"}, \"resource_output_sha256\": {\"..\": \"" + b + "\"}}");
        check(!read_manifest(root / L"bad.json").ok, "unsafe or unmatched names rejected");
        check(!read_manifest(root / L"absent.json").ok, "absent manifest");
    }
    {   // Steam libraries and editions
        const std::string vdf = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n"
                                "\t\t\"label\"\t\t\"\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\Steam \\\"Lib\\\"\"\n\t}\n}\n";
        const auto libs = steam_library_paths(vdf);
        check(libs.size() == 2 && libs[0] == L"C:\\Program Files (x86)\\Steam" && libs[1] == L"D:\\Steam \"Lib\"", "vdf library paths");
        check(steam_library_paths("").empty() && steam_library_paths("\"path\" \"unterminated").empty(), "vdf malformed");
        check(std::wstring(exe_edition(ExeHash)) == L"GOG" && std::wstring(exe_edition(SteamExeHash)) == L"Steam" &&
              std::wstring(exe_edition(std::wstring(64, L'0'))).empty() && known_exe(SteamExeHash), "editions");
        write(root / L"ed" / L"u9.exe", "not the game");
        check(game_edition(root / L"ed").empty() && game_edition(root / L"none").empty(), "unsupported or missing exe");
    }
    {   // Settings round trip, defaults and clamping
        const auto ini = root / L"u9cht.ini", front = root / L"front.ini";
        auto d = load_settings(ini, front);
        check(d.features == DefaultFeatures && d.hover_delay_ms == DefaultHoverDelayMs && d.letter_spacing == 2, "defaults");
        Settings s = d;
        s.game_dir = L"C:\\遊戲\\Ultima 9"; s.font = L"D:\\字型\\a.ttf"; s.letter_spacing = 4; s.line_spacing = 7;
        check(!(d.features & FeatureSmartJump), "smart jump is off by default");
        s.features = FeatureWheelZoom | FeatureMovieSubtitles | FeatureSmartJump; s.hover_delay_ms = 400;
        save_settings(s, ini, front);
        const auto t = load_settings(ini, front);
        check(t.game_dir == s.game_dir && t.font == s.font && t.letter_spacing == 4 && t.line_spacing == 7 &&
              t.features == s.features && t.hover_delay_ms == 400, "settings round trip (non-ASCII paths)");
        write(ini, "[journal]\r\nletter_spacing = 99\r\n[features]\r\nhover_delay_ms = 5\r\nwheel_pages = 7\r\n");
        const auto c = load_settings(ini, front);
        check(c.letter_spacing == MaxJournalLetterSpacing && c.hover_delay_ms == MinHoverDelayMs &&
              (c.features & FeatureWheelPages), "clamped and invalid values");
    }
    {   // Resolution: only the three values change, every other byte is kept
        const auto game = root / L"res";
        write(game / L"Options.ini", "[Video]\r\nGameScreenWidth=800\r\nGameScreenHeight=600\r\nGameScreenDepth=32\r\n");
        write(game / L"dxcfg.ini", "[general]\r\ndisplay=800x600@120\r\npresentation=windowed\r\n");
        const auto r = game_resolution(game);
        check(r.width == 800 && r.height == 600, "resolution read");
        check(apply_resolution(game, {1600, 1200}).ok, "resolution applied");
        check(read(game / L"Options.ini") == "[Video]\r\nGameScreenWidth=1600\r\nGameScreenHeight=1200\r\nGameScreenDepth=32\r\n" &&
              read(game / L"dxcfg.ini") == "[general]\r\ndisplay=1600x1200@120\r\npresentation=windowed\r\n", "only sizes changed");
        check(apply_resolution(game, {0, 0}).ok && game_resolution(game).width == 1600, "zero keeps the setting");
        write(game / L"dxcfg.ini", "[general]\r\ndisplay=desktop\r\npresentation=application\r\n");   // Steam release
        check(apply_resolution(game, {800, 600}).ok && game_resolution(game).width == 800 &&
              read(game / L"dxcfg.ini") == "[general]\r\ndisplay=desktop\r\npresentation=application\r\n", "desktop wrapper left as is");
        write(game / L"Options.ini", "[Video]\r\nGameScreenDepth=32\r\n");
        check(!apply_resolution(game, {1600, 1200}).ok && read(game / L"Options.ini") == "[Video]\r\nGameScreenDepth=32\r\n",
              "Options.ini without sizes refused untouched");
        check(game_resolution(root / L"nowhere").width == 0, "missing Options.ini");
        const auto ini = root / L"r-u9cht.ini", front = root / L"r-front.ini";
        auto s = default_settings(); s.width = 1600; s.height = 1200;
        save_settings(s, ini, front);
        check(load_settings(ini, front).width == 1600, "resolution choice saved");
        check(supported({800, 600}) && supported({1024, 768}) && supported({1280, 960}) && supported({1600, 1200}) &&
              !supported({1920, 1080}) && !supported({640, 480}) && !supported({0, 0}), "supported sizes");
        write(front, "[display]\r\nwidth = 1234\r\nheight = 5\r\n");
        check(load_settings(ini, front).width == 0, "unoffered size ignored");
    }
    {   // Window or full screen: only presentation= (and a display= that names no size) change, and it can be undone
        const auto game = root / L"win";
        const std::string steam = "[general]\r\ndisplay=desktop\r\npresentation=application\r\naspect=enabled\r\n";
        write(game / L"Options.ini", "[Video]\r\nGameScreenWidth=1600\r\nGameScreenHeight=1200\r\n");
        write(game / L"dxcfg.ini", steam);
        auto s = default_settings();
        check(apply_presentation(game, s, 144).ok && read(game / L"dxcfg.ini") == steam && s.saved_presentation.empty(),
              "box clear and nothing saved: untouched");
        s.windowed = true;
        check(apply_presentation(game, s, 144).ok, "window applied");
        check(read(game / L"dxcfg.ini") == "[general]\r\ndisplay=1600x1200@144\r\npresentation=windowed\r\naspect=enabled\r\n",
              "desktop display gets the game's size");
        check(s.saved_presentation == L"application" && s.saved_display == L"desktop", "previous values remembered");
        check(apply_presentation(game, s, 60).ok && s.saved_presentation == L"application" && s.saved_display == L"desktop" &&
              read(game / L"dxcfg.ini") == "[general]\r\ndisplay=1600x1200@144\r\npresentation=windowed\r\naspect=enabled\r\n",
              "applying again changes nothing and keeps what was remembered");
        const auto ini = root / L"w-u9cht.ini", front = root / L"w-front.ini";
        s.dialogue_letter = 2; s.dialogue_line = 5;
        s.features |= FeatureClickSkip; s.features &= ~FeatureEscContents;
        save_settings(s, ini, front);
        auto t = load_settings(ini, front);
        check(t.windowed && t.saved_presentation == L"application" && t.saved_display == L"desktop" &&
              t.dialogue_letter == 2 && t.dialogue_line == 5, "window and dialogue settings survive a save");
        check((t.features & FeatureClickSkip) != 0 && (default_settings().features & FeatureClickSkip) != 0, "click skip saved; on by default");
        check((t.features & FeatureEscContents) == 0 && (default_settings().features & FeatureEscContents) != 0,
              "Esc contents switch saved off; on by default");
        t.windowed = false;
        check(apply_presentation(game, t, 60).ok && read(game / L"dxcfg.ini") == steam && t.saved_presentation.empty() && t.saved_display.empty(),
              "clearing the box restores the file byte for byte");
        // A wrapper that already names a size keeps it; the resolution step owns that value.
        write(game / L"dxcfg.ini", "[general]\r\ndisplay=800x600@120\r\npresentation=application\r\n");
        auto u = default_settings(); u.windowed = true;
        check(apply_presentation(game, u, 60).ok && read(game / L"dxcfg.ini") == "[general]\r\ndisplay=800x600@120\r\npresentation=windowed\r\n" &&
              u.saved_display == L"800x600@120", "a sized display is left alone");
        // The player's own window (not made by the front page) is not undone.
        auto own = default_settings();
        check(apply_presentation(game, own, 60).ok && read(game / L"dxcfg.ini") == "[general]\r\ndisplay=800x600@120\r\npresentation=windowed\r\n",
              "a window the front page did not make stays");
        // The chosen resolution wins over the game's current one; a missing size or key is refused untouched.
        write(game / L"dxcfg.ini", steam);
        auto chosen = default_settings(); chosen.windowed = true; chosen.width = 800; chosen.height = 600;
        check(apply_presentation(game, chosen, 0).ok && read(game / L"dxcfg.ini").find("display=800x600@60") != std::string::npos,
              "chosen resolution and default refresh");
        write(game / L"dxcfg.ini", steam); write(game / L"Options.ini", "[Video]\r\n");
        auto blind = default_settings(); blind.windowed = true;
        check(!apply_presentation(game, blind, 60).ok && read(game / L"dxcfg.ini") == steam, "no known size: refused untouched");
        write(game / L"dxcfg.ini", "[general]\r\ndisplay=desktop\r\n");
        check(!apply_presentation(game, blind, 60).ok && read(game / L"dxcfg.ini") == "[general]\r\ndisplay=desktop\r\n",
              "no presentation key: refused untouched");
        check(!apply_presentation(root / L"nowhere", blind, 60).ok, "missing dxcfg.ini");
        // Settings: defaults, clamping, and a tampered saved value is dropped.
        const auto d = default_settings();
        check(!d.windowed && d.dialogue_letter == 0 && d.dialogue_line == 0, "window and dialogue defaults");
        write(ini, "[dialogue]\r\nletter_spacing = 9\r\nline_spacing = 99\r\n");
        write(front, "[display]\r\nwindowed = 1\r\nsaved_presentation = a b\r\nsaved_display = desktop\r\n");
        const auto c = load_settings(ini, front);
        check(c.dialogue_letter == MaxDialogueLetterSpacing && c.dialogue_line == MaxDialogueLineSpacing, "dialogue values clamped");
        check(c.windowed && c.saved_presentation.empty() && c.saved_display.empty(), "unexpected saved value dropped");
    }
    {   // Install and recover on a synthetic game folder
        const auto game = root / L"game", data = root / L"data", backup = root / L"backup";
        write(game / L"static" / L"text.flx", "english text");
        write(game / L"static" / L"TYPENAME.FLX", "english names");
        write(data / L"static" / L"text.flx", "chinese text");
        write(data / L"static" / L"TYPENAME.FLX", "chinese names");
        const std::string manifest = "{\"source_game_sha256\": {\"text.flx\": \"" + hex(game / L"static/text.flx") +
            "\", \"TYPENAME.FLX\": \"" + hex(game / L"static/TYPENAME.FLX") + "\"}, \"resource_output_sha256\": {\"text.flx\": \"" +
            hex(data / L"static/text.flx") + "\", \"TYPENAME.FLX\": \"" + hex(data / L"static/TYPENAME.FLX") + "\"}}";
        write(data / L"build-manifest.json", manifest);
        const auto m = read_manifest(data / L"build-manifest.json");
        check(m.ok, "synthetic manifest");
        check(check_game(game, m) == GameStatus::NotFound, "no u9.exe");
        check(recover(game, backup).ok, "nothing to recover");

        auto r = install(game, data, m, backup);
        check(r.ok && read(game / L"static/text.flx") == "chinese text" && read(game / L"static/TYPENAME.FLX") == "chinese names",
              "install replaces both files");
        check(fs::is_regular_file(backup / L"installed.txt"), "receipt written");
        check(!install(game, data, m, backup).ok, "second install refused while a receipt exists");
        r = recover(game, backup);
        check(r.ok && read(game / L"static/text.flx") == "english text" && read(game / L"static/TYPENAME.FLX") == "english names",
              "recover restores originals");
        check(!fs::exists(backup / L"installed.txt") && !fs::exists(backup / L"text.flx"), "backup cleared");

        // A file changed by something else after install is never overwritten.
        check(install(game, data, m, backup).ok, "install again");
        write(game / L"static" / L"text.flx", "patched by someone else");
        r = recover(game, backup);
        check(!r.ok && read(game / L"static/text.flx") == "patched by someone else" &&
              read(game / L"static/TYPENAME.FLX") == "english names", "foreign change kept, other file restored");
        check(fs::is_regular_file(backup / L"installed.txt") && read(backup / L"text.flx") == "english text", "backup kept on conflict");
        write(game / L"static" / L"text.flx", "english text");
        check(recover(game, backup).ok && !fs::exists(backup / L"installed.txt"), "recover after manual fix");

        // Version mismatch and damaged data are refused before anything changes.
        write(game / L"static" / L"text.flx", "other version");
        r = install(game, data, m, backup);
        check(!r.ok && read(game / L"static/TYPENAME.FLX") == "english names" && !fs::exists(backup / L"installed.txt"),
              "source mismatch refused");
        write(game / L"static" / L"text.flx", "english text");
        write(data / L"static" / L"text.flx", "damaged");
        r = install(game, data, m, backup);
        check(!r.ok && read(game / L"static/text.flx") == "english text", "damaged data refused");
    }
    {   // A release: data/patches only, built from the player's originals (flx_patch.h)
        const auto game = root / L"pgame", data = root / L"pdata", backup = root / L"pbackup";
        write(game / L"static" / L"text.flx", "english text");
        const std::string chinese = "chinese text";   // "text" is copied from the original
        std::string patch = std::string("U9CP") + std::string("   ", 4);
        const auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) patch += char(v >> (8*i)); };
        u32(12); u32(uint32_t(chinese.size())); u32(2);
        patch += char(1); u32(8); patch += "chinese ";
        patch += char(0); u32(8); u32(4);
        write(data / L"patches" / L"text.flx.u9cp", patch);
        write(root / L"want.flx", chinese);
        write(data / L"build-manifest.json", "{\"source_game_sha256\": {\"text.flx\": \"" + hex(game / L"static/text.flx") +
              "\"}, \"resource_output_sha256\": {\"text.flx\": \"" + hex(root / L"want.flx") + "\"}}");
        const auto m = read_manifest(data / L"build-manifest.json");
        auto r = prepare_static(game, data, m, backup);
        check(r.ok && read(data / L"static/text.flx") == chinese && !r.message.empty(), "patch builds the Chinese file");
        r = prepare_static(game, data, m, backup);
        check(r.ok && r.message.empty(), "already built: nothing to do");
        check(install(game, data, m, backup).ok && read(game / L"static/text.flx") == chinese, "install from a release");
        fs::remove(data / L"static" / L"text.flx", ec);
        check(prepare_static(game, data, m, backup).ok && read(data / L"static/text.flx") == chinese,
              "while installed the original comes from the backup");
        check(recover(game, backup).ok && read(game / L"static/text.flx") == "english text", "recover after a release install");
        fs::remove(data / L"static" / L"text.flx", ec);
        write(game / L"static" / L"text.flx", "other version");
        check(!prepare_static(game, data, m, backup).ok && !fs::exists(data / L"static/text.flx"), "other game version refused");
        write(game / L"static" / L"text.flx", "english text");
        write(data / L"patches" / L"text.flx.u9cp", patch.substr(0, patch.size() - 1));
        check(!prepare_static(game, data, m, backup).ok && !fs::exists(data / L"static/text.flx"), "damaged patch refused");
        fs::remove(data / L"patches" / L"text.flx.u9cp", ec);
        check(!prepare_static(game, data, m, backup).ok, "missing patch refused");
    }
    {   // Signposts: prepare_redirect on a synthetic bitmap16.flx (no game art).
        const auto game = root / L"sign-game", data = root / L"sign-data";
        fs::create_directories(game / L"static", ec); fs::create_directories(data / L"patches", ec);
        auto put32 = [](std::string& s, uint32_t v) { for (int i = 0; i < 4; ++i) s += char(uint8_t(v >> (8 * i))); };
        auto board = [&](uint32_t seed) {   // tests/test_bitmap_flx.py synthetic_payload(8, 4, 2, seed)
            std::string px; uint32_t v = seed, w = 8, h = 4;
            for (int m = 0; m < 3; ++m) {
                for (uint32_t i = 0; i < w * h; ++i) { v = (v * 1103515245u + 12345u) & 0xFFFF; px += char(uint8_t(v)); px += char(uint8_t(v >> 8)); }
                w /= 2; h = h > 1 ? h / 2 : 1;
            }
            std::string frame; put32(frame, 0x60000400); put32(frame, 8); put32(frame, 4); put32(frame, 0); put32(frame, 0);
            for (uint32_t y = 0; y < 4; ++y) put32(frame, 0x14 + 16 + 16 * y);
            frame += px;
            std::string out; put32(out, 8 | 2 << 16); put32(out, 4); put32(out, 1); put32(out, 0x56000); put32(out, 0x18);
            put32(out, uint32_t(frame.size())); return out + frame;
        };
        const std::string base = board(1), sign = board(2);
        std::string flx(0x80 + 8 * 4, '\0');
        flx.replace(0, 4, "    "); flx[0x50] = 4; flx[0x54] = 2;
        auto slot = [&](int i, uint32_t off, uint32_t len) { for (int k = 0; k < 4; ++k) { flx[0x80 + 8 * i + k] = char(uint8_t(off >> (8 * k))); flx[0x84 + 8 * i + k] = char(uint8_t(len >> (8 * k))); } };
        const uint32_t at = uint32_t(flx.size());
        slot(1, at, uint32_t(base.size())); slot(3, at + uint32_t(base.size()), uint32_t(sign.size()));
        flx += base + sign;
        write(game / L"static" / L"bitmap16.flx", flx);
        std::string layer;
        for (uint32_t i = 0; i < 32; ++i) { layer += char(uint8_t(i * 11)); layer += char(uint8_t(i * 29)); layer += char(uint8_t(i * 53)); layer += char(i % 5 ? uint8_t(i * 37) : 0); }
        auto bin = [](const std::wstring& h) { std::string o; for (size_t i = 0; i + 1 < h.size(); i += 2) o += char(std::stoi(h.substr(i, 2), nullptr, 16)); return o; };
        auto hash = [](const std::string& s) { return sha256_bytes(s.data(), s.size()); };
        const std::wstring golden = L"5036293e837c1f8282353d26ce38750012e81d96bb9819ff427e64a16b62e8e5";   // test_bitmap_flx.py
        std::string patch = "U9CS"; put32(patch, 1); put32(patch, uint32_t(flx.size())); patch += bin(hash(flx));
        put32(patch, 1); patch += bin(hash(base)); put32(patch, 1);
        put32(patch, 3); patch += char(8); patch += char(0); patch += char(4); patch += char(0); patch += bin(hash(sign));
        patch += layer; patch += bin(golden);
        const auto made = u9cht::sign_patch::board(std::vector<uint8_t>(base.begin(), base.end()), std::vector<uint8_t>(sign.begin(), sign.end()),
                                                   std::vector<uint8_t>(layer.begin(), layer.end()));
        check(made && sha256_bytes(made->data(), made->size()) == golden, "golden board");
        std::string expected = flx;
        if (made) expected.replace(at + base.size(), made->size(), std::string(made->begin(), made->end()));
        write(data / L"patches" / L"bitmap16.u9cs", patch + bin(hash(expected)));
        auto r = prepare_redirect(game, data);
        check(r.ok && r.message == L"已產生中文路標。" && read(data / L"redirect/bitmap16.flx") == expected, "redirect copy made");
        check(read(game / L"static" / L"bitmap16.flx") == flx, "game file untouched");
        check(prepare_redirect(game, data).message.empty(), "second start reuses the copy");
        write(data / L"patches" / L"bitmap16.u9cs", patch + std::string(32, char(0)));
        r = prepare_redirect(game, data);
        check(r.ok && r.message.find(L"路標維持英文") == 0 && !fs::exists(data / L"redirect/bitmap16.flx"),
              "wrong result hash: copy removed, boards stay English, still ok");
        write(data / L"patches" / L"bitmap16.u9cs", patch + bin(hash(expected)));
        write(game / L"static" / L"bitmap16.flx", flx.substr(0, flx.size() - 1));
        check(prepare_redirect(game, data).message.find(L"版本不符") != std::wstring::npos, "other game file: English");
        fs::remove(data / L"patches" / L"bitmap16.u9cs", ec);
        check(prepare_redirect(game, data).ok && prepare_redirect(game, data).message.empty(), "no patch: nothing to do");
    }
    {   // Player reports (report.h), on synthetic log lines.
        namespace rp = u9cht::front::report;
        const std::string log =
            "{\"event\":\"runtime_ready\",\"pid\":1}\r\n"
            "{\"event\":\"glyph_cache_warm\",\"pid\":1,\"scalar\":20013}\r\n"
            "{\"event\":\"flow_text\",\"pid\":1,\"length\":5,\"head\":\"Hello \\\"x\\\"\"}\r\n"
            "{\"event\":\"translation_flag\",\"pid\":1,\"number\":1,\"count\":2,\"items\":[{\"key\":\"text.flx:12\",\"text\":\"\xE4\xBD\xA0\xE5\xA5\xBD\\\"\"},{\"key\":\"TYPENAME.FLX:5\",\"text\":\"\xE5\x8A\x8D\"}]}\r\n"
            "{\"event\":\"translation_flag\",\"pid\":1,\"number\":2,\"count\":1,\"items\":[{\"key\":\"text.flx:12\",\"text\":\"\xE4\xBD\xA0\xE5\xA5\xBD\\\"\"}]}\r\n"
            "{\"event\":\"exception\",\"pid\":1,\"code\":3221225477,\"module\":\"u9.exe+0x19CEAC\"}\r\n"
            "{\"event\":\"game_exited\",\"pid\":2,\"exit_code\":3221226525}\r\n";
        const auto s = rp::summarize(log);
        check(s.crashed && s.where == "u9.exe+0x19CEAC" && s.exit_code == 3221226525LL, "crash found");
        check(s.flag_presses == 2 && s.flags.size() == 2 && s.flags[0].key == "text.flx:12" &&
              s.flags[0].text == "\xE4\xBD\xA0\xE5\xA5\xBD\"" && s.flags[1].key == "TYPENAME.FLX:5", "flags found, unique");
        check(!rp::summarize("{\"event\":\"game_exited\",\"exit_code\":0}\r\n").crashed, "clean exit");
        const auto t = rp::trimmed_log(log);
        check(t.find("glyph_cache_warm") == std::string::npos && t.find("Hello") == std::string::npos &&
              t.find("\"head\":\"\"") != std::string::npos && t.find("translation_flag") != std::string::npos, "trimmed log");
        {   // a long array becomes "<omitted>", short values stay
            const std::string big = "{\"event\":\"runtime_counters\",\"frames\":5,\"entries\":[" + std::string(1200, '1') +
                                    "],\"texture\":{\"a\":[1,2]},\"s\":\"x]\"}\r\n";
            check(rp::trimmed_log(big) == "{\"event\":\"runtime_counters\",\"frames\":5,\"entries\":\"<omitted>\",\"texture\":{\"a\":[1,2]},\"s\":\"x]\"}\r\n",
                  "big values omitted");
            // two big values on one line (runtime_counters has entries and lifecycle)
            const std::string two = "{\"entries\":[" + std::string(1100, '1') + "],\"k\":\"v\",\"lifecycle\":[{\"seq\":1}" +
                                    std::string(1100, ' ') + "]}\r\n";
            check(rp::trimmed_log(two) == "{\"entries\":\"<omitted>\",\"k\":\"v\",\"lifecycle\":\"<omitted>\"}\r\n", "two big values omitted");
        }
        check(rp::scrub("a C:\\\\Games\\\\U9\\\\static b C:\\Games\\U9 c", {fs::path(L"C:\\Games\\U9")}) == "a <folder>\\\\static b <folder> c", "scrub");
        check(rp::flags_text(s).find("text.flx:12\t") != std::string::npos, "flags text");
        const auto zip = root / L"report.zip";
        check(rp::write_zip(zip, {{"記錄.jsonl", t}, {"a.txt", "abc"}}), "zip written");
        const auto z = read(zip);
        check(z.size() > 30 && z.compare(0, 4, "PK\x03\x04") == 0 && z.find("abc") != std::string::npos &&
              z.compare(z.size() - 22, 4, "PK\x05\x06") == 0 && uint8_t(z[z.size() - 12]) == 2, "zip structure");
        // CRC-32 of "abc" is 0x352441C2; it sits in the second local header.
        check(z.find(std::string("\xC2\x41\x24\x35", 4)) != std::string::npos, "zip crc");
    }
    fs::remove_all(root, ec);
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "front core tests passed\n";
    return 0;
}
