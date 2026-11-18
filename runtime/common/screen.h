#pragma once
#include <cstdint>

namespace u9cht {
// Screen sizes the Chinese runtime draws at (2026-11-18; docs/research/U9_RESOLUTIONS.md). The game lays its
// interface out at 640x480 and scales it by width/640 and height/480; Chinese drawn straight onto the screen is
// rasterised at height/30 pixels (20 at 800x600, 40 at 1600x1200), rounded. Only 4:3 sizes: on a wider screen the
// game stretches its interface sideways while the Chinese glyphs would not be, so their cells would gap.
inline constexpr uint32_t MinScreenHeight = 480, MaxScreenHeight = 1536;
inline constexpr uint32_t screen_em(uint32_t width, uint32_t height) noexcept {
    if (!width || !height || uint64_t(width)*3 != uint64_t(height)*4) return 0;
    if (height < MinScreenHeight || height > MaxScreenHeight) return 0;
    return (height + 15) / 30;
}
// A length given in 1600x1200 units (the overlay layout), on a screen `height` pixels tall.
inline constexpr int32_t screen_units(int32_t value, uint32_t height) noexcept {
    return int32_t(int64_t(value)*int64_t(height)/1200);
}
static_assert(screen_em(800,600) == 20 && screen_em(1600,1200) == 40, "the two sizes verified before 2026-11-18");
static_assert(screen_em(1024,768) == 26 && screen_em(1280,960) == 32 && screen_em(640,480) == 16);
static_assert(screen_em(1920,1080) == 0 && screen_em(800,601) == 0 && screen_em(2048,1536) == 51 && screen_em(2400,1800) == 0);
static_assert(screen_units(401,600) == 200 && screen_units(401,1200) == 401 && screen_units(1177,768) == 753);
}
