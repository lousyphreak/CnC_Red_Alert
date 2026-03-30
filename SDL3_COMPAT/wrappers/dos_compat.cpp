#include "dos.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <limits>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <utility>
#include <vector>

namespace {

#if defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__) && !defined(__unix__)
#define RA_REAL_WINDOWS 1
#else
#define RA_REAL_WINDOWS 0
#endif

struct FindResult {
    unsigned attrib = 0;
    uint16_t wr_time = 0;
    uint16_t wr_date = 0;
    uint32_t size = 0;
    std::string name;
};

struct FindState {
    std::vector<FindResult> matches;
    size_t index = 0;
};

std::string leaf_name(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("/\\");
    if (separator == std::string::npos) {
        return path;
    }
    return path.substr(separator + 1);
}

std::string join_path(const std::string& directory, const std::string& name)
{
    if (directory.empty() || directory == ".") {
        return name;
    }

    if (directory.back() == '/' || directory.back() == '\\') {
        return directory + name;
    }

    return directory + "/" + name;
}

bool wildcard_match_ci(const char* pattern, const char* value)
{
    if (*pattern == '\0') {
        return *value == '\0';
    }

    if (*pattern == '*') {
        while (*(pattern + 1) == '*') {
            ++pattern;
        }

        for (const char* cursor = value;; ++cursor) {
            if (wildcard_match_ci(pattern + 1, cursor)) {
                return true;
            }
            if (*cursor == '\0') {
                return false;
            }
        }
    }

    if (*value == '\0') {
        return false;
    }

    if (*pattern == '?') {
        return wildcard_match_ci(pattern + 1, value + 1);
    }

    const unsigned char lhs = static_cast<unsigned char>(*pattern);
    const unsigned char rhs = static_cast<unsigned char>(*value);
    if (std::toupper(lhs) != std::toupper(rhs)) {
        return false;
    }

    return wildcard_match_ci(pattern + 1, value + 1);
}

std::pair<unsigned short, unsigned short> make_dos_timestamp(std::time_t raw_time)
{
    std::tm local_time {};
#if RA_REAL_WINDOWS
    localtime_s(&local_time, &raw_time);
#else
    localtime_r(&raw_time, &local_time);
#endif

    unsigned year = 0;
    if (local_time.tm_year >= 80) {
        year = static_cast<unsigned>(local_time.tm_year - 80);
    }

    const unsigned short dos_date = static_cast<unsigned short>(
        ((year & 0x7fU) << 9) |
        (((static_cast<unsigned>(local_time.tm_mon) + 1U) & 0x0fU) << 5) |
        (static_cast<unsigned>(local_time.tm_mday) & 0x1fU));

    const unsigned short dos_time = static_cast<unsigned short>(
        ((static_cast<unsigned>(local_time.tm_hour) & 0x1fU) << 11) |
        ((static_cast<unsigned>(local_time.tm_min) & 0x3fU) << 5) |
        ((static_cast<unsigned>(local_time.tm_sec) / 2U) & 0x1fU));

    return {dos_date, dos_time};
}

unsigned determine_attributes(const char* name, const struct stat& status)
{
    unsigned attrib = 0;

    if (name != nullptr && name[0] == '.') {
        attrib |= _A_HIDDEN;
    }

    if ((status.st_mode & S_IWUSR) == 0) {
        attrib |= _A_RDONLY;
    }

    if (S_ISDIR(status.st_mode)) {
        attrib |= _A_SUBDIR;
    } else if (S_ISREG(status.st_mode)) {
        attrib |= _A_ARCH;
    }

    if (attrib == 0) {
        attrib = _A_NORMAL;
    }

    return attrib;
}

uint32_t clamp_to_u32(uint64_t value)
{
    const uint64_t maximum = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
    if (value > maximum) {
        return std::numeric_limits<uint32_t>::max();
    }
    return static_cast<uint32_t>(value);
}

bool matches_attributes(unsigned desired, unsigned actual)
{
    if ((desired & _A_VOLID) != 0U) {
        return (actual & _A_VOLID) != 0U;
    }

    if ((desired & _A_SUBDIR) == 0U && (actual & _A_SUBDIR) != 0U) {
        return false;
    }

    if ((desired & _A_HIDDEN) == 0U && (actual & _A_HIDDEN) != 0U) {
        return false;
    }

    if ((desired & _A_SYSTEM) == 0U && (actual & _A_SYSTEM) != 0U) {
        return false;
    }

    return true;
}

std::string translate_directory(std::string directory)
{
    std::replace(directory.begin(), directory.end(), '\\', '/');

    if (directory.size() >= 2 && std::isalpha(static_cast<unsigned char>(directory[0])) && directory[1] == ':') {
#if RA_REAL_WINDOWS
#else
        const std::string original = directory;
        directory.erase(0, 2);
        if (!directory.empty() && (directory[0] == '/' || directory[0] == '\\')) {
            directory.erase(0, 1);
        }
        if (directory.empty()) {
            return original;
        }
#endif
    }

    if (directory.empty()) {
        return ".";
    }

    return directory;
}

void release_state(find_t* block)
{
    if (block != nullptr && block->__handle != nullptr) {
        delete static_cast<FindState*>(block->__handle);
        block->__handle = nullptr;
    }
}

int populate_block(find_t* block, const FindResult& result)
{
    if (block == nullptr) {
        return -1;
    }

    block->attrib = static_cast<unsigned char>(result.attrib);
    block->wr_time = result.wr_time;
    block->wr_date = result.wr_date;
    block->size = result.size;
    std::snprintf(block->name, sizeof(block->name), "%s", result.name.c_str());
    return 0;
}

} // namespace

int _dos_findfirst(const char* path, unsigned attrib, struct find_t* block)
{
    if (path == nullptr || block == nullptr) {
        return -1;
    }

    release_state(block);

    std::string input(path);
    std::replace(input.begin(), input.end(), '\\', '/');

    std::string directory = ".";
    std::string pattern = input;

    const std::string::size_type separator = input.find_last_of('/');
    if (separator != std::string::npos) {
        directory = input.substr(0, separator + 1);
        pattern = input.substr(separator + 1);
    }

    const std::string search_root = translate_directory(directory);
    auto* state = new FindState();

    if ((attrib & _A_VOLID) != 0U) {
        struct stat status {};
        if (stat(search_root.c_str(), &status) == 0) {
            FindResult result;
            result.attrib = _A_VOLID;
            result.name = leaf_name(search_root);
            if (result.name.empty()) {
                result.name = search_root;
            }
            state->matches.push_back(std::move(result));
        }
    } else {
        DIR* directory_handle = opendir(search_root.c_str());
        if (directory_handle != nullptr) {
            for (dirent* entry = readdir(directory_handle); entry != nullptr; entry = readdir(directory_handle)) {
                const std::string name(entry->d_name);
                const std::string full_path = join_path(search_root, name);
                struct stat status {};
                if (stat(full_path.c_str(), &status) != 0) {
                    continue;
                }

                FindResult result;
                result.attrib = determine_attributes(name.c_str(), status);
                if (!matches_attributes(attrib, result.attrib)) {
                    continue;
                }

                const auto [wr_date, wr_time] = make_dos_timestamp(status.st_mtime);
                result.wr_date = wr_date;
                result.wr_time = wr_time;
                result.name = name;
                result.size = S_ISREG(status.st_mode) ? clamp_to_u32(static_cast<uint64_t>(status.st_size)) : 0U;

                if (!wildcard_match_ci(pattern.c_str(), name.c_str())) {
                    continue;
                }

                state->matches.push_back(std::move(result));
            }
            closedir(directory_handle);
        }
    }

    if (state->matches.empty()) {
        delete state;
        block->__handle = nullptr;
        return -1;
    }

    block->__handle = state;
    return populate_block(block, state->matches[0]);
}

int _dos_findnext(struct find_t* block)
{
    if (block == nullptr || block->__handle == nullptr) {
        return -1;
    }

    auto* state = static_cast<FindState*>(block->__handle);
    ++state->index;
    if (state->index >= state->matches.size()) {
        release_state(block);
        return -1;
    }

    return populate_block(block, state->matches[state->index]);
}

int _dos_getdrive(unsigned* drive)
{
    if (drive != nullptr) {
        *drive = 3;
    }
    return 0;
}

int _dos_setdrive(unsigned, unsigned* numdrives)
{
    if (numdrives != nullptr) {
        *numdrives = 26;
    }
    return 0;
}

int _dos_getdiskfree(unsigned, struct diskfree_t* diskspace)
{
    if (diskspace == nullptr) {
        return -1;
    }

    struct statvfs info {};
    if (statvfs(".", &info) != 0) {
        return -1;
    }

    uint32_t cluster_size = clamp_to_u32(info.f_frsize != 0 ? static_cast<uint64_t>(info.f_frsize)
                                                             : static_cast<uint64_t>(info.f_bsize));
    if (cluster_size == 0U) {
        cluster_size = 4096U;
    }

    diskspace->bytes_per_sector = 512U;
    diskspace->sectors_per_cluster = cluster_size / diskspace->bytes_per_sector;
    if (diskspace->sectors_per_cluster == 0U) {
        diskspace->sectors_per_cluster = 1U;
    }
    diskspace->avail_clusters = clamp_to_u32(static_cast<uint64_t>(info.f_bavail));
    diskspace->total_clusters = clamp_to_u32(static_cast<uint64_t>(info.f_blocks));
    return 0;
}

void _harderr(int (*)(unsigned int, unsigned int, unsigned int*))
{
}

void _hardresume(int)
{
}
