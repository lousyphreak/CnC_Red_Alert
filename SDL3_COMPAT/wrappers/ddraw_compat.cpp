#include "ddraw.h"

#include <SDL3/SDL_render.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
int g_texture_width = 0;
int g_texture_height = 0;
IDirectDrawSurface* g_primary_surface = nullptr;
IDirectDrawSurface* g_pending_surface = nullptr;
RAWindow* g_pending_window = nullptr;
int g_present_batch_depth = 0;
bool g_present_pending = false;

bool ddraw_trace_enabled()
{
    static int enabled = -1;
    if (enabled == -1) {
        enabled = std::getenv("RA_TRACE_STARTUP") != nullptr ? 1 : 0;
    }
    return enabled != 0;
}

RAWindow* ensure_window(RAWindow* window, int width, int height)
{
    if (window && window->sdl_window) {
        return window;
    }

    auto* created = new RAWindow{};
    created->title = "Red Alert";
    created->width = width;
    created->height = height;
    created->sdl_window = SDL_CreateWindow(created->title.c_str(), width, height, SDL_WINDOW_RESIZABLE);
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
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (g_texture) {
            SDL_SetTextureBlendMode(g_texture, SDL_BLENDMODE_NONE);
        }
        g_texture_width = width;
        g_texture_height = height;
    }
}

void present_surface(IDirectDrawSurface* surface, RAWindow* window)
{
    static int present_trace_count = 0;
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
    size_t pixel_count = static_cast<size_t>(surface->Width()) * static_cast<size_t>(surface->Height());
    size_t nonzero = 0;
    for (size_t i = 0; i < pixel_count; ++i) {
        if (pixels[i] != 0) {
            ++nonzero;
        }
    }
    if (ddraw_trace_enabled()
        && present_trace_count < 24
        && (present_trace_count < 4 || nonzero != 0 || palette == nullptr)) {
        std::fprintf(stderr,
            "[ddraw] present palette=%p pix_nonzero=%zu/%zu idx=%u,%u,%u,%u rgb0=%u,%u,%u\n",
            static_cast<void const*>(palette),
            nonzero,
            pixel_count,
            static_cast<unsigned>(pixels[0]),
            static_cast<unsigned>(pixels[1]),
            static_cast<unsigned>(pixels[2]),
            static_cast<unsigned>(pixels[3]),
            palette ? static_cast<unsigned>(palette[pixels[0]].peRed) : 0u,
            palette ? static_cast<unsigned>(palette[pixels[0]].peGreen) : 0u,
            palette ? static_cast<unsigned>(palette[pixels[0]].peBlue) : 0u);
        std::fflush(stderr);
        ++present_trace_count;
    }

    for (int y = 0; y < surface->Height(); ++y) {
        for (int x = 0; x < surface->Width(); ++x) {
            const uint8_t index = pixels[y * surface->Width() + x];
            const PALETTEENTRY entry = palette ? palette[index] : PALETTEENTRY{index, index, index, 0};
            rgba[y * surface->Width() + x] = (0xffu << 24) | (static_cast<uint32_t>(entry.peRed) << 16)
                | (static_cast<uint32_t>(entry.peGreen) << 8) | static_cast<uint32_t>(entry.peBlue);
        }
    }

    SDL_UpdateTexture(g_texture, nullptr, rgba.data(), surface->Width() * static_cast<int>(sizeof(uint32_t)));
    SDL_FRect destination{0.0f, 0.0f, static_cast<float>(surface->Width()), static_cast<float>(surface->Height())};
    RAWindow* present_window = window ? window : surface->Window();
    if (!RA_GetPresentationRect(present_window, &destination)) {
        int output_width = 0;
        int output_height = 0;
        if (SDL_GetRenderOutputSize(g_renderer, &output_width, &output_height) && output_width > 0 && output_height > 0) {
            const float scale = std::min(
                static_cast<float>(output_width) / static_cast<float>(surface->Width()),
                static_cast<float>(output_height) / static_cast<float>(surface->Height()));
            destination.w = std::max(1.0f, static_cast<float>(surface->Width()) * scale);
            destination.h = std::max(1.0f, static_cast<float>(surface->Height()) * scale);
            destination.x = (static_cast<float>(output_width) - destination.w) * 0.5f;
            destination.y = (static_cast<float>(output_height) - destination.h) * 0.5f;
        }
    }
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, g_texture, nullptr, &destination);
    SDL_RenderPresent(g_renderer);
}

void queue_present(IDirectDrawSurface* surface, RAWindow* window)
{
    if (!surface || !surface->IsPrimary()) {
        return;
    }

    g_pending_surface = surface;
    g_pending_window = window;
    g_present_pending = true;
}

void flush_pending_present()
{
    if (!g_present_pending || g_present_batch_depth != 0) {
        return;
    }

    IDirectDrawSurface* surface = g_pending_surface ? g_pending_surface : g_primary_surface;
    RAWindow* window = g_pending_window;
    g_pending_surface = nullptr;
    g_pending_window = nullptr;
    g_present_pending = false;

    if (surface) {
        present_surface(surface, window);
    }
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
        queue_present(g_primary_surface, nullptr);
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
        queue_present(this, window_);
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
        queue_present(this, window_);
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
        queue_present(this, window_);
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
HWND IDirectDrawSurface::Window() const { return window_; }
void IDirectDrawSurface::Present()
{
    queue_present(this, window_);
    flush_pending_present();
}

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
        window_->width = width_;
        window_->height = height_;
        SDL_SetWindowResizable(window_->sdl_window, true);
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
        queue_present(*surface, window_);
        flush_pending_present();
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

extern "C" void DirectDraw_Begin_Present_Batch(void)
{
    ++g_present_batch_depth;
}

extern "C" void DirectDraw_End_Present_Batch(void)
{
    if (g_present_batch_depth <= 0) {
        return;
    }

    --g_present_batch_depth;
    flush_pending_present();
}

extern "C" void DirectDraw_Flush_Present(void)
{
    flush_pending_present();
}
