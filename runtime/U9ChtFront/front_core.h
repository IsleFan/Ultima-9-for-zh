#pragma once
// The release's front page (U9Cht.exe): settings, finding the game, and installing the Chinese resources
// for one play session only. Everything here is UI-free and tested by tests/front_core_tests.cpp.
//
// Install model (same as the developer session tool, scripts/run_p8_0_session.py): before the game starts
// the four built FLX files replace the player's originals in <game>/static, after a verified backup and a
// receipt written BEFORE the first replacement; when the game ends they are put back and verified. A
// receipt left by a crash or power loss is resolved by recover() the next time the page starts.
#include <cstdint>
#include <filesystem>
#include <map>
#include "screen.h"
#include <string>
#include <vector>

namespace u9cht::front {
namespace fs = std::filesystem;

struct Settings {
    std::wstring game_dir;          // front.ini [game] dir
    std::wstring font;              // u9cht.ini [font] file ("" = the release's default font)
    uint32_t letter_spacing = 2, line_spacing = 3;
    uint32_t features = 0;          // protocol.h Feature* bits
    uint32_t hover_delay_ms = 0;
    uint32_t width = 0, height = 0;  // front.ini [display]; 0 = keep the game's own setting
    uint32_t dialogue_letter = 0, dialogue_line = 0;   // u9cht.ini [dialogue]
    // front.ini [display]: run the game in a window. saved_* hold what dxcfg.ini said before the front page
    // switched it to a window, so that clearing the box puts it back; empty = the front page changed nothing.
    bool windowed = false;
    std::wstring saved_presentation, saved_display;
};
struct Resolution { uint32_t width = 0, height = 0; };
// The sizes offered on the front page (2026-11-18): 4:3 sizes the runtime draws Chinese at (common/screen.h,
// glyphs height/30 px). 800x600 and 1600x1200 were verified in game first; 1024x768 and 1280x960 ran the game at the
// front page on 2026-11-12, when the runtime still ended it safely at those sizes (error 18).
inline constexpr Resolution Resolutions[] = {{800, 600}, {1024, 768}, {1280, 960}, {1600, 1200}};
static_assert([] { for (const auto& r : Resolutions) if (!screen_em(r.width, r.height)) return false; return true; }(),
              "every offered size must be one the runtime draws at");
inline bool supported(Resolution r) {
    for (const auto& s : Resolutions) if (s.width == r.width && s.height == r.height) return true;
    return false;
}
// GameScreenWidth/Height in Options.ini ({0,0} when absent).
Resolution game_resolution(const fs::path& game);
// Sets Options.ini GameScreenWidth/Height and dxcfg.ini display=WxH (its @refresh is kept). Every other byte
// of both files stays as it is. The change is persistent, like changing it in the game's own options.
struct Result;
Result apply_resolution(const fs::path& game, Resolution r);
// dxcfg.ini presentation= (and display= when it names no size): a window when s.windowed, otherwise whatever it
// was before the front page made it a window. `refresh` is used only when a display size has to be written.
// Updates s.saved_*; the caller saves the settings. Every other byte of dxcfg.ini stays as it is.
Result apply_presentation(const fs::path& game, Settings& s, uint32_t refresh);
Settings default_settings();
// Missing files or keys keep the defaults; out-of-range values are clamped.
Settings load_settings(const fs::path& launcher_ini, const fs::path& front_ini);
// UTF-16LE with BOM, as GetPrivateProfile*W reads it (font paths may hold Chinese).
void save_settings(const Settings& s, const fs::path& launcher_ini, const fs::path& front_ini);

struct Manifest {
    bool ok = false;
    std::map<std::wstring, std::wstring> source, output;   // static file name -> SHA-256 (lowercase hex)
    uint64_t plans = 0;
};
Manifest read_manifest(const fs::path& build_manifest_json);
// Exposed for tests: the {"name": "hex", ...} object after "key":, or empty when absent/malformed.
std::map<std::wstring, std::wstring> json_hash_object(const std::string& json, const std::string& key);

enum class GameStatus { Ok, NotFound, ExeMismatch, WrapperMismatch, ResourceMismatch };
// ResourceMismatch: a static file matches neither the build's source nor its output.
GameStatus check_game(const fs::path& dir, const Manifest& m, std::wstring* detail = nullptr);
// Registry (GOG, Steam libraries, uninstall entries) and common folders; folders holding u9.exe, supported
// editions first.
std::vector<fs::path> find_game_candidates();
// "GOG", "Steam" or "" (no u9.exe, or an executable this release does not support).
std::wstring game_edition(const fs::path& dir);
// Library folders listed in a Steam libraryfolders.vdf text (exposed for tests).
std::vector<std::wstring> steam_library_paths(const std::string& vdf);

struct Result { bool ok = true; std::wstring message; };
// Whole-file read, and a write through a temporary file that then replaces the target.
bool read_bytes(const fs::path& p, std::string& out);
bool write_bytes(const fs::path& p, const std::string& data);
// Undo an install whose receipt is in `backup`. Files changed by anything else are left alone and reported.
Result recover(const fs::path& game, const fs::path& backup);
// A release ships no FLX (they hold the game's English text) but data/patches/<name>.u9cp (flx_patch.h). Builds
// data/static/<name> from the player's original, taken from the game folder or, while an install is still in
// place, from `backup`, and checks it against the manifest. Files already there with the right hash are kept.
Result prepare_static(const fs::path& game, const fs::path& data, const Manifest& m, const fs::path& backup);
// Signposts (docs/formats/U9-BITMAP16-NOTES.md): with <data>/patches/bitmap16.u9cs, makes
// <data>/redirect/bitmap16.flx from the game's own file once (a stamp file holds the result hash). Always ok: on any
// mismatch the copy is removed, the boards stay English and the message says why. The game folder is not changed.
Result prepare_redirect(const fs::path& game, const fs::path& data);
// Calls prepare_static first, then prepare_redirect.
Result install(const fs::path& game, const fs::path& data, const Manifest& m, const fs::path& backup);

// Proxy loading (protocol.h): U9ChtProxy.dll is copied to <game>/dsound.dll for one session. A dsound.dll already
// there is replaced or removed only when it holds the proxy's marker, i.e. it is a copy of ours from any build;
// anything else belongs to the player and is refused / left alone.
bool is_proxy(const fs::path& file);
Result install_proxy(const fs::path& game, const fs::path& proxy);
// True when no proxy of ours is left in the game folder afterwards.
bool remove_proxy(const fs::path& game);
}
