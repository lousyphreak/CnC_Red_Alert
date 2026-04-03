#include "sdl_fs.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>
#include <vector>

#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
#include <emscripten.h>
#endif

static std::string WWFS_CurrentDirectoryPath();

namespace {
std::mutex& WWFS_BaseDirectoryMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::string& WWFS_BaseDirectoryOverride()
{
    static std::string path;
    return path;
}

std::string WWFS_DefaultBaseDirectory()
{
    static const std::string path = []() {
        const char* base_path = SDL_GetBasePath();
        if (base_path && *base_path) {
            std::string path_string(base_path);
            while (!path_string.empty() && (path_string.back() == '/' || path_string.back() == '\\')) {
                path_string.pop_back();
            }
            return path_string.empty() ? std::string(".") : path_string;
        }

        return std::string(".");
    }();

    return path;
}

std::string WWFS_BaseDirectory()
{
    std::scoped_lock lock(WWFS_BaseDirectoryMutex());
    if (!WWFS_BaseDirectoryOverride().empty()) {
        return WWFS_BaseDirectoryOverride();
    }
    return WWFS_DefaultBaseDirectory();
}

std::string WWFS_ProcessCurrentDirectory()
{
    char* current_directory = SDL_GetCurrentDirectory();
    if (current_directory && *current_directory) {
        std::string path(current_directory);
        SDL_free(current_directory);
        std::replace(path.begin(), path.end(), '\\', '/');
        while (path.size() > 1 && path.back() == '/') {
            path.pop_back();
        }
        return path;
    }

    if (current_directory) {
        SDL_free(current_directory);
    }
    return WWFS_BaseDirectory();
}

std::mutex& WWFS_CurrentDirectoryMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::string& WWFS_CurrentDirectoryStorage()
{
    static std::string path;
    return path;
}

bool WWFS_GetPathInfoAbsolute(const std::string& normalized_path, SDL_PathInfo* info);
bool WWFS_PathExists(const char* path);

bool WWFS_IsAbsolutePath(const std::string& path)
{
    return !path.empty() && (path[0] == '/' || path[0] == '\\' || (path.size() > 1 && path[1] == ':'));
}

std::string WWFS_StripTrailingPathSeparators(std::string path)
{
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        if (path.size() == 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
            break;
        }
        path.pop_back();
    }
    return path;
}

std::string WWFS_CurrentDirectoryPathLocked()
{
    std::string& current_directory = WWFS_CurrentDirectoryStorage();
    if (current_directory.empty()) {
        current_directory = WWFS_ProcessCurrentDirectory();
    }
    return current_directory;
}

bool WWFS_LocalPathExists(const char* path)
{
    if (!path || !*path) {
        return false;
    }

    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info);
}

std::string WWFS_AppendPathComponent(const std::string& base, const std::string& component)
{
    if (base.empty()) {
        return component;
    }

    if (base == "/" || base.back() == '/' || base.back() == '\\') {
        return base + component;
    }

    return base + "/" + component;
}

#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
constexpr char kWWFSEmscriptenAssetManifestPath[] = "/ra-assets-manifest.txt";
constexpr char kWWFSEmscriptenDefaultAssetBaseUrl[] = "../GameData/";
constexpr Sint64 kWWFSEmscriptenRangeChunkSize = 256 * 1024;

struct WWFS_EmscriptenRangeFileCache {
    std::string remote_relative_path;
    Sint64 size = -1;
    std::unordered_map<Sint64, std::vector<unsigned char>> chunks;
    std::mutex mutex;
};

struct WWFS_EmscriptenRangeStream {
    std::shared_ptr<WWFS_EmscriptenRangeFileCache> file;
    Sint64 position = 0;
};

std::mutex& WWFS_EmscriptenCacheMutex()
{
    static std::mutex mutex;
    return mutex;
}

bool& WWFS_EmscriptenCacheInitialized()
{
    static bool initialized = false;
    return initialized;
}

std::mutex& WWFS_EmscriptenManifestMutex()
{
    static std::mutex mutex;
    return mutex;
}

bool& WWFS_EmscriptenManifestLoaded()
{
    static bool loaded = false;
    return loaded;
}

std::unordered_map<std::string, std::string>& WWFS_EmscriptenAssetManifest()
{
    static std::unordered_map<std::string, std::string> manifest;
    return manifest;
}

std::mutex& WWFS_EmscriptenRangeCacheMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<std::string, std::shared_ptr<WWFS_EmscriptenRangeFileCache>>& WWFS_EmscriptenRangeFileCaches()
{
    static std::unordered_map<std::string, std::shared_ptr<WWFS_EmscriptenRangeFileCache>> caches;
    return caches;
}

std::string WWFS_FoldPath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return path;
}

bool WWFS_IsReadOnlyOpenMode(const char* mode)
{
    return mode && std::strchr(mode, 'r') != nullptr && std::strchr(mode, '+') == nullptr
        && std::strchr(mode, 'w') == nullptr && std::strchr(mode, 'a') == nullptr;
}

bool WWFS_IsMixAssetPath(const std::string& path)
{
    const std::string folded_path = WWFS_FoldPath(path);
    return folded_path.size() >= 4 && folded_path.compare(folded_path.size() - 4, 4, ".mix") == 0;
}

bool WWFS_PathIsWithinBaseDirectory(const std::string& normalized_path, std::string* relative_path = nullptr)
{
    const std::string base_path = WWFS_BaseDirectory();
    if (base_path.empty()) {
        return false;
    }

    if (normalized_path == base_path) {
        if (relative_path) {
            relative_path->clear();
        }
        return true;
    }

    if (base_path == "/") {
        if (normalized_path.empty() || normalized_path.front() != '/') {
            return false;
        }
        if (relative_path) {
            *relative_path = normalized_path.substr(1);
        }
        return true;
    }

    if (normalized_path.size() <= base_path.size()) {
        return false;
    }
    if (normalized_path.compare(0, base_path.size(), base_path) != 0) {
        return false;
    }

    const char separator = normalized_path[base_path.size()];
    if (separator != '/' && separator != '\\') {
        return false;
    }

    if (relative_path) {
        *relative_path = normalized_path.substr(base_path.size() + 1);
    }
    return true;
}

bool WWFS_LoadEmscriptenAssetManifest()
{
    std::scoped_lock lock(WWFS_EmscriptenManifestMutex());
    bool& manifest_loaded = WWFS_EmscriptenManifestLoaded();
    std::unordered_map<std::string, std::string>& asset_manifest = WWFS_EmscriptenAssetManifest();
    if (manifest_loaded) {
        return !asset_manifest.empty();
    }

    manifest_loaded = true;

    SDL_IOStream* stream = SDL_IOFromFile(kWWFSEmscriptenAssetManifestPath, "rb");
    if (!stream) {
        return false;
    }

    const Sint64 end = SDL_SeekIO(stream, 0, SDL_IO_SEEK_END);
    if (end < 0 || SDL_SeekIO(stream, 0, SDL_IO_SEEK_SET) < 0) {
        SDL_CloseIO(stream);
        return false;
    }

    std::string manifest_text;
    manifest_text.resize(static_cast<size_t>(end));
    if (!manifest_text.empty()) {
        const size_t bytes_read = SDL_ReadIO(stream, manifest_text.data(), manifest_text.size());
        manifest_text.resize(bytes_read);
    }

    SDL_CloseIO(stream);

    size_t cursor = 0;
    while (cursor < manifest_text.size()) {
        const size_t next_newline = manifest_text.find('\n', cursor);
        const size_t line_end = (next_newline == std::string::npos) ? manifest_text.size() : next_newline;
        std::string line = manifest_text.substr(cursor, line_end - cursor);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            std::replace(line.begin(), line.end(), '\\', '/');
            asset_manifest[WWFS_FoldPath(line)] = line;
        }

        if (next_newline == std::string::npos) {
            break;
        }
        cursor = next_newline + 1;
    }

    return !asset_manifest.empty();
}

bool WWFS_ResolveEmscriptenAssetPath(const std::string& normalized_path, std::string* local_path, std::string* remote_relative_path)
{
    std::string relative_path;
    if (!WWFS_PathIsWithinBaseDirectory(normalized_path, &relative_path) || relative_path.empty()) {
        return false;
    }
    if (!WWFS_LoadEmscriptenAssetManifest()) {
        return false;
    }

    const std::unordered_map<std::string, std::string>& asset_manifest = WWFS_EmscriptenAssetManifest();
    const std::string folded_relative_path = WWFS_FoldPath(relative_path);
    const auto entry = asset_manifest.find(folded_relative_path);
    if (entry == asset_manifest.end()) {
        return false;
    }

    if (remote_relative_path) {
        *remote_relative_path = entry->second;
    }
    if (local_path) {
        *local_path = WWFS_AppendPathComponent(WWFS_BaseDirectory(), entry->second);
    }
    return true;
}

EM_ASYNC_JS(int, wwfs_emscripten_init_cache_js, (const char* base_dir_c), {
    const baseDir = UTF8ToString(base_dir_c);
    const ensureDir = (path) => {
        if (!path || path === '/') {
            return;
        }

        const parts = path.split('/');
        let current = '';
        for (const part of parts) {
            if (!part) {
                continue;
            }

            current += '/' + part;
            const analyzed = FS.analyzePath(current);
            if (!analyzed.exists) {
                FS.mkdir(current);
            }
        }
    };

    try {
        ensureDir(baseDir);
        if (!Module.raWWFSCacheMounted) {
            FS.mount(IDBFS, {}, baseDir);
            Module.raWWFSCacheMounted = true;
        }
        if (!Module.raWWFSCacheInitPromise) {
            Module.raWWFSCacheInitPromise = new Promise((resolve, reject) => {
                FS.syncfs(true, (err) => err ? reject(err) : resolve());
            });
        }

        await Module.raWWFSCacheInitPromise;
        return 1;
    } catch (err) {
        console.error('Red Alert Emscripten cache initialization failed:', err);
        Module.raWWFSCacheInitPromise = null;
        return 0;
    }
});

EM_ASYNC_JS(int, wwfs_emscripten_fetch_asset_js, (const char* local_path_c, const char* remote_relative_c, const char* default_base_url_c), {
    const localPath = UTF8ToString(local_path_c);
    const remoteRelative = UTF8ToString(remote_relative_c);
    const defaultBaseUrl = UTF8ToString(default_base_url_c);

    const ensureDir = (path) => {
        if (!path || path === '/') {
            return;
        }

        const parts = path.split('/');
        let current = '';
        for (const part of parts) {
            if (!part) {
                continue;
            }

            current += '/' + part;
            const analyzed = FS.analyzePath(current);
            if (!analyzed.exists) {
                FS.mkdir(current);
            }
        }
    };

    const configuredBaseUrl = Module.raAssetBaseUrl || globalThis.RA_ASSET_BASE_URL || defaultBaseUrl;
    const absoluteBaseUrl = new URL(configuredBaseUrl, globalThis.location.href);
    const assetUrl = new URL(remoteRelative, absoluteBaseUrl);

    try {
        if (FS.analyzePath(localPath).exists) {
            return 1;
        }

        const lastSlash = localPath.lastIndexOf('/');
        if (lastSlash > 0) {
            ensureDir(localPath.substring(0, lastSlash));
        }

        const response = await fetch(assetUrl, { credentials: 'same-origin' });
        if (!response.ok) {
            console.error(`Red Alert asset fetch failed for ${assetUrl}: ${response.status} ${response.statusText}`);
            return 0;
        }

        const bytes = new Uint8Array(await response.arrayBuffer());
        FS.writeFile(localPath, bytes, { canOwn: true });

        await new Promise((resolve, reject) => {
            FS.syncfs(false, (err) => err ? reject(err) : resolve());
        });

        return 1;
    } catch (err) {
        console.error(`Red Alert asset fetch failed for ${assetUrl}:`, err);
        return 0;
    }
});

EM_ASYNC_JS(int, wwfs_emscripten_query_range_asset_size_js, (const char* remote_relative_c, const char* default_base_url_c), {
    const remoteRelative = UTF8ToString(remote_relative_c);
    const defaultBaseUrl = UTF8ToString(default_base_url_c);
    const configuredBaseUrl = Module.raAssetBaseUrl || globalThis.RA_ASSET_BASE_URL || defaultBaseUrl;
    const absoluteBaseUrl = new URL(configuredBaseUrl, globalThis.location.href);
    const assetUrl = new URL(remoteRelative, absoluteBaseUrl);

    try {
        const response = await fetch(assetUrl, {
            credentials: 'same-origin',
            headers: { 'Range': 'bytes=0-0' }
        });

        if (!response.ok) {
            console.error(`Red Alert range size probe failed for ${assetUrl}: ${response.status} ${response.statusText}`);
            return -1;
        }

        if (response.status !== 206) {
            if (response.body) {
                try {
                    await response.body.cancel();
                } catch (err) {
                }
            }
            return -2;
        }

        const contentRange = response.headers.get('content-range') || response.headers.get('Content-Range') || '';
        const slash = contentRange.lastIndexOf('/');
        if (slash < 0) {
            await response.arrayBuffer();
            return -1;
        }

        await response.arrayBuffer();
        const totalSize = Number.parseInt(contentRange.substring(slash + 1), 10);
        return Number.isFinite(totalSize) && totalSize >= 0 ? totalSize : -1;
    } catch (err) {
        console.error(`Red Alert range size probe failed for ${assetUrl}:`, err);
        return -1;
    }
});

EM_ASYNC_JS(int, wwfs_emscripten_fetch_asset_range_js, (const char* remote_relative_c, const char* default_base_url_c, int offset, int length, unsigned char* dest, int dest_capacity), {
    const remoteRelative = UTF8ToString(remote_relative_c);
    const defaultBaseUrl = UTF8ToString(default_base_url_c);
    const configuredBaseUrl = Module.raAssetBaseUrl || globalThis.RA_ASSET_BASE_URL || defaultBaseUrl;
    const absoluteBaseUrl = new URL(configuredBaseUrl, globalThis.location.href);
    const assetUrl = new URL(remoteRelative, absoluteBaseUrl);

    try {
        const response = await fetch(assetUrl, {
            credentials: 'same-origin',
            headers: { 'Range': `bytes=${offset}-${offset + length - 1}` }
        });

        if (!response.ok) {
            console.error(`Red Alert range fetch failed for ${assetUrl}: ${response.status} ${response.statusText}`);
            return -1;
        }

        if (response.status !== 206) {
            if (response.body) {
                try {
                    await response.body.cancel();
                } catch (err) {
                }
            }
            return -2;
        }

        const bytes = new Uint8Array(await response.arrayBuffer());
        const actual = Math.min(bytes.length, dest_capacity);
        HEAPU8.set(bytes.subarray(0, actual), dest);
        return actual;
    } catch (err) {
        console.error(`Red Alert range fetch failed for ${assetUrl}:`, err);
        return -1;
    }
});

std::shared_ptr<WWFS_EmscriptenRangeFileCache> WWFS_GetEmscriptenRangeFileCache(const std::string& normalized_path)
{
    std::string actual_local_path;
    std::string remote_relative_path;
    if (!WWFS_ResolveEmscriptenAssetPath(normalized_path, &actual_local_path, &remote_relative_path)
        || !WWFS_IsMixAssetPath(remote_relative_path)
        || WWFS_LocalPathExists(actual_local_path.c_str())) {
        return {};
    }

    const std::string cache_key = WWFS_FoldPath(remote_relative_path);
    {
        std::scoped_lock lock(WWFS_EmscriptenRangeCacheMutex());
        const auto existing = WWFS_EmscriptenRangeFileCaches().find(cache_key);
        if (existing != WWFS_EmscriptenRangeFileCaches().end()) {
            return existing->second;
        }
    }

    const int range_size = wwfs_emscripten_query_range_asset_size_js(remote_relative_path.c_str(), kWWFSEmscriptenDefaultAssetBaseUrl);
    if (range_size <= 0) {
        if (range_size == -2) {
            SDL_SetError("HTTP range requests are not available for '%s'", remote_relative_path.c_str());
        } else {
            SDL_SetError("Failed to query the size of remote asset '%s'", remote_relative_path.c_str());
        }
        return {};
    }

    std::shared_ptr<WWFS_EmscriptenRangeFileCache> file_cache(new (std::nothrow) WWFS_EmscriptenRangeFileCache());
    if (!file_cache) {
        SDL_OutOfMemory();
        return {};
    }
    file_cache->remote_relative_path = remote_relative_path;
    file_cache->size = static_cast<Sint64>(range_size);

    std::scoped_lock lock(WWFS_EmscriptenRangeCacheMutex());
    auto& caches = WWFS_EmscriptenRangeFileCaches();
    const auto inserted = caches.emplace(cache_key, file_cache);
    return inserted.second ? file_cache : inserted.first->second;
}

bool WWFS_EnsureEmscriptenRangeChunkLoaded(const std::shared_ptr<WWFS_EmscriptenRangeFileCache>& file_cache, Sint64 chunk_index)
{
    if (!file_cache || chunk_index < 0) {
        return false;
    }

    {
        std::scoped_lock lock(file_cache->mutex);
        if (file_cache->chunks.find(chunk_index) != file_cache->chunks.end()) {
            return true;
        }
    }

    const Sint64 chunk_offset = chunk_index * kWWFSEmscriptenRangeChunkSize;
    if (chunk_offset >= file_cache->size) {
        return false;
    }

    const Sint64 max_chunk_size = file_cache->size - chunk_offset;
    const size_t requested_size = static_cast<size_t>(std::min(max_chunk_size, kWWFSEmscriptenRangeChunkSize));
    std::vector<unsigned char> chunk_data(requested_size);
    const int bytes_fetched = wwfs_emscripten_fetch_asset_range_js(file_cache->remote_relative_path.c_str(),
        kWWFSEmscriptenDefaultAssetBaseUrl,
        static_cast<int>(chunk_offset),
        static_cast<int>(requested_size),
        chunk_data.data(),
        static_cast<int>(chunk_data.size()));
    if (bytes_fetched <= 0) {
        if (bytes_fetched == -2) {
            SDL_SetError("HTTP range requests are not available for '%s'", file_cache->remote_relative_path.c_str());
        } else {
            SDL_SetError("Failed to fetch a range from remote asset '%s'", file_cache->remote_relative_path.c_str());
        }
        return false;
    }

    chunk_data.resize(static_cast<size_t>(bytes_fetched));

    std::scoped_lock lock(file_cache->mutex);
    auto& chunks = file_cache->chunks;
    if (chunks.find(chunk_index) == chunks.end()) {
        chunks.emplace(chunk_index, std::move(chunk_data));
    }
    return true;
}

Sint64 SDLCALL WWFS_EmscriptenRangeStreamSize(void* userdata)
{
    const auto* stream = static_cast<WWFS_EmscriptenRangeStream*>(userdata);
    return (stream && stream->file) ? stream->file->size : -1;
}

Sint64 SDLCALL WWFS_EmscriptenRangeStreamSeek(void* userdata, Sint64 offset, SDL_IOWhence whence)
{
    auto* stream = static_cast<WWFS_EmscriptenRangeStream*>(userdata);
    if (!stream || !stream->file) {
        SDL_SetError("Invalid Emscripten range stream");
        return -1;
    }

    Sint64 base = 0;
    switch (whence) {
    case SDL_IO_SEEK_SET:
        base = 0;
        break;
    case SDL_IO_SEEK_CUR:
        base = stream->position;
        break;
    case SDL_IO_SEEK_END:
        base = stream->file->size;
        break;
    default:
        SDL_SetError("Invalid Emscripten range seek mode");
        return -1;
    }

    if ((offset < 0 && base < -offset) || (offset > 0 && base > LLONG_MAX - offset)) {
        SDL_SetError("Emscripten range seek overflow");
        return -1;
    }

    const Sint64 new_position = base + offset;
    if (new_position < 0) {
        SDL_SetError("Emscripten range seek before start of file");
        return -1;
    }

    stream->position = new_position;
    return stream->position;
}

size_t SDLCALL WWFS_EmscriptenRangeStreamRead(void* userdata, void* ptr, size_t size, SDL_IOStatus* status)
{
    auto* stream = static_cast<WWFS_EmscriptenRangeStream*>(userdata);
    if (!stream || !stream->file || !ptr) {
        if (status) {
            *status = SDL_IO_STATUS_ERROR;
        }
        SDL_SetError("Invalid Emscripten range stream read");
        return 0;
    }

    if (stream->position >= stream->file->size) {
        if (status) {
            *status = SDL_IO_STATUS_EOF;
        }
        return 0;
    }

    const size_t requested = static_cast<size_t>(std::min<Sint64>(static_cast<Sint64>(size), stream->file->size - stream->position));
    unsigned char* output = static_cast<unsigned char*>(ptr);
    size_t copied = 0;

    while (copied < requested) {
        const Sint64 absolute_offset = stream->position + static_cast<Sint64>(copied);
        const Sint64 chunk_index = absolute_offset / kWWFSEmscriptenRangeChunkSize;
        if (!WWFS_EnsureEmscriptenRangeChunkLoaded(stream->file, chunk_index)) {
            if (status) {
                *status = SDL_IO_STATUS_ERROR;
            }
            return copied;
        }

        std::scoped_lock lock(stream->file->mutex);
        const auto chunk = stream->file->chunks.find(chunk_index);
        if (chunk == stream->file->chunks.end()) {
            if (status) {
                *status = SDL_IO_STATUS_ERROR;
            }
            SDL_SetError("Missing cached Emscripten range chunk");
            return copied;
        }

        const size_t chunk_offset = static_cast<size_t>(absolute_offset % kWWFSEmscriptenRangeChunkSize);
        if (chunk_offset >= chunk->second.size()) {
            break;
        }

        const size_t available = chunk->second.size() - chunk_offset;
        const size_t to_copy = std::min(available, requested - copied);
        std::memcpy(output + copied, chunk->second.data() + chunk_offset, to_copy);
        copied += to_copy;
    }

    stream->position += static_cast<Sint64>(copied);
    if (status && copied < size) {
        *status = (stream->position >= stream->file->size) ? SDL_IO_STATUS_EOF : SDL_IO_STATUS_READY;
    }
    return copied;
}

size_t SDLCALL WWFS_EmscriptenRangeStreamWrite(void*, const void*, size_t, SDL_IOStatus* status)
{
    if (status) {
        *status = SDL_IO_STATUS_READONLY;
    }
    SDL_SetError("Emscripten range streams are read-only");
    return 0;
}

bool SDLCALL WWFS_EmscriptenRangeStreamFlush(void*, SDL_IOStatus* status)
{
    if (status) {
        *status = SDL_IO_STATUS_READY;
    }
    return true;
}

bool SDLCALL WWFS_EmscriptenRangeStreamClose(void* userdata)
{
    delete static_cast<WWFS_EmscriptenRangeStream*>(userdata);
    return true;
}

const SDL_IOStreamInterface& WWFS_EmscriptenRangeStreamInterface()
{
    static const SDL_IOStreamInterface interface = []() {
        SDL_IOStreamInterface iface;
        SDL_INIT_INTERFACE(&iface);
        iface.size = WWFS_EmscriptenRangeStreamSize;
        iface.seek = WWFS_EmscriptenRangeStreamSeek;
        iface.read = WWFS_EmscriptenRangeStreamRead;
        iface.write = WWFS_EmscriptenRangeStreamWrite;
        iface.flush = WWFS_EmscriptenRangeStreamFlush;
        iface.close = WWFS_EmscriptenRangeStreamClose;
        return iface;
    }();

    return interface;
}

SDL_IOStream* WWFS_OpenEmscriptenRangeStream(const std::string& normalized_path)
{
    const std::shared_ptr<WWFS_EmscriptenRangeFileCache> file_cache = WWFS_GetEmscriptenRangeFileCache(normalized_path);
    if (!file_cache) {
        return nullptr;
    }

    WWFS_EmscriptenRangeStream* stream_state = new (std::nothrow) WWFS_EmscriptenRangeStream();
    if (!stream_state) {
        SDL_OutOfMemory();
        return nullptr;
    }

    stream_state->file = file_cache;
    SDL_IOStream* stream = SDL_OpenIO(&WWFS_EmscriptenRangeStreamInterface(), stream_state);
    if (!stream) {
        delete stream_state;
        return nullptr;
    }

    return stream;
}

bool WWFS_GetSyntheticEmscriptenPathInfo(const std::string& normalized_path, SDL_PathInfo* info)
{
    std::string actual_local_path;
    if (!WWFS_ResolveEmscriptenAssetPath(normalized_path, &actual_local_path, nullptr)) {
        return false;
    }

    if (WWFS_LocalPathExists(actual_local_path.c_str())) {
        return SDL_GetPathInfo(actual_local_path.c_str(), info);
    }

    if (info) {
        info->type = SDL_PATHTYPE_FILE;
        info->size = 0;
        info->create_time = 0;
        info->modify_time = 0;
        info->access_time = 0;
    }

    return true;
}

bool WWFS_EnsureEmscriptenAssetCached(const std::string& normalized_path, std::string* resolved_path)
{
    std::string actual_local_path;
    std::string remote_relative_path;
    if (!WWFS_ResolveEmscriptenAssetPath(normalized_path, &actual_local_path, &remote_relative_path)) {
        return false;
    }

    if (WWFS_LocalPathExists(actual_local_path.c_str())) {
        if (resolved_path) {
            *resolved_path = actual_local_path;
        }
        return true;
    }

    if (!WWFS_InitializeEmscriptenAssetCache()) {
        return false;
    }

    if (WWFS_LocalPathExists(actual_local_path.c_str())) {
        if (resolved_path) {
            *resolved_path = actual_local_path;
        }
        return true;
    }

    if (!wwfs_emscripten_fetch_asset_js(actual_local_path.c_str(), remote_relative_path.c_str(), kWWFSEmscriptenDefaultAssetBaseUrl)) {
        SDL_SetError("Failed to fetch Emscripten asset '%s'", remote_relative_path.c_str());
        return false;
    }

    if (!WWFS_LocalPathExists(actual_local_path.c_str())) {
        SDL_SetError("Fetched Emscripten asset '%s' was not written to '%s'", remote_relative_path.c_str(), actual_local_path.c_str());
        return false;
    }

    if (resolved_path) {
        *resolved_path = actual_local_path;
    }
    return true;
}
#else
bool WWFS_GetSyntheticEmscriptenPathInfo(const std::string&, SDL_PathInfo*)
{
    return false;
}

bool WWFS_EnsureEmscriptenAssetCached(const std::string&, std::string*)
{
    return false;
}
#endif

bool WWFS_GetPathInfoAbsolute(const std::string& normalized_path, SDL_PathInfo* info)
{
    if (normalized_path.empty()) {
        return false;
    }

    if (SDL_GetPathInfo(normalized_path.c_str(), info)) {
        return true;
    }

    return WWFS_GetSyntheticEmscriptenPathInfo(normalized_path, info);
}

bool WWFS_PathExists(const char* path)
{
    return path && *path && WWFS_GetPathInfoAbsolute(path, nullptr);
}

struct CaseMatchContext {
    const char* requested_name;
    std::string matched_name;
    std::string folded_match;
    bool exact_found;
};

SDL_EnumerationResult case_match_callback(void* userdata, const char* dirname, const char* fname)
{
    (void)dirname;
    auto* ctx = static_cast<CaseMatchContext*>(userdata);

    if (SDL_strcmp(fname, ctx->requested_name) == 0) {
        ctx->matched_name = fname;
        ctx->exact_found = true;
        return SDL_ENUM_SUCCESS;
    }

    if (ctx->folded_match.empty() && SDL_strcasecmp(fname, ctx->requested_name) == 0) {
        ctx->folded_match = fname;
    }

    return SDL_ENUM_CONTINUE;
}

std::string resolve_existing_case_insensitive_path(const std::string& path)
{
    if (path.empty()) {
        return {};
    }

    if (WWFS_LocalPathExists(path.c_str())) {
        return path;
    }

    std::string current = path.front() == '/' ? std::string("/") : std::string();
    std::string::size_type cursor = (path.front() == '/') ? 1 : 0;

    while (cursor <= path.size()) {
        const std::string::size_type separator = path.find('/', cursor);
        const std::string requested_name = path.substr(cursor, separator - cursor);
        cursor = (separator == std::string::npos) ? path.size() + 1 : separator + 1;

        if (requested_name.empty() || requested_name == ".") {
            continue;
        }

        if (requested_name == "..") {
            current = WWFS_AppendPathComponent(current, requested_name);
            continue;
        }

        const std::string search_directory = current.empty() ? std::string(".") : current;

        CaseMatchContext ctx;
        ctx.requested_name = requested_name.c_str();
        ctx.exact_found = false;

        if (!SDL_EnumerateDirectory(search_directory.c_str(), case_match_callback, &ctx)) {
            return {};
        }

        std::string final_match = ctx.exact_found ? ctx.matched_name : ctx.folded_match;
        if (final_match.empty()) {
            return {};
        }

        current = WWFS_AppendPathComponent(current, final_match);
    }

    return current.empty() ? path : current;
}

int virtual_cd_index_for_drive_letter(char drive_letter)
{
    const int index = std::tolower(static_cast<unsigned char>(drive_letter)) - 'c';
    if (index < 0 || index >= 4) {
        return -1;
    }

    char mix_name[16];
    std::snprintf(mix_name, sizeof(mix_name), "MAIN%d.MIX", index + 1);

    std::string mix_path = WWFS_BaseDirectory();
    if (!mix_path.empty() && mix_path.back() != '/' && mix_path.back() != '\\') {
        mix_path.push_back('/');
    }
    mix_path += mix_name;

    if (!WWFS_PathExists(mix_path.c_str())) {
        return -1;
    }

    return index;
}

bool resolve_virtual_cd_path(const char* windows_path, std::string& resolved_path, int* cd_index = nullptr)
{
    if (!windows_path || std::strlen(windows_path) < 2 || windows_path[1] != ':') {
        return false;
    }

    const int index = virtual_cd_index_for_drive_letter(windows_path[0]);
    if (index < 0) {
        return false;
    }

    std::string relative = windows_path + 2;
    std::replace(relative.begin(), relative.end(), '\\', '/');
    while (!relative.empty() && relative.front() == '/') {
        relative.erase(relative.begin());
    }

    const std::string base_path = WWFS_BaseDirectory();
    if (relative.empty()) {
        resolved_path = base_path;
    } else if (SDL_strcasecmp(relative.c_str(), "main.mix") == 0) {
        char mix_name[16];
        std::snprintf(mix_name, sizeof(mix_name), "MAIN%d.MIX", index + 1);
        resolved_path = base_path + "/" + mix_name;
    } else {
        resolved_path = base_path + "/" + relative;
    }

    if (cd_index) {
        *cd_index = index;
    }
    return true;
}


} // end anonymous namespace

void WWFS_SetBaseDirectory(const char* path)
{
    std::scoped_lock lock(WWFS_BaseDirectoryMutex());
    std::string& base_directory_override = WWFS_BaseDirectoryOverride();
    if (!path || !*path) {
        base_directory_override.clear();
        return;
    }

    std::string normalized(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    base_directory_override = WWFS_StripTrailingPathSeparators(normalized);
    if (base_directory_override.empty()) {
        base_directory_override = "/";
    }
}

std::string WWFS_GetBaseDirectoryPath()
{
    return WWFS_BaseDirectory();
}

bool WWFS_InitializeEmscriptenAssetCache()
{
#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
    std::scoped_lock lock(WWFS_EmscriptenCacheMutex());
    bool& cache_initialized = WWFS_EmscriptenCacheInitialized();
    if (cache_initialized) {
        return true;
    }

    const std::string base_directory = WWFS_BaseDirectory();
    if (base_directory.empty()) {
        SDL_SetError("Emscripten asset cache base directory is empty");
        return false;
    }

    if (!wwfs_emscripten_init_cache_js(base_directory.c_str())) {
        SDL_SetError("Failed to initialize the Emscripten asset cache at '%s'", base_directory.c_str());
        return false;
    }

    cache_initialized = true;
#endif
    return true;
}

int WWFS_GetVirtualCDIndexForDriveLetter(char drive_letter)
{
    return virtual_cd_index_for_drive_letter(drive_letter);
}

bool WWFS_ResolveVirtualCDPath(const char* windows_path, std::string& resolved_path, int* cd_index)
{
    return resolve_virtual_cd_path(windows_path, resolved_path, cd_index);
}

unsigned WWFS_GetCurrentDriveNumber()
{
    char cwd[PATH_MAX];
    if (WWFS_GetCurrentDirectory(cwd, sizeof(cwd)) != nullptr) {
        const unsigned char drive = static_cast<unsigned char>(cwd[0]);
        if (std::isalpha(drive) && cwd[1] == ':') {
            return static_cast<unsigned>(std::toupper(drive) - 'A' + 1);
        }
    }
    return 3;
}

unsigned WWFS_GetDriveCount()
{
    return 26;
}

void WWFS_ChangeToDrive(unsigned drive)
{
    if (drive >= 1 && drive <= WWFS_GetDriveCount()) {
        char root[4] = {'A', ':', '\\', '\0'};
        root[0] = static_cast<char>('A' + drive - 1);
        WWFS_ChangeDirectory(root);
    }
}

void WWFS_SplitPath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
    if (drive) {
        drive[0] = '\0';
    }
    if (dir) {
        dir[0] = '\0';
    }
    if (fname) {
        fname[0] = '\0';
    }
    if (ext) {
        ext[0] = '\0';
    }
    if (!path) {
        return;
    }

    const char* component_start = path;
    const char* colon = SDL_strchr(path, ':');
    if (colon) {
        if (drive) {
            const size_t drive_length = static_cast<size_t>(colon - path + 1);
            SDL_memcpy(drive, path, drive_length);
            drive[drive_length] = '\0';
        }
        component_start = colon + 1;
    }

    const char* separator = SDL_strrchr(component_start, '/');
    const char* backslash = SDL_strrchr(component_start, '\\');
    if (!separator || (backslash && backslash > separator)) {
        separator = backslash;
    }

    const char* dot = SDL_strrchr(component_start, '.');
    if (dot && separator && dot < separator) {
        dot = nullptr;
    }

    if (dir && separator) {
        const size_t dir_length = static_cast<size_t>(separator - component_start + 1);
        SDL_memcpy(dir, component_start, dir_length);
        dir[dir_length] = '\0';
    }

    const char* fname_start = separator ? separator + 1 : component_start;
    const char* fname_end = dot ? dot : path + SDL_strlen(path);
    if (fname && fname_end >= fname_start) {
        const size_t fname_length = static_cast<size_t>(fname_end - fname_start);
        SDL_memcpy(fname, fname_start, fname_length);
        fname[fname_length] = '\0';
    }

    if (ext && dot) {
        SDL_memcpy(ext, dot, SDL_strlen(dot) + 1);
    }
}

void WWFS_MakePath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
    path[0] = '\0';

    if (drive && drive[0] != '\0') {
        std::strcat(path, drive);
    }

    if (dir && dir[0] != '\0') {
        std::strcat(path, dir);
    }

    if (fname && fname[0] != '\0') {
        std::strcat(path, fname);
    }

    if (ext && ext[0] != '\0') {
        if (ext[0] != '.') {
            std::strcat(path, ".");
        }
        std::strcat(path, ext);
    }
}

std::string WWFS_NormalizePath(const char* windows_path)
{
    std::string resolved_path;
    if (WWFS_ResolveVirtualCDPath(windows_path, resolved_path)) {
        return resolved_path;
    }

    std::string normalized = windows_path ? windows_path : "";
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    if (normalized.empty()) {
        return WWFS_CurrentDirectoryPath();
    }

    if (!WWFS_IsAbsolutePath(normalized)) {
        normalized = WWFS_AppendPathComponent(WWFS_CurrentDirectoryPath(), normalized);
    }

    const std::string resolved_existing = resolve_existing_case_insensitive_path(normalized);
    if (!resolved_existing.empty()) {
        return resolved_existing;
    }

    return normalized;
}

static std::string WWFS_CurrentDirectoryPath()
{
    std::scoped_lock lock(WWFS_CurrentDirectoryMutex());
    return WWFS_CurrentDirectoryPathLocked();
}

bool WWFS_SetCurrentDirectory(const char* path)
{
    if (!path || !*path) {
        SDL_SetError("Current directory path is empty");
        return false;
    }

    const std::string normalized = WWFS_StripTrailingPathSeparators(WWFS_NormalizePath(path));
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(normalized.c_str(), &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
        return false;
    }

    std::scoped_lock lock(WWFS_CurrentDirectoryMutex());
    WWFS_CurrentDirectoryStorage() = normalized;
    return true;
}

char* WWFS_GetCurrentDirectory(char* buffer, int max_length)
{
    if (!buffer || max_length <= 0) {
        SDL_SetError("Invalid current directory buffer");
        return nullptr;
    }

    const std::string current_directory = WWFS_CurrentDirectoryPath();
    if (current_directory.empty() || static_cast<int>(current_directory.size()) >= max_length) {
        SDL_SetError("Current directory buffer is too small");
        return nullptr;
    }

    std::memcpy(buffer, current_directory.c_str(), current_directory.size() + 1);
    return buffer;
}

bool WWFS_CreateDirectory(const char* path)
{
    return path && *path && SDL_CreateDirectory(WWFS_NormalizePath(path).c_str());
}

bool WWFS_GetPathInfo(const char* path, SDL_PathInfo* info)
{
    if (!path || !*path) {
        return false;
    }

    const std::string normalized = WWFS_NormalizePath(path);
    return WWFS_GetPathInfoAbsolute(normalized, info);
}

bool WWFS_RemovePath(const char* path)
{
    return path && *path && SDL_RemovePath(WWFS_NormalizePath(path).c_str());
}

bool WWFS_RenamePath(const char* old_path, const char* new_path)
{
    return old_path && *old_path && new_path && *new_path
        && SDL_RenamePath(WWFS_NormalizePath(old_path).c_str(), WWFS_NormalizePath(new_path).c_str());
}

char** WWFS_GlobDirectory(const char* path, const char* pattern, SDL_GlobFlags flags, int* count)
{
    if (!path || !*path) {
        if (count) {
            *count = 0;
        }
        SDL_SetError("Invalid glob path");
        return nullptr;
    }

    const std::string normalized = WWFS_NormalizePath(path);
    return SDL_GlobDirectory(normalized.c_str(), pattern, flags, count);
}

SDL_IOStream* WWFS_OpenFile(const char* path, const char* mode)
{
    if (!path || !*path || !mode) {
        SDL_SetError("Invalid file open parameters");
        return nullptr;
    }

    std::string normalized = WWFS_NormalizePath(path);
#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
    const bool allow_fetch = (std::strchr(mode, 'r') != nullptr) || (std::strchr(mode, '+') != nullptr);
    if (allow_fetch && !WWFS_LocalPathExists(normalized.c_str())) {
        if (WWFS_IsReadOnlyOpenMode(mode)) {
            SDL_IOStream* range_stream = WWFS_OpenEmscriptenRangeStream(normalized);
            if (range_stream) {
                return range_stream;
            }
            SDL_ClearError();
        }

        std::string cached_path;
        if (WWFS_EnsureEmscriptenAssetCached(normalized, &cached_path)) {
            const std::string resolved_existing = resolve_existing_case_insensitive_path(normalized);
            normalized = resolved_existing.empty() ? cached_path : resolved_existing;
        }
    }
#endif
    return SDL_IOFromFile(normalized.c_str(), mode);
}

SDL_Storage* WWFS_OpenFileStorage(const char* path)
{
    if (!path || !*path) {
        SDL_SetError("Invalid storage path");
        return nullptr;
    }

    const std::string normalized = WWFS_NormalizePath(path);
    return SDL_OpenFileStorage(normalized.c_str());
}
