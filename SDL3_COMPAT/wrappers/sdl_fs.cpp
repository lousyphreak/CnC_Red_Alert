#include "sdl_fs.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <unordered_map>

static std::string WWFS_CurrentDirectoryPath();

namespace {
std::string WWFS_BaseDirectory()
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

std::mutex g_wwfs_current_directory_mutex;
std::string g_wwfs_current_directory;

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
    if (g_wwfs_current_directory.empty()) {
        g_wwfs_current_directory = WWFS_ProcessCurrentDirectory();
    }
    return g_wwfs_current_directory;
}

bool WWFS_PathExists(const char* path)
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

    if (WWFS_PathExists(path.c_str())) {
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
    std::scoped_lock lock(g_wwfs_current_directory_mutex);
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

    std::scoped_lock lock(g_wwfs_current_directory_mutex);
    g_wwfs_current_directory = normalized;
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
    return path && *path && SDL_GetPathInfo(WWFS_NormalizePath(path).c_str(), info);
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

    const std::string normalized = WWFS_NormalizePath(path);
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
