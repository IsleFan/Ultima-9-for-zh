#pragma once
// Player reports (2026-11-27): after a game the front page reads that session's runtime log, notices a crash or
// translations the player flagged in game (translation_flag, key flag_translation), and offers to pack a report zip
// for a GitHub issue. The zip holds only the localization's own state: the log without the English fragments some
// events carry, the launcher log, the settings and the system version. UI-free; tested by tests/front_core_tests.cpp.
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace u9cht::front::report {
namespace fs = std::filesystem;

struct Flag { std::string key, text; };   // a store key ("text.flx:123") and its Chinese text, UTF-8
struct Session {
    bool crashed = false;          // an exception event, or the game ended with a code other than 0
    long long exit_code = 0;
    std::string where;             // module+offset of the first exception, e.g. "u9.exe+0x19CEAC"
    int flag_presses = 0;
    std::vector<Flag> flags;       // unique, in the order flagged
};
Session summarize(const std::string& jsonl);
// The log as it goes into a report: no glyph_cache_warm lines, and the text fields of flow_text, book_flow and
// file_probe emptied (they hold pieces of the game's English text or file names).
std::string trimmed_log(const std::string& jsonl);
// `text` with every occurrence of each folder (as UTF-8, also with JSON-escaped backslashes) replaced by `<folder>`.
std::string scrub(std::string text, const std::vector<fs::path>& folders);
// The flagged translations as a short UTF-8 text for the issue.
std::string flags_text(const Session& s);
// A ZIP archive (stored, no compression; UTF-8 names) of `files` = {name, bytes}.
bool write_zip(const fs::path& file, const std::vector<std::pair<std::string, std::string>>& files);
}
