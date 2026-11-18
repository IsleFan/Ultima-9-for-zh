// Release patches (common/flx_patch.h) on synthetic bytes only. The same format is written by u9cht/l10n/patch.py
// (tests/test_patch.py); the fixture below is the patch that tool makes for these inputs.
#include "flx_patch.h"
#include <iostream>

using namespace u9cht;
namespace {
int failures = 0;
void check(bool ok, const char* what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
using Bytes = std::vector<uint8_t>;
void u32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8*i))); }
Bytes header(uint32_t source, uint32_t target, uint32_t ops, uint32_t version = 1) {
    Bytes b{'U','9','C','P'}; u32(b,version); u32(b,source); u32(b,target); u32(b,ops); return b;
}
}

int main() {
    const Bytes source{'H','e','l','l','o',' ','w','o','r','l','d','!','A','B'};
    {   // copy 0..5, add "XY", widen "AB" (12..13), copy "world" (6..10)
        Bytes p = header(14, 2+5+4+5, 4);
        p.push_back(1); u32(p,2); p.push_back('X'); p.push_back('Y');
        p.push_back(0); u32(p,0); u32(p,5);
        p.push_back(2); u32(p,12); u32(p,2);
        p.push_back(0); u32(p,6); u32(p,5);
        std::string why;
        const auto out = patch::apply(source,p,&why);
        const Bytes want{'X','Y','H','e','l','l','o','A',0,'B',0,'w','o','r','l','d'};
        check(out && *out == want, "copy, add and widen in order");
    }
    {   std::string why;
        Bytes p = header(14,0,0);
        check(patch::apply(source,p,&why) && patch::apply(source,p)->empty(), "empty target");
        check(!patch::apply(Bytes(13),p,&why) && why == "source size differs", "wrong source refused");
        p[0] = 'X';
        check(!patch::apply(source,p,&why) && why == "not a U9CP patch", "magic");
        check(!patch::apply(source,header(14,0,0,2),&why) && why == "unsupported patch version", "version");
    }
    {   std::string why;
        Bytes p = header(14,4,1); p.push_back(0); u32(p,12); u32(p,4);
        check(!patch::apply(source,p,&why) && why == "copy outside the source", "copy bounds");
        Bytes w = header(14,6,1); w.push_back(2); u32(w,12); u32(w,3);
        check(!patch::apply(source,w,&why) && why == "widen outside the source", "widen bounds");
        Bytes a = header(14,4,1); a.push_back(1); u32(a,4); a.push_back('x');
        check(!patch::apply(source,a,&why) && why == "add runs past the patch", "add bounds");
        Bytes l = header(14,2,1); l.push_back(0); u32(l,0); u32(l,3);
        check(!patch::apply(source,l,&why) && why == "output too long", "target size enforced");
        Bytes s = header(14,3,1); s.push_back(0); u32(s,0); u32(s,2);
        check(!patch::apply(source,s,&why) && why == "output size differs", "short output refused");
        Bytes t = header(14,0,0); t.push_back(0);
        check(!patch::apply(source,t,&why) && why == "trailing bytes", "trailing bytes refused");
        Bytes k = header(14,0,1); k.push_back(7);
        check(!patch::apply(source,k,&why) && why == "unknown op", "unknown op");
        Bytes h{'U','9','C','P',1,0};
        check(!patch::apply(source,h,&why) && why == "truncated header", "truncated header");
    }
    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "flx_patch tests passed\n";
    return 0;
}
