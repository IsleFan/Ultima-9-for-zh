// Direct3D overlay, the parts with no device (U9ChtRuntime/gpu_overlay.h): atlas packing, quad geometry and the
// rule that sends a frame with a covering occluder back to the Lock path. Synthetic glyphs only.
#include "gpu_overlay.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace u9cht;
using namespace u9cht::gpu;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
Glyph make(uint32_t w, uint32_t h) { Glyph g; g.width = w; g.height = h; g.alpha.assign(size_t(w)*h,255); return g; }
bool close_to(float a, float b) { return std::fabs(a-b) < 1e-6f; }
}

int main() {
    {   // placing and finding
        auto atlas = std::make_unique<Atlas>();
        auto a = make(40,40), b = make(20,30);
        check(!atlas->find(&a), "an empty atlas has nothing");
        const auto* sa = atlas->place(&a); const auto* sb = atlas->place(&b);
        check(sa && sb && atlas->size() == 2, "two glyphs placed");
        check(sa && sa->x == 0 && sa->y == 0 && sa->width == 40 && sa->height == 40, "first glyph at the origin");
        check(sb && sb->x == 41 && sb->y == 0, "second glyph after one texel of padding");
        check(atlas->find(&a) == sa && atlas->find(&b) == sb, "both found again");
        check(!atlas->place(&a), "a present glyph is not placed twice");
        a.width = 39;
        check(!atlas->find(&a), "a glyph whose size changed is not found");
        atlas->clear();
        check(atlas->size() == 0 && !atlas->find(&b), "clear empties the atlas");
    }
    {   // shelves wrap, never overlap, and the atlas reports when it is full
        auto atlas = std::make_unique<Atlas>();
        std::vector<Glyph> glyphs(700, make(40,40));
        std::vector<const Slot*> placed;
        for (auto& g : glyphs) { const auto* s = atlas->place(&g); if (!s) break; placed.push_back(s); }
        check(placed.size() == 24*24, "a 1024 atlas holds 24x24 glyphs of 40+1 texels");
        bool inside = true, apart = true;
        for (size_t i = 0; i < placed.size(); ++i) {
            const auto& s = *placed[i];
            inside &= s.x+s.width <= AtlasSide && s.y+s.height <= AtlasSide;
            if (i && placed[i-1]->y == s.y) apart &= placed[i-1]->x+placed[i-1]->width < s.x;
        }
        check(inside, "every slot lies inside the texture");
        check(apart, "neighbours on a shelf do not touch");
        check(!atlas->place(&glyphs.back()), "a full atlas refuses");
        Glyph none = make(0,5), huge = make(AtlasSide,4);
        check(!atlas->place(&none) && !atlas->place(&huge) && !atlas->place(nullptr), "empty, oversized and null glyphs are refused");
    }
    {   // the slot table refuses before probing gets long
        auto atlas = std::make_unique<Atlas>();
        std::vector<Glyph> glyphs(AtlasSlots, make(1,1));
        uint32_t count = 0;
        for (auto& g : glyphs) { if (!atlas->place(&g)) break; ++count; }
        check(count == AtlasSlots/2, "at most half the slot table is used");
    }
    {   // quad geometry: full glyph
        auto g = make(10,20);
        const Slot s{&g,100,200,10,20};
        const DrawCommand c{&g,50,60,{0,0,1600,1200},{1,2,3}};
        Vertex v[6]{};
        check(quad(c,s,v), "a visible command makes a quad");
        check(close_to(v[0].x,49.5f) && close_to(v[0].y,59.5f) && close_to(v[2].x,59.5f) && close_to(v[2].y,79.5f), "corners shifted half a pixel");
        check(close_to(v[0].u,100.0f/AtlasSide) && close_to(v[0].v,200.0f/AtlasSide) && close_to(v[2].u,110.0f/AtlasSide) && close_to(v[2].v,220.0f/AtlasSide), "texture corners");
        check(v[0].color == 0xFF010203U && v[5].color == 0xFF010203U && v[0].rhw == 1.0f && v[0].z == 0.0f, "colour and depth");
        check(close_to(v[1].x,v[2].x) && close_to(v[1].y,v[0].y) && close_to(v[5].x,v[0].x) && close_to(v[5].y,v[2].y) &&
              close_to(v[3].x,v[0].x) && close_to(v[4].x,v[2].x), "two triangles cover the rectangle");
    }
    {   // quad geometry: clipped on every side, and fully clipped
        auto g = make(10,20);
        const Slot s{&g,100,200,10,20};
        const DrawCommand c{&g,50,60,{53,65,58,70},{255,255,255}};
        Vertex v[6]{};
        check(quad(c,s,v), "a partly clipped command makes a quad");
        check(close_to(v[0].x,52.5f) && close_to(v[0].y,64.5f) && close_to(v[2].x,57.5f) && close_to(v[2].y,69.5f), "clipped corners");
        check(close_to(v[0].u,103.0f/AtlasSide) && close_to(v[0].v,205.0f/AtlasSide) && close_to(v[2].u,108.0f/AtlasSide) && close_to(v[2].v,210.0f/AtlasSide),
              "texture corners follow the clip");
        const DrawCommand outside{&g,50,60,{0,0,40,40},{}};
        check(!quad(outside,s,v), "a command outside its clip makes nothing");
    }
    {   // occluders
        auto g = make(10,10);
        auto frame = std::make_unique<FrameList>();
        frame->width = 1600; frame->height = 1200;
        frame->add({&g,100,100,{0,0,1600,1200},{}});
        frame->add({&g,300,100,{0,0,1600,1200},{}});
        check(!needs_masks(*frame), "no occluder, no masks");
        frame->mask_count = 1; frame->masks[0].area = {500,500,600,600}; frame->masks[0].covers = 2;
        check(!needs_masks(*frame), "an occluder away from the text does not matter");
        frame->masks[0].area = {305,105,400,200};
        check(needs_masks(*frame), "an occluder over text it covers needs the Lock path");
        frame->masks[0].covers = 1;
        check(!needs_masks(*frame), "text queued after the occluder lies above it");
    }
    {   // strips under an occluder show exactly the pixels blend() would write
        auto g = make(40,40);
        const Slot s{&g,0,0,40,40};
        auto frame = std::make_unique<FrameList>();
        frame->width = 1600; frame->height = 1200;
        frame->add({&g,100,100,{0,0,1600,1200},{9,8,7}});
        frame->add({&g,100,100,{105,110,135,128},{9,8,7}}); // clipped copy
        auto vertices = std::make_unique<std::array<Vertex,MaxQuads*6>>();
        check(emit(*frame,0,s,vertices->data(),6) == 6, "no occluder: one quad");
        check(emit(*frame,0,s,vertices->data(),5) == -1, "no room for the quad");
        // An arrow-like mask: a diagonal band, stretched 2x over its area, partly over the glyph.
        frame->mask_count = 1;
        auto& mask = frame->masks[0];
        mask.area = {120,90,184,154}; mask.covers = 2; mask.width = 32; mask.height = 32;
        for (uint32_t y = 0; y < 32; ++y) for (uint32_t x = 0; x < 32; ++x)
            mask.opaque[y*MaskMaxSide+x] = (x >= y/2 && x <= y) || (x == 20 && y%3 == 0);
        for (uint32_t index = 0; index < 2; ++index) {
            const auto& c = frame->commands[index];
            const auto n = emit(*frame,index,s,vertices->data(),uint32_t(vertices->size()));
            check(n > 6 && n%6 == 0, "an occluded glyph becomes several strips");
            std::vector<uint8_t> covered(40*40,0); bool exact_uv = true, once = true;
            for (int32_t q = 0; q < n; q += 6) {
                const auto& a = (*vertices)[q]; const auto& b = (*vertices)[q+2];
                const int32_t l = int32_t(a.x+0.5f), t = int32_t(a.y+0.5f), r = int32_t(b.x+0.5f), bt = int32_t(b.y+0.5f);
                exact_uv &= close_to(a.u,float(l-100)/AtlasSide) && close_to(a.v,float(t-100)/AtlasSide) &&
                            close_to(b.u,float(r-100)/AtlasSide) && close_to(b.v,float(bt-100)/AtlasSide);
                for (int32_t y = t; y < bt; ++y) for (int32_t x = l; x < r; ++x) {
                    auto& cell = covered[size_t(y-100)*40+(x-100)]; once &= cell == 0; cell = 1;
                }
            }
            bool matches = true; uint32_t hidden = 0;
            for (int32_t y = 100; y < 140; ++y) for (int32_t x = 100; x < 140; ++x) {
                const bool in_clip = x >= c.clip.left && x < c.clip.right && y >= c.clip.top && y < c.clip.bottom;
                const bool want = in_clip && !mask.hides(x,y);
                hidden += in_clip && mask.hides(x,y);
                matches &= covered[size_t(y-100)*40+(x-100)] == uint8_t(want);
            }
            check(hidden > 0, "the test occluder really hides glyph pixels");
            check(matches, "strips cover every visible pixel and no hidden one");
            check(once, "strips do not overlap");
            check(exact_uv, "each strip samples its own texels");
        }
        check(emit(*frame,0,s,vertices->data(),12) == -1, "too little room for the strips");
        mask.covers = 0;
        check(emit(*frame,0,s,vertices->data(),uint32_t(vertices->size())) == 6, "text queued after the occluder stays one quad");
        // A comb finer than MaxSpans gives the frame up.
        mask.covers = 2; mask.area = {100,100,140,140}; mask.width = 40; mask.height = 40;
        for (uint32_t y = 0; y < 40; ++y) for (uint32_t x = 0; x < 40; ++x) mask.opaque[y*MaskMaxSide+x] = x%2;
        check(emit(*frame,0,s,vertices->data(),uint32_t(vertices->size())) == -1, "a row with too many spans is refused");
    }
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "gpu overlay tests passed\n";
    return 0;
}
