#pragma once
// Signpost patches (U9CS, 2026-11-26; docs/formats/U9-BITMAP16-NOTES.md "Release patch"). A release carries no game
// art: for each place-name board it carries a text layer (64x32 RGBA, this project's drawing) and the hashes to check.
// The front page lays each layer over the player's own blank board (bitmap16.flx entry 3381), makes the mipmaps again
// and writes the boards into a copy of the player's file, <data>\redirect\bitmap16.flx (file_redirect.h).
// Pure logic, no file access; the same arithmetic as u9cht/bitmap_flx.py (overlay, downsample, to_rgb, from_rgb).
//
// Layout, little-endian:
//   "U9CS" | u32 version (1) | u32 source size | 32 source SHA-256 | u32 base entry | 32 base SHA-256 | u32 count
//   count x { u32 entry | u16 width | u16 height | 32 original SHA-256 | width*height*4 RGBA | 32 board SHA-256 }
//   32 result SHA-256 (the whole patched file)
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace u9cht::sign_patch {
inline constexpr uint32_t Version = 1, MaxBoards = 256, FlagsRgb565 = 0x60000400, FrameHeader = 0x14;
using Hash = std::array<uint8_t, 32>;

struct Board { uint32_t entry = 0; uint16_t width = 0, height = 0; Hash original{}, result{}; std::vector<uint8_t> rgba; };
struct Patch {
    uint32_t source_size = 0, base_entry = 0;
    Hash source{}, base{}, result{};
    std::vector<Board> boards;
};

namespace detail {
inline uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
inline void put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
}

inline std::optional<Patch> parse(const std::vector<uint8_t>& d, std::string* error = nullptr) {
    const auto fail = [&](const char* why) { if (error) *error = why; return std::optional<Patch>{}; };
    using detail::u32;
    if (d.size() < 4 + 8 + 32 + 4 + 32 + 4 + 32 || d[0] != 'U' || d[1] != '9' || d[2] != 'C' || d[3] != 'S') return fail("not a U9CS patch");
    if (u32(&d[4]) != Version) return fail("unknown U9CS version");
    Patch p;
    size_t at = 8;
    p.source_size = u32(&d[at]); at += 4;
    std::copy_n(&d[at], 32, p.source.begin()); at += 32;
    p.base_entry = u32(&d[at]); at += 4;
    std::copy_n(&d[at], 32, p.base.begin()); at += 32;
    const uint32_t count = u32(&d[at]); at += 4;
    if (count > MaxBoards) return fail("too many boards");
    for (uint32_t i = 0; i < count; ++i) {
        if (d.size() - at < 8 + 32) return fail("U9CS cut short");
        Board b;
        b.entry = u32(&d[at]); b.width = detail::u16(&d[at + 4]); b.height = detail::u16(&d[at + 6]); at += 8;
        std::copy_n(&d[at], 32, b.original.begin()); at += 32;
        const size_t n = size_t(b.width) * b.height * 4;
        if (!b.width || !b.height || b.width > 256 || b.height > 256 || d.size() - at < n + 32) return fail("U9CS board cut short");
        b.rgba.assign(d.begin() + std::ptrdiff_t(at), d.begin() + std::ptrdiff_t(at + n)); at += n;
        std::copy_n(&d[at], 32, b.result.begin()); at += 32;
        p.boards.push_back(std::move(b));
    }
    if (d.size() - at != 32) return fail("U9CS has the wrong length");
    std::copy_n(&d[at], 32, p.result.begin());
    return p;
}

// RGB565 <-> 8-bit RGB, rounded as u9cht/bitmap_flx.py does (to_rgb, from_rgb).
inline std::array<int, 3> rgb(uint16_t v) {
    const int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    return {(r * 255 + 15) / 31, (g * 255 + 31) / 63, (b * 255 + 15) / 31};
}
inline uint16_t pack(int r, int g, int b) {
    return uint16_t(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
}

struct Layout { uint32_t width = 0, height = 0, levels = 0; size_t pixels = 0; };
// The single-frame RGB565 layout with mipmaps (bitmap_flx.parse_texture); anything else is refused.
inline std::optional<Layout> layout(const std::vector<uint8_t>& e) {
    using detail::u32;
    if (e.size() < 0x18 + FrameHeader) return std::nullopt;
    Layout l;
    const uint32_t word0 = u32(&e[0]);
    l.width = word0 & 0xFFFF; l.levels = (word0 >> 16) + 1; l.height = u32(&e[4]);
    if (u32(&e[8]) != 1 || u32(&e[0x10]) != 0x18 || size_t(u32(&e[0x14])) + 0x18 != e.size()) return std::nullopt;
    if (u32(&e[0x18]) != FlagsRgb565 || u32(&e[0x1C]) != l.width || u32(&e[0x20]) != l.height) return std::nullopt;
    if (!l.width || !l.height || l.width > 1024 || l.height > 1024 || l.levels > 16) return std::nullopt;
    const size_t rows = 0x18 + FrameHeader;
    if (e.size() < rows + 4 * size_t(l.height)) return std::nullopt;
    for (uint32_t y = 0; y < l.height; ++y)
        if (u32(&e[rows + 4 * y]) != FrameHeader + 4 * l.height + 2 * l.width * y) return std::nullopt;
    l.pixels = rows + 4 * size_t(l.height);
    size_t total = 0; uint32_t w = l.width, h = l.height;
    for (uint32_t i = 0; i < l.levels; ++i) { total += size_t(w) * h * 2; w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1; }
    if (l.pixels + total != e.size()) return std::nullopt;
    return l;
}

// The new board: `target` with its image replaced by `rgba` laid over `base`'s image, mipmaps made again.
inline std::optional<std::vector<uint8_t>> board(const std::vector<uint8_t>& base, const std::vector<uint8_t>& target,
                                                 const std::vector<uint8_t>& rgba, std::string* error = nullptr) {
    const auto fail = [&](const char* why) { if (error) *error = why; return std::optional<std::vector<uint8_t>>{}; };
    const auto lb = layout(base), lt = layout(target);
    if (!lb || !lt) return fail("board layout not supported");
    if (lb->width != lt->width || lb->height != lt->height || lb->levels != lt->levels) return fail("base and target differ");
    const size_t n = size_t(lt->width) * lt->height;
    if (rgba.size() != n * 4) return fail("layer size does not match");
    std::vector<uint16_t> level(n);
    for (size_t i = 0; i < n; ++i) {
        const uint16_t v = detail::u16(&base[lb->pixels + 2 * i]);
        const int a = rgba[4 * i + 3];
        if (!a) { level[i] = v; continue; }
        const auto d = rgb(v);
        int c[3];
        for (int k = 0; k < 3; ++k) c[k] = (rgba[4 * i + size_t(k)] * a + d[size_t(k)] * (255 - a) + 127) / 255;
        level[i] = pack(c[0], c[1], c[2]);
    }
    std::vector<uint8_t> out = target;
    size_t at = lt->pixels;
    uint32_t w = lt->width, h = lt->height;
    for (uint32_t li = 0; li < lt->levels; ++li) {
        for (size_t i = 0; i < level.size(); ++i) detail::put16(&out[at + 2 * i], level[i]);
        at += level.size() * 2;
        if (li + 1 == lt->levels) break;
        const uint32_t nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
        std::vector<uint16_t> next(size_t(nw) * nh);
        for (uint32_t y = 0; y < nh; ++y)
            for (uint32_t x = 0; x < nw; ++x) {
                int sum[3]{};
                for (uint32_t dy = 0; dy < 2; ++dy)
                    for (uint32_t dx = 0; dx < 2; ++dx) {
                        const uint32_t sy = std::min(h - 1, 2 * y + dy), sx = std::min(w - 1, 2 * x + dx);
                        const uint16_t v = level[size_t(sy) * w + sx];
                        sum[0] += (v >> 11) & 31; sum[1] += (v >> 5) & 63; sum[2] += v & 31;
                    }
                next[size_t(y) * nw + x] = uint16_t(((sum[0] + 2) / 4) << 11 | ((sum[1] + 2) / 4) << 5 | ((sum[2] + 2) / 4));
            }
        level = std::move(next); w = nw; h = nh;
    }
    return out;
}
}
