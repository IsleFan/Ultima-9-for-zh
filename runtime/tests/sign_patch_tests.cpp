// sign_patch.h on synthetic boards (no game art). The golden hash is the one tests/test_bitmap_flx.py checks for the
// same input, so the front page and the Python builder make the same bytes.
#include "sign_patch.h"
#include "platform.h"
#include <iostream>

using namespace u9cht;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++failures; } }
void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back(uint8_t(x >> (8 * i))); }
void put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8)); }

// tests/test_bitmap_flx.py synthetic_payload(w, h, mips, seed)
std::vector<uint8_t> synthetic(uint32_t w, uint32_t h, uint32_t mips, uint32_t seed) {
    std::vector<uint8_t> pixels; uint32_t v = seed, lw = w, lh = h;
    for (uint32_t m = 0; m <= mips; ++m) {
        for (uint32_t i = 0; i < lw * lh; ++i) { v = (v * 1103515245u + 12345u) & 0xFFFF; put16(pixels, uint16_t(v)); }
        lw = lw > 1 ? lw / 2 : 1; lh = lh > 1 ? lh / 2 : 1;
    }
    std::vector<uint8_t> frame;
    put32(frame, sign_patch::FlagsRgb565); put32(frame, w); put32(frame, h); put32(frame, 0); put32(frame, 0);
    for (uint32_t y = 0; y < h; ++y) put32(frame, sign_patch::FrameHeader + 4 * h + 2 * w * y);
    frame.insert(frame.end(), pixels.begin(), pixels.end());
    std::vector<uint8_t> out;
    put32(out, w | mips << 16); put32(out, h); put32(out, 1); put32(out, 0x56000);
    put32(out, 0x18); put32(out, uint32_t(frame.size()));
    out.insert(out.end(), frame.begin(), frame.end());
    return out;
}
// synthetic_layer(w, h)
std::vector<uint8_t> layer(uint32_t w, uint32_t h) {
    std::vector<uint8_t> out;
    for (uint32_t i = 0; i < w * h; ++i) {
        out.push_back(uint8_t(i * 11 % 256)); out.push_back(uint8_t(i * 29 % 256)); out.push_back(uint8_t(i * 53 % 256));
        out.push_back(uint8_t(i % 5 == 0 ? 0 : i * 37 % 256));
    }
    return out;
}
}

int main() {
    const auto base = synthetic(8, 4, 2, 1), target = synthetic(8, 4, 2, 2);
    check(sign_patch::layout(base).has_value(), "synthetic board is the supported layout");
    check(synthetic(64, 32, 3, 1).size() == 5612, "64x32 board is the signpost size");
    std::string why;
    const auto made = sign_patch::board(base, target, layer(8, 4), &why);
    check(made.has_value() && made->size() == target.size(), "board made");
    if (made) check(sha256_bytes(made->data(), made->size()) == L"5036293e837c1f8282353d26ce38750012e81d96bb9819ff427e64a16b62e8e5",
                    "same bytes as u9cht/bitmap_flx.py (golden)");
    check(!sign_patch::board(base, synthetic(4, 4, 2, 2), layer(8, 4)), "size mismatch refused");
    check(!sign_patch::board(base, target, layer(4, 4)), "layer size refused");
    auto bad = base; bad[0x19] = 0x05;   // frame flags 0x60000500 (ARGB1555)
    check(!sign_patch::board(bad, target, layer(8, 4)), "other layout refused");
    check(sign_patch::rgb(0xFFFF) == std::array<int, 3>{255, 255, 255} && sign_patch::pack(255, 255, 255) == 0xFFFF, "white");
    for (const uint16_t v : std::initializer_list<uint16_t>{0x0000, 0x1234, 0xF800, 0x07E0, 0x001F, 0xABCD}) {
        const auto c = sign_patch::rgb(v);
        check(sign_patch::pack(c[0], c[1], c[2]) == v, "565 round trip");
    }
    {   // parse
        std::vector<uint8_t> p{'U', '9', 'C', 'S'};
        put32(p, 1); put32(p, 1000); p.insert(p.end(), 32, 0xAA); put32(p, 3381); p.insert(p.end(), 32, 0xBB); put32(p, 1);
        put32(p, 4472); put16(p, 8); put16(p, 4); p.insert(p.end(), 32, 0xCC);
        const auto l = layer(8, 4); p.insert(p.end(), l.begin(), l.end()); p.insert(p.end(), 32, 0xDD);
        p.insert(p.end(), 32, 0xEE);
        const auto parsed = sign_patch::parse(p);
        check(parsed && parsed->source_size == 1000 && parsed->base_entry == 3381 && parsed->boards.size() == 1 &&
              parsed->boards[0].entry == 4472 && parsed->boards[0].rgba == l && parsed->result[0] == 0xEE, "parse");
        auto shorter = p; shorter.pop_back();
        check(!sign_patch::parse(shorter), "short refused");
        auto longer = p; longer.push_back(0);
        check(!sign_patch::parse(longer), "long refused");
        auto version = p; version[4] = 2;
        check(!sign_patch::parse(version), "version refused");
    }
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "sign patch tests passed\n";
    return 0;
}
