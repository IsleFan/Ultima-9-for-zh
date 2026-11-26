#pragma once
// File redirect (authorized 2026-11-26): u9.exe opens its resources with CreateFileA (IAT slot 0x7400CC). When it
// opens static\bitmap16.flx for reading, the runtime opens <data>\redirect\bitmap16.flx instead, a copy whose
// signpost textures are in Chinese (docs/formats/U9-BITMAP16-NOTES.md). The game folder is never changed.
// Header-only and game-free: tested by tests/file_redirect_tests.cpp.
#include <cstdint>
#include <string_view>

namespace u9cht::file_redirect {
inline constexpr std::string_view Bitmap16 = "bitmap16.flx";
inline constexpr wchar_t RedirectFolder[] = L"redirect";

inline char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }
inline bool same(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (lower(a[i]) != lower(b[i])) return false;
    return true;
}
// True for "static\bitmap16.flx" in any letter case, with '\' or '/', relative or absolute
// ("C:\GOG Games\Ultima 9\static\BITMAP16.FLX", ".\static/bitmap16.flx").
inline bool is_bitmap16(std::string_view path) {
    const auto cut = path.find_last_of("\\/");
    if (cut == std::string_view::npos || !same(path.substr(cut + 1), Bitmap16)) return false;
    auto folder = path.substr(0, cut);
    const auto up = folder.find_last_of("\\/");
    return same(up == std::string_view::npos ? folder : folder.substr(up + 1), "static");
}
// Only plain reads are redirected: no write access, and a disposition that opens an existing file. u9.exe opens its
// resources with GENERIC_READ and OPEN_ALWAYS (CONFIRMED by the file probe, 2026-11-26); the redirect target exists,
// so OPEN_ALWAYS only opens it, as it does the game's own file.
inline constexpr uint32_t GenericWrite = 0x40000000, FileWriteData = 0x2, FileAppendData = 0x4, OpenExisting = 3,
                          OpenAlways = 4;
inline bool read_only(uint32_t access, uint32_t disposition) {
    return (access & (GenericWrite | FileWriteData | FileAppendData)) == 0 &&
           (disposition == OpenExisting || disposition == OpenAlways);
}
}
