#include "ddraw.h"

#include <SDL3/SDL_render.h>

#include <algorithm>
#include <vector>

namespace {

SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
int g_texture_width = 0;
int g_texture_height = 0;
IDirectDrawSurface* g_primary_surface = nullptr;

RAWindow* ensure_window(RAWindow* window, int width, int height)
{
    if (window && window->sdl_window) {
        return window;
    }

    auto* created = new RAWindow{};
    created->title = "Red Alert";
    created->width = width;
    created->height = height;
    created->sdl_window = SDL_CreateWindow(created->title.c_str(), width, height, 0);
    return created;
}

void ensure_renderer(RAWindow* window, int width, int height)
{
    if (!window || !window->sdl_window) {
        return;
    }

    if (!g_renderer) {
        g_renderer = SDL_CreateRenderer(window->sdl_window, nullptr);
    }

    if (!g_renderer) {
        return;
    }

    if (!g_texture || g_texture_width != width || g_texture_height != height) {
        if (g_texture) {
            SDL_DestroyTexture(g_texture);
        }
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        g_texture_width = width;
        g_texture_height = height;
    }
}

void present_surface(IDirectDrawSurface* surface, RAWindow* window)
{
    if (!surface || !surface->IsPrimary()) {
        return;
    }

    ensure_renderer(window, surface->Width(), surface->Height());
    if (!g_renderer || !g_texture) {
        return;
    }

    std::vector<uint32_t> rgba(static_cast<size_t>(surface->Width()) * static_cast<size_t>(surface->Height()));
    const PALETTEENTRY* palette = surface->PaletteEntries();
    uint8_t* pixels = surface->Pixels();

    for (int y = 0; y < surface->Height(); ++y) {
        for (int x = 0; x < surface->Width(); ++x) {
            const uint8_t index = pixels[y * surface->Width() + x];
            const PALETTEENTRY entry = palette ? palette[index] : PALETTEENTRY{index, index, index, 0};
            rgba[y * surface->Width() + x] = (0xffu << 24) | (static_cast<uint32_t>(entry.peRed) << 16)
                | (static_cast<uint32_t>(entry.peGreen) << 8) | static_cast<uint32_t>(entry.peBlue);
        }
    }

    SDL_UpdateTexture(g_texture, nullptr, rgba.data(), surface->Width() * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, g_texture, nullptr, nullptr);
    SDL_RenderPresent(g_renderer);
}

RECT normalize_rect(const RECT* rect, int width, int height)
{
    RECT result{};
    result.left = rect ? std::max<LONG>(0, rect->left) : 0;
    result.top = rect ? std::max<LONG>(0, rect->top) : 0;
    result.right = rect ? std::min<LONG>(width, rect->right) : width;
    result.bottom = rect ? std::min<LONG>(height, rect->bottom) : height;
    return result;
}

} // namespace

IDirectDrawPalette::IDirectDrawPalette() : ref_count_(1)
{
    std::fill(std::begin(entries_), std::end(entries_), PALETTEENTRY{0, 0, 0, 0});
}

HRESULT IDirectDrawPalette::GetEntries(DWORD, DWORD start, DWORD count, PALETTEENTRY* entries)
{
    if (!entries || start + count > 256) {
        return DDERR_INVALIDPARAMS;
    }
    std::copy(entries_ + start, entries_ + start + count, entries);
    return DD_OK;
}

HRESULT IDirectDrawPalette::SetEntries(DWORD, DWORD start, DWORD count, const PALETTEENTRY* entries)
{
    if (!entries || start + count > 256) {
        return DDERR_INVALIDPARAMS;
    }
    std::copy(entries, entries + count, entries_ + start);
    if (g_primary_surface && g_primary_surface->UsesPalette(this)) {
        g_primary_surface->Present();
    }
    return DD_OK;
}

HRESULT IDirectDrawPalette::Release()
{
    if (--ref_count_ == 0) {
        delete this;
    }
    return DD_OK;
}

const PALETTEENTRY* IDirectDrawPalette::Entries() const
{
    return entries_;
}

IDirectDrawSurface::IDirectDrawSurface(int width, int height, bool primary, bool system_memory, HWND window)
    : width_(width), height_(height), primary_(primary), system_memory_(system_memory), window_(window), ref_count_(1), palette_(nullptr), pixels_(static_cast<size_t>(width) * static_cast<size_t>(height), 0)
{
    if (primary_) {
        g_primary_surface = this;
    }
}

HRESULT IDirectDrawSurface::Lock(RECT*, DDSURFACEDESC* desc, DWORD, HANDLE)
{
    if (!desc) {
        return DDERR_INVALIDPARAMS;
    }
    desc->dwSize = sizeof(*desc);
    desc->dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc->dwWidth = width_;
    desc->dwHeight = height_;
    desc->lPitch = width_;
    desc->lpSurface = pixels_.data();
    desc->ddsCaps.dwCaps = primary_ ? DDSCAPS_PRIMARYSURFACE : (system_memory_ ? DDSCAPS_SYSTEMMEMORY : DDSCAPS_OFFSCREENPLAIN);
    return DD_OK;
}

HRESULT IDirectDrawSurface::Unlock(LPVOID)
{
    if (primary_) {
        present_surface(this, window_);
    }
    return DD_OK;
}

HRESULT IDirectDrawSurface::Blt(RECT* dest_rect, IDirectDrawSurface* src_surface, RECT* src_rect, DWORD flags, DDBLTFX* fx)
{
    RECT dest = normalize_rect(dest_rect, width_, height_);

    if (!src_surface && (flags & DDBLT_COLORFILL)) {
        const uint8_t fill = fx ? static_cast<uint8_t>(fx->dwFillColor & 0xff) : 0;
        for (LONG y = dest.top; y < dest.bottom; ++y) {
            std::fill_n(pixels_.data() + y * width_ + dest.left, dest.right - dest.left, fill);
        }
        return DD_OK;
    }

    if (!src_surface) {
        return DDERR_INVALIDPARAMS;
    }

    RECT src_bounds = normalize_rect(src_rect, src_surface->Width(), src_surface->Height());
    const int copy_width = std::min<int>(dest.right - dest.left, src_bounds.right - src_bounds.left);
    const int copy_height = std::min<int>(dest.bottom - dest.top, src_bounds.bottom - src_bounds.top);
    if (copy_width <= 0 || copy_height <= 0) {
        return DD_OK;
    }

    for (int row = 0; row < copy_height; ++row) {
        uint8_t* dst = pixels_.data() + (dest.top + row) * width_ + dest.left;
        uint8_t* src = src_surface->Pixels() + (src_bounds.top + row) * src_surface->Width() + src_bounds.left;
        if (flags & DDBLT_KEYSRC) {
            for (int col = 0; col < copy_width; ++col) {
                if (src[col] != 0) {
                    dst[col] = src[col];
                }
            }
        } else {
            std::copy(src, src + copy_width, dst);
        }
    }
    if (primary_) {
        present_surface(this, window_);
    }
    return DD_OK;
}

HRESULT IDirectDrawSurface::GetBltStatus(DWORD)
{
    return DD_OK;
}

HRESULT IDirectDrawSurface::Restore()
{
    return DD_OK;
}

HRESULT IDirectDrawSurface::Release()
{
    if (--ref_count_ == 0) {
        if (g_primary_surface == this) {
            g_primary_surface = nullptr;
        }
        delete this;
    }
    return DD_OK;
}

HRESULT IDirectDrawSurface::GetCaps(DDSCAPS* caps)
{
    if (!caps) return DDERR_INVALIDPARAMS;
    caps->dwCaps = primary_ ? DDSCAPS_PRIMARYSURFACE : (system_memory_ ? DDSCAPS_SYSTEMMEMORY : DDSCAPS_OFFSCREENPLAIN);
    return DD_OK;
}

HRESULT IDirectDrawSurface::GetPalette(IDirectDrawPalette** palette)
{
    if (!palette) {
        return DDERR_INVALIDPARAMS;
    }
    if (!palette_) {
        *palette = nullptr;
        return DDERR_NOPALETTEATTACHED;
    }

    auto* copy = new IDirectDrawPalette();
    if (copy->SetEntries(0, 0, 256, palette_->Entries()) != DD_OK) {
        copy->Release();
        *palette = nullptr;
        return DDERR_GENERIC;
    }

    *palette = copy;
    return DD_OK;
}

HRESULT IDirectDrawSurface::SetPalette(IDirectDrawPalette* palette)
{
    palette_ = palette;
    if (primary_) {
        present_surface(this, window_);
    }
    return DD_OK;
}

HRESULT IDirectDrawSurface::AddAttachedSurface(IDirectDrawSurface*)
{
    return DD_OK;
}

int IDirectDrawSurface::Width() const { return width_; }
int IDirectDrawSurface::Height() const { return height_; }
uint8_t* IDirectDrawSurface::Pixels() { return pixels_.data(); }
const PALETTEENTRY* IDirectDrawSurface::PaletteEntries() const { return palette_ ? palette_->Entries() : nullptr; }
bool IDirectDrawSurface::IsPrimary() const { return primary_; }
bool IDirectDrawSurface::UsesPalette(const IDirectDrawPalette* palette) const { return palette_ == palette; }
void IDirectDrawSurface::Present() { present_surface(this, window_); }

IDirectDraw::IDirectDraw() : width_(640), height_(480), bits_per_pixel_(8), window_(nullptr), ref_count_(1)
{
}

HRESULT IDirectDraw::SetCooperativeLevel(HWND hwnd, DWORD)
{
    window_ = hwnd;
    return DD_OK;
}

HRESULT IDirectDraw::SetDisplayMode(int width, int height, int bits_per_pixel)
{
    width_ = width;
    height_ = height;
    bits_per_pixel_ = bits_per_pixel;
    window_ = ensure_window(window_, width_, height_);
    if (window_ && window_->sdl_window) {
        SDL_SetWindowSize(window_->sdl_window, width_, height_);
        SDL_ShowWindow(window_->sdl_window);
    }
    return DD_OK;
}

HRESULT IDirectDraw::CreatePalette(DWORD, PALETTEENTRY* entries, IDirectDrawPalette** palette, HANDLE)
{
    if (!palette) {
        return DDERR_INVALIDPARAMS;
    }
    auto* created = new IDirectDrawPalette();
    if (entries) {
        created->SetEntries(0, 0, 256, entries);
    }
    *palette = created;
    return DD_OK;
}

HRESULT IDirectDraw::CreateSurface(DDSURFACEDESC* desc, IDirectDrawSurface** surface, HANDLE)
{
    if (!desc || !surface) {
        return DDERR_INVALIDPARAMS;
    }
    const bool primary = (desc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) != 0;
    const bool system_memory = (desc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) != 0;
    const int width = primary ? width_ : static_cast<int>(desc->dwWidth);
    const int height = primary ? height_ : static_cast<int>(desc->dwHeight);
    *surface = new IDirectDrawSurface(width > 0 ? width : width_, height > 0 ? height : height_, primary, system_memory, window_);
    if (primary) {
        present_surface(*surface, window_);
    }
    return DD_OK;
}

HRESULT IDirectDraw::GetCaps(DDCAPS* driver_caps, DDCAPS* hel_caps)
{
    auto fill = [](DDCAPS* caps) {
        if (!caps) return;
        caps->dwSize = sizeof(*caps);
        caps->dwCaps = DDCAPS_BLT | DDCAPS_BLTCOLORFILL | DDCAPS_CANBLTSYSMEM;
        caps->dwVidMemTotal = 32 * 1024 * 1024;
        caps->dwVidMemFree = 32 * 1024 * 1024;
        caps->dwSVBCaps = DDCAPS_BLT;
        caps->dwVSBCaps = DDCAPS_BLT;
        caps->dwSSBCaps = DDCAPS_BLT;
    };
    fill(driver_caps);
    fill(hel_caps);
    return DD_OK;
}

HRESULT IDirectDraw::WaitForVerticalBlank(DWORD, HANDLE)
{
    SDL_Delay(16);
    return DD_OK;
}

HRESULT IDirectDraw::RestoreDisplayMode()
{
    return DD_OK;
}

HRESULT IDirectDraw::Release()
{
    if (--ref_count_ == 0) {
        delete this;
    }
    return DD_OK;
}

int IDirectDraw::Width() const { return width_; }
int IDirectDraw::Height() const { return height_; }
HWND IDirectDraw::Window() const { return window_; }

extern "C" HRESULT DirectDrawCreate(LPVOID, LPDIRECTDRAW* direct_draw, LPVOID)
{
    if (!direct_draw) {
        return DDERR_INVALIDPARAMS;
    }
    *direct_draw = new IDirectDraw();
    return DD_OK;
}
