#pragma once
// Release patches (2026-11-18; docs/release/PATCH_FORMAT.md). A release carries no game file and no file that holds
// the game's English text. For each FLX it carries a patch that turns the player's own original file into the
// Chinese one: runs copied from the original plus the bytes this project wrote. Made by u9cht/l10n/patch.py, applied
// by the front page. The caller checks the SHA-256 of the input and of the result against the build manifest.
//
// Layout, little-endian:
//   "U9CP" | u32 version (1) | u32 source size | u32 target size | u32 op count
//   ops: u8 kind, then kind 0 (copy): u32 source offset, u32 length
//                      kind 1 (add):  u32 length, that many bytes
//                      kind 2 (widen): u32 source offset, u32 length; each source byte becomes a UTF-16LE unit
//                      (byte, 0). The build re-encodes some original ASCII passages, such as the Gargish quoted in
//                      books, as UTF-16LE; this keeps them out of the patch too.
// The ops must produce exactly `target size` bytes and every copy must stay inside the source.
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace u9cht::patch {
inline constexpr uint32_t Version = 1;
inline constexpr uint32_t MaxSize = 64u << 20;   // FLX text files are below 1 MB

namespace detail {
struct Reader {
    const uint8_t* p; size_t n, at = 0;
    bool u32(uint32_t& v) {
        if (n - at < 4) return false;
        v = uint32_t(p[at]) | uint32_t(p[at+1]) << 8 | uint32_t(p[at+2]) << 16 | uint32_t(p[at+3]) << 24;
        at += 4; return true;
    }
    bool u8(uint8_t& v) { if (at >= n) return false; v = p[at++]; return true; }
};
}

// The patched file, or nothing with *error set.
inline std::optional<std::vector<uint8_t>> apply(const std::vector<uint8_t>& source, const std::vector<uint8_t>& patch,
                                                 std::string* error = nullptr) {
    const auto fail = [&](const char* why) { if (error) *error = why; return std::optional<std::vector<uint8_t>>{}; };
    detail::Reader r{patch.data(), patch.size()};
    if (patch.size() < 4 || std::memcmp(patch.data(), "U9CP", 4) != 0) return fail("not a U9CP patch");
    r.at = 4;
    uint32_t version = 0, source_size = 0, target_size = 0, ops = 0;
    if (!r.u32(version) || !r.u32(source_size) || !r.u32(target_size) || !r.u32(ops)) return fail("truncated header");
    if (version != Version) return fail("unsupported patch version");
    if (source_size != source.size()) return fail("source size differs");
    if (target_size > MaxSize) return fail("target too large");
    std::vector<uint8_t> out;
    out.reserve(target_size);
    for (uint32_t i = 0; i < ops; ++i) {
        uint8_t kind = 0; uint32_t a = 0, b = 0;
        if (!r.u8(kind)) return fail("truncated op");
        if (kind == 0) {
            if (!r.u32(a) || !r.u32(b)) return fail("truncated copy");
            if (a > source.size() || b > source.size() - a) return fail("copy outside the source");
            if (b > target_size - out.size()) return fail("output too long");
            out.insert(out.end(), source.begin() + a, source.begin() + a + b);
        } else if (kind == 2) {
            if (!r.u32(a) || !r.u32(b)) return fail("truncated widen");
            if (a > source.size() || b > source.size() - a) return fail("widen outside the source");
            if (b > (target_size - out.size()) / 2) return fail("output too long");
            for (uint32_t k = 0; k < b; ++k) { out.push_back(source[a+k]); out.push_back(0); }
        } else if (kind == 1) {
            if (!r.u32(a)) return fail("truncated add");
            if (a > r.n - r.at) return fail("add runs past the patch");
            if (a > target_size - out.size()) return fail("output too long");
            out.insert(out.end(), patch.begin() + ptrdiff_t(r.at), patch.begin() + ptrdiff_t(r.at + a));
            r.at += a;
        } else return fail("unknown op");
    }
    if (r.at != patch.size()) return fail("trailing bytes");
    if (out.size() != target_size) return fail("output size differs");
    return out;
}
}
