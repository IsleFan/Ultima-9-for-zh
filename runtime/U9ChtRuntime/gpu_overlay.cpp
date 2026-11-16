#define DIRECT3D_VERSION 0x0700
#include "gpu_overlay.h"
#include <d3d.h>
#include <cstring>

namespace u9cht::gpu {
namespace {
struct RenderState { D3DRENDERSTATETYPE type; DWORD value; };
// Everything the quads depend on. Each is read before it is set and put back afterwards, so the game's own
// idea of the device state stays true.
constexpr RenderState RenderStates[] = {
    {D3DRENDERSTATE_ZENABLE,D3DZB_FALSE}, {D3DRENDERSTATE_ZWRITEENABLE,FALSE},
    {D3DRENDERSTATE_ALPHABLENDENABLE,TRUE}, {D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA},
    {D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA}, {D3DRENDERSTATE_ALPHATESTENABLE,FALSE},
    {D3DRENDERSTATE_CULLMODE,D3DCULL_NONE}, {D3DRENDERSTATE_LIGHTING,FALSE}, {D3DRENDERSTATE_FOGENABLE,FALSE},
    {D3DRENDERSTATE_COLORKEYENABLE,FALSE}, {D3DRENDERSTATE_SPECULARENABLE,FALSE},
    {D3DRENDERSTATE_STENCILENABLE,FALSE}, {D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID},
    {D3DRENDERSTATE_SHADEMODE,D3DSHADE_GOURAUD},
};
struct StageState { DWORD stage; D3DTEXTURESTAGESTATETYPE type; DWORD value; };
constexpr StageState StageStates[] = {
    {0,D3DTSS_COLOROP,D3DTOP_SELECTARG2}, {0,D3DTSS_COLORARG2,D3DTA_DIFFUSE},
    {0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1}, {0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE},
    {0,D3DTSS_MINFILTER,D3DTFN_POINT}, {0,D3DTSS_MAGFILTER,D3DTFG_POINT}, {0,D3DTSS_MIPFILTER,D3DTFP_NONE},
    {0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP}, {0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP}, {0,D3DTSS_TEXCOORDINDEX,0},
    {1,D3DTSS_COLOROP,D3DTOP_DISABLE}, {1,D3DTSS_ALPHAOP,D3DTOP_DISABLE},
};
constexpr size_t RenderCount = sizeof(RenderStates)/sizeof(RenderStates[0]);
constexpr size_t StageCount = sizeof(StageStates)/sizeof(StageStates[0]);

IDirectDrawSurface7* create_texture(IDirectDrawSurface7* back, HRESULT& hr) noexcept {
    IUnknown* owner = nullptr;
    hr = back->GetDDInterface(reinterpret_cast<void**>(&owner));
    if (FAILED(hr) || !owner) return nullptr;
    IDirectDraw7* draw = nullptr;
    hr = owner->QueryInterface(IID_IDirectDraw7,reinterpret_cast<void**>(&draw));
    owner->Release();
    if (FAILED(hr) || !draw) return nullptr;
    DDSURFACEDESC2 desc{}; desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
    desc.dwWidth = desc.dwHeight = AtlasSide;
    desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE; desc.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE;
    auto& pf = desc.ddpfPixelFormat; pf.dwSize = sizeof(pf);
    pf.dwFlags = DDPF_RGB|DDPF_ALPHAPIXELS; pf.dwRGBBitCount = 32;
    pf.dwRBitMask = 0xFF0000; pf.dwGBitMask = 0xFF00; pf.dwBBitMask = 0xFF; pf.dwRGBAlphaBitMask = 0xFF000000;
    IDirectDrawSurface7* texture = nullptr;
    hr = draw->CreateSurface(&desc,&texture,nullptr);
    draw->Release();
    return SUCCEEDED(hr) ? texture : nullptr;
}
// Writes the given slots (or clears everything first) under one texture Lock.
HRESULT upload(IDirectDrawSurface7* texture, const Slot* const* slots, uint32_t count, bool clear) noexcept {
    DDSURFACEDESC2 desc{}; desc.dwSize = sizeof(desc);
    const auto hr = texture->Lock(nullptr,&desc,DDLOCK_WAIT|DDLOCK_NOSYSLOCK|DDLOCK_WRITEONLY,nullptr);
    if (FAILED(hr)) return hr;
    const bool usable = desc.lpSurface && desc.dwWidth == AtlasSide && desc.dwHeight == AtlasSide &&
        desc.lPitch >= LONG(AtlasSide*4) && desc.ddpfPixelFormat.dwRGBBitCount == 32 &&
        desc.ddpfPixelFormat.dwRGBAlphaBitMask == 0xFF000000;
    if (usable) {
        auto* base = static_cast<uint8_t*>(desc.lpSurface);
        if (clear) for (uint32_t y = 0; y < AtlasSide; ++y) std::memset(base+size_t(y)*desc.lPitch,0,AtlasSide*4);
        for (uint32_t i = 0; i < count; ++i) {
            const auto& s = *slots[i]; const auto* alpha = s.glyph->alpha.data();
            for (uint32_t y = 0; y < s.height; ++y) {
                auto* row = reinterpret_cast<uint32_t*>(base+size_t(s.y+y)*desc.lPitch)+s.x;
                for (uint32_t x = 0; x < s.width; ++x) row[x] = uint32_t(alpha[size_t(y)*s.width+x])<<24|0xFFFFFF;
            }
        }
    }
    const auto unlocked = texture->Unlock(nullptr);
    return !usable ? E_FAIL : unlocked;
}
}

Status Overlay::draw_guarded(void* device_pointer, void* backbuffer, const FrameList& frame) noexcept {
    last_hresult = 0;
    auto* device = static_cast<IDirect3DDevice7*>(device_pointer);
    auto* back = static_cast<IDirectDrawSurface7*>(backbuffer);
    if (!device || !back) return Status::NoDevice;
    HRESULT hr = S_OK;
    if (device_ != device_pointer) {
        // A new device (the game recreated its renderer). The old texture belonged to the old one and is left
        // alone rather than released through an object that may be gone.
        device_ = device_pointer; texture_ = nullptr; atlas_.clear();
    }
    auto* texture = static_cast<IDirectDrawSurface7*>(texture_);
    bool clear = false;
    if (!texture) {
        texture = create_texture(back,hr);
        if (!texture) { last_hresult = uint32_t(hr); return Status::NoTexture; }
        texture_ = texture; atlas_.clear(); clear = true;
    } else if (texture->IsLost() == DDERR_SURFACELOST) {
        hr = texture->Restore();
        if (FAILED(hr)) { last_hresult = uint32_t(hr); return Status::NoTexture; }
        atlas_.clear(); clear = true;
    }
    // Every glyph of the frame gets a slot; a full atlas is emptied once and refilled from this frame.
    const Slot* fresh[MaxQuads]; uint32_t fresh_count = 0;
    for (int attempt = 0;; ++attempt) {
        bool full = false; fresh_count = 0;
        for (uint32_t i = 0; i < frame.count && !full; ++i) {
            const auto* g = frame.commands[i].glyph;
            if (!g || atlas_.find(g)) continue;
            if (g->alpha.size() < size_t(g->width)*g->height) return Status::AtlasFull;
            const auto* slot = atlas_.place(g);
            if (!slot || fresh_count >= MaxQuads) full = true; else fresh[fresh_count++] = slot;
        }
        if (!full) break;
        if (attempt) return Status::AtlasFull;
        atlas_.clear(); clear = true; ++resets;
    }
    if (fresh_count || clear) {
        hr = upload(texture,fresh,fresh_count,clear);
        if (FAILED(hr)) { last_hresult = uint32_t(hr); atlas_.clear(); return Status::TextureLock; }
        uploads += fresh_count;
    }
    uint32_t count = 0;
    for (uint32_t i = 0; i < frame.count; ++i) {
        const auto& c = frame.commands[i];
        const auto* slot = c.glyph ? atlas_.find(c.glyph) : nullptr;
        if (!slot) continue;
        const auto added = emit(frame,i,*slot,vertices_.data()+count,uint32_t(vertices_.size())-count);
        if (added < 0) return Status::Masked;
        count += uint32_t(added);
    }
    if (frame.mask_count && needs_masks(frame)) ++strip_frames;
    if (!count) return Status::Drawn;

    DWORD saved_render[RenderCount], saved_stage[StageCount];
    for (size_t i = 0; i < RenderCount; ++i)
        if (FAILED(hr = device->GetRenderState(RenderStates[i].type,&saved_render[i]))) { last_hresult = uint32_t(hr); return Status::StateRead; }
    for (size_t i = 0; i < StageCount; ++i)
        if (FAILED(hr = device->GetTextureStageState(StageStates[i].stage,StageStates[i].type,&saved_stage[i]))) { last_hresult = uint32_t(hr); return Status::StateRead; }
    D3DVIEWPORT7 saved_view{};
    if (FAILED(hr = device->GetViewport(&saved_view))) { last_hresult = uint32_t(hr); return Status::StateRead; }
    IDirectDrawSurface7* saved_texture = nullptr;
    if (FAILED(hr = device->GetTexture(0,&saved_texture))) { last_hresult = uint32_t(hr); return Status::StateRead; }

    Status status = Status::Drawn;
    if (FAILED(hr = device->BeginScene())) { last_hresult = uint32_t(hr); status = Status::Scene; }
    else {
        bool ok = true;
        for (size_t i = 0; i < RenderCount; ++i) ok &= SUCCEEDED(device->SetRenderState(RenderStates[i].type,RenderStates[i].value));
        for (size_t i = 0; i < StageCount; ++i) ok &= SUCCEEDED(device->SetTextureStageState(StageStates[i].stage,StageStates[i].type,StageStates[i].value));
        D3DVIEWPORT7 view{0,0,frame.width,frame.height,0.0f,1.0f};
        const bool same_view = saved_view.dwX == 0 && saved_view.dwY == 0 && saved_view.dwWidth == frame.width && saved_view.dwHeight == frame.height;
        if (!same_view) ok &= SUCCEEDED(device->SetViewport(&view));
        ok &= SUCCEEDED(device->SetTexture(0,texture));
        if (!ok) status = Status::Draw;
        else if (FAILED(hr = device->DrawPrimitive(D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,vertices_.data(),count,0))) {
            last_hresult = uint32_t(hr); status = Status::Draw;
        }
        if (FAILED(hr = device->EndScene())) { last_hresult = uint32_t(hr); status = Status::Scene; disabled_ = true; }
        if (!same_view && FAILED(device->SetViewport(&saved_view))) { ++restore_failures; disabled_ = true; }
    }
    // Put back everything, whether or not it was reached above: setting a state to its own value is harmless.
    if (FAILED(device->SetTexture(0,saved_texture))) { ++restore_failures; disabled_ = true; }
    if (saved_texture) saved_texture->Release();
    for (size_t i = StageCount; i--;)
        if (FAILED(device->SetTextureStageState(StageStates[i].stage,StageStates[i].type,saved_stage[i]))) { ++restore_failures; disabled_ = true; }
    for (size_t i = RenderCount; i--;)
        if (FAILED(device->SetRenderState(RenderStates[i].type,saved_render[i]))) { ++restore_failures; disabled_ = true; }
    return status;
}

Status Overlay::draw(void* device, void* backbuffer, const FrameList& frame) noexcept {
    if (disabled_) return Status(last_status);
    Status status = Status::Fault;
    __try { status = draw_guarded(device,backbuffer,frame); }
    __except (EXCEPTION_EXECUTE_HANDLER) { status = Status::Fault; disabled_ = true; }
    if (status == Status::Drawn) { ++frames; failures_in_a_row_ = 0; }
    else if (status == Status::Masked) ++masked_frames;
    else {
        ++fallbacks; last_status = uint32_t(status);
        if (++failures_in_a_row_ >= 8) disabled_ = true; // not working here: stop paying for the attempt
    }
    return status;
}
}
