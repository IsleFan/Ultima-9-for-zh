#pragma once
// Draws a frame's glyph commands through the game's Direct3D 7 device instead of locking the back buffer
// (docs/research/U9_PERFORMANCE.md: the Lock costs 7-10 ms a frame whatever its rectangle; user authorization
// 2026-11-16, AGENTS section 14 C). On by default; the developer marker gpu_overlay.off forces the Lock path,
// which is also the fallback whenever a step here fails.
//
// Glyph coverage goes once into an ARGB atlas texture. Each command becomes one screen-space quad whose colour is
// the command's and whose alpha is the atlas texel: the same dest = rgb*a + dest*(1-a) as blend().
#include "compositor.h"
#include <array>
#include <cstdint>

namespace u9cht::gpu {
// MaxQuads: a glyph under the cursor is cut into strips (emit), so a frame needs more quads than commands.
inline constexpr uint32_t AtlasSide = 1024, AtlasSlots = 4096, MaxQuads = 4096;

struct Slot { const Glyph* glyph = nullptr; uint16_t x = 0, y = 0, width = 0, height = 0; };

// Shelf packer with an open-addressed pointer table. No allocation.
class Atlas {
    std::array<Slot,AtlasSlots> slots_{};
    uint32_t used_ = 0, shelf_x_ = 0, shelf_y_ = 0, shelf_h_ = 0;
    static uint32_t home(const Glyph* g) noexcept {
        return uint32_t((reinterpret_cast<uintptr_t>(g)>>4)*0x9E3779B1U)>>20; // 12 bits
    }
public:
    void clear() noexcept { slots_.fill({}); used_ = shelf_x_ = shelf_y_ = shelf_h_ = 0; }
    uint32_t size() const noexcept { return used_; }
    // The glyph's slot, or null. A slot whose size no longer matches the glyph is treated as absent.
    const Slot* find(const Glyph* g) const noexcept {
        for (uint32_t i = home(g), n = 0; n < AtlasSlots; i = (i+1)&(AtlasSlots-1), ++n) {
            const auto& s = slots_[i];
            if (!s.glyph) return nullptr;
            if (s.glyph == g) return s.width == g->width && s.height == g->height ? &s : nullptr;
        }
        return nullptr;
    }
    // Reserves room for a glyph not yet present (one texel of padding). Null when it does not fit: the caller
    // clears the atlas and starts over, or gives the frame up.
    const Slot* place(const Glyph* g) noexcept {
        if (!g || !g->width || !g->height || g->width+1 > AtlasSide || g->height+1 > AtlasSide) return nullptr;
        if (used_*2 >= AtlasSlots) return nullptr; // keeps probing short
        if (shelf_x_+g->width+1 > AtlasSide) { shelf_y_ += shelf_h_; shelf_x_ = 0; shelf_h_ = 0; }
        if (shelf_y_+g->height+1 > AtlasSide) return nullptr;
        uint32_t i = home(g);
        while (slots_[i].glyph) {
            if (slots_[i].glyph == g) return nullptr; // stale size: only clear() removes it
            i = (i+1)&(AtlasSlots-1);
        }
        slots_[i] = {g,uint16_t(shelf_x_),uint16_t(shelf_y_),uint16_t(g->width),uint16_t(g->height)};
        shelf_x_ += g->width+1; shelf_h_ = shelf_h_ > g->height+1 ? shelf_h_ : g->height+1; ++used_;
        return &slots_[i];
    }
};

// Pre-transformed, lit, one texture: Direct3D's D3DFVF_TLVERTEX layout.
struct Vertex { float x, y, z, rhw; uint32_t color, specular; float u, v; };
static_assert(sizeof(Vertex) == 32);

// The rectangle `r` of a command (already inside its clip and its glyph) as two triangles.
// Direct3D 7 puts pixel centres on integers, hence the half-pixel shift for 1:1 texels.
inline void rect_quad(const DrawCommand& c, const Slot& s, Rect r, Vertex* out) noexcept {
    const float l = float(r.left)-0.5f, t = float(r.top)-0.5f, rr = float(r.right)-0.5f, b = float(r.bottom)-0.5f;
    const float inv = 1.0f/float(AtlasSide);
    const float u0 = float(s.x+(r.left-c.left))*inv, v0 = float(s.y+(r.top-c.top))*inv;
    const float u1 = float(s.x+(r.right-c.left))*inv, v1 = float(s.y+(r.bottom-c.top))*inv;
    const uint32_t color = 0xFF000000U|uint32_t(c.rgb[0])<<16|uint32_t(c.rgb[1])<<8|c.rgb[2];
    const Vertex a{l,t,0,1,color,0,u0,v0}, bb{rr,t,0,1,color,0,u1,v0}, cc{rr,b,0,1,color,0,u1,v1}, d{l,b,0,1,color,0,u0,v1};
    out[0] = a; out[1] = bb; out[2] = cc; out[3] = a; out[4] = cc; out[5] = d;
}
// The part of a command inside its clip. False when nothing is visible.
inline bool quad(const DrawCommand& c, const Slot& s, Vertex* out) noexcept {
    const auto r = intersect(c.clip,{c.left,c.top,c.left+int32_t(s.width),c.top+int32_t(s.height)});
    if (empty(r)) return false;
    rect_quad(c,s,r,out); return true;
}
// Command `index` of a frame as vertices: one quad, or, where a native occluder drawn later (the cursor) lies
// over it, strips that leave out exactly the pixels blend() would skip (the same OccluderMask::hides test).
// Rows with the same visible spans share a strip. Returns the vertex count, 0 when nothing shows, or -1 when
// `room` vertices are not enough or a row is too fragmented: the caller then uses the Lock path for the frame.
inline int32_t emit(const FrameList& f, uint32_t index, const Slot& s, Vertex* out, uint32_t room) noexcept {
    const auto& c = f.commands[index];
    const auto r = intersect(c.clip,{c.left,c.top,c.left+int32_t(s.width),c.top+int32_t(s.height)});
    if (empty(r)) return 0;
    std::array<const OccluderMask*,4> above{}; uint32_t above_count = 0;
    for (uint32_t m = 0; m < f.mask_count && m < above.size(); ++m)
        if (index < f.masks[m].covers && !empty(intersect(r,f.masks[m].area))) above[above_count++] = &f.masks[m];
    if (!above_count) {
        if (room < 6) return -1;
        rect_quad(c,s,r,out); return 6;
    }
    constexpr uint32_t MaxSpans = 16;
    struct Spans { std::array<int32_t,MaxSpans*2> x{}; uint32_t count = 0; };
    Spans pending; int32_t pending_top = r.top; uint32_t used = 0;
    const auto flush = [&](int32_t bottom) {
        for (uint32_t k = 0; k < pending.count; ++k) {
            if (used+6 > room) return false;
            rect_quad(c,s,{pending.x[k*2],pending_top,pending.x[k*2+1],bottom},out+used); used += 6;
        }
        return true;
    };
    for (int32_t y = r.top; y < r.bottom; ++y) {
        Spans row; int32_t start = -1;
        for (int32_t x = r.left; x <= r.right; ++x) {
            bool hidden = x == r.right;
            for (uint32_t m = 0; m < above_count && !hidden; ++m) hidden = above[m]->hides(x,y);
            if (!hidden && start < 0) start = x;
            else if (hidden && start >= 0) {
                if (row.count == MaxSpans) return -1;
                row.x[row.count*2] = start; row.x[row.count*2+1] = x; ++row.count; start = -1;
            }
        }
        bool same = y != r.top && row.count == pending.count;
        for (uint32_t k = 0; same && k < row.count*2; ++k) same = row.x[k] == pending.x[k];
        if (same) continue;
        if (y != r.top && !flush(y)) return -1;
        pending = row; pending_top = y;
    }
    if (!flush(r.bottom)) return -1;
    return int32_t(used);
}

// True when a native occluder (the cursor) lies over a command it must cover. emit() cuts those commands into
// strips; this only tells such frames apart (counters, tests).
inline bool needs_masks(const FrameList& f) noexcept {
    for (uint32_t m = 0; m < f.mask_count; ++m)
        for (uint32_t i = 0; i < f.count && i < f.masks[m].covers; ++i) {
            const auto& c = f.commands[i];
            if (!c.glyph) continue;
            const auto r = intersect(c.clip,{c.left,c.top,c.left+int32_t(c.glyph->width),c.top+int32_t(c.glyph->height)});
            if (!empty(intersect(r,f.masks[m].area))) return true;
        }
    return false;
}

enum class Status : uint32_t {
    Drawn = 0,
    Masked = 1,        // an occluder covers text in a way too fragmented for strips: Lock path this frame
    NoDevice = 2, NoTexture = 3, TextureLock = 4, AtlasFull = 5, StateRead = 6, Scene = 7, Draw = 8, Fault = 9
};

// Owns the atlas texture. Every call is on the render thread, after the game's EndScene and before its Flip.
// No allocation, logging or GDI of our own.
class Overlay {
    Atlas atlas_;
    std::array<Vertex,MaxQuads*6> vertices_{};
    void* device_ = nullptr;   // IDirect3DDevice7 the texture belongs to
    void* texture_ = nullptr;  // IDirectDrawSurface7
    bool disabled_ = false;
    uint32_t failures_in_a_row_ = 0;
    Status draw_guarded(void* device, void* backbuffer, const FrameList& frame) noexcept;
public:
    uint64_t frames = 0, fallbacks = 0, uploads = 0, resets = 0, restore_failures = 0, masked_frames = 0, strip_frames = 0;
    uint32_t last_status = 0, last_hresult = 0;
    bool disabled() const noexcept { return disabled_; }
    uint32_t glyphs() const noexcept { return atlas_.size(); }
    // Drawn: every command is on the back buffer and the device state is as it was. Anything else: nothing was
    // drawn (or the state was restored) and the caller uses the Lock path. A fault or a state that could not be
    // restored disables the overlay for the rest of the run.
    Status draw(void* device, void* backbuffer, const FrameList& frame) noexcept;
};
}
