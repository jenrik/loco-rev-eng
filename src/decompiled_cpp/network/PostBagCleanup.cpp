// Status: INTEGRATED

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef _WIN32
#include <algorithm>
#include <filesystem>
#include <string>
#endif

extern char g_install_path[]; // 0x4A99C8

#ifdef _WIN32
extern "C" {
void* __stdcall CRT_FindFirstFile(const char* pattern, void* find_data);
int32_t __stdcall CRT_FindNextFile(void* handle, void* find_data);
int32_t __stdcall CRT_FindClose(void* handle);
int32_t __stdcall SetFileAttributesA(const char* path, uint32_t attributes);
int32_t __stdcall DeleteFileA(const char* path);
int32_t __stdcall wsprintfA(char* output, const char* format, ...);
}

struct LegacyFindData {
    uint8_t metadata[0x2C];
    char filename[260];
    char alternate_filename[14];
};
static_assert(offsetof(LegacyFindData, filename) == 0x2C,
              "WIN32_FIND_DATA filename offset");
#endif

/** DPLAY_ReceiveMessage — remove non-dot files matching a PostBag wildcard.
 *  Address: 0x443550 */
#ifdef _WIN32
void __stdcall DPLAY_ReceiveMessage(const char* wildcard_path)
#else
void DPLAY_ReceiveMessage(const char* wildcard_path)
#endif
{
    if (wildcard_path == nullptr) return;
#ifdef _WIN32
    char directory[0x504] = {};
    char file_path[0x504] = {};
    std::strcpy(directory, wildcard_path);
    const std::size_t length = std::strlen(directory);
    // Every caller passes a path ending in "*.*". 0x4435D7..0x4435DB
    // replaces the '*' three bytes from the end with NUL, retaining '\\'.
    if (length >= 3) directory[length - 3] = '\0';

    LegacyFindData find_data = {};
    void* handle = CRT_FindFirstFile(wildcard_path, &find_data);
    if (handle == reinterpret_cast<void*>(static_cast<intptr_t>(-1))) return;
    do {
        if (find_data.filename[0] != '.') {
            wsprintfA(file_path, "%s%s", directory, find_data.filename);
            SetFileAttributesA(file_path, 0x80); // FILE_ATTRIBUTE_NORMAL
            DeleteFileA(file_path);
        }
        // CRT_FindNextFile at 0x467B50 returns zero while another entry exists.
    } while (CRT_FindNextFile(handle, &find_data) == 0);
    CRT_FindClose(handle);
#else
    std::string native_path(wildcard_path);
    std::replace(native_path.begin(), native_path.end(), '\\', '/');
    if (native_path.size() >= 3 &&
        native_path.compare(native_path.size() - 3, 3, "*.*") == 0) {
        native_path.erase(native_path.size() - 3);
    }
    std::error_code error;
    const std::filesystem::path directory(native_path);
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error)) {
        const std::string filename = iterator->path().filename().string();
        std::error_code type_error;
        if (filename.empty() || filename.front() == '.' ||
            iterator->is_directory(type_error)) {
            continue;
        }
        std::filesystem::remove(iterator->path(), error);
        if (error) error.clear(); // Original ignores attribute/delete failures.
    }
#endif
}

/** DPLAY_SendMessages — clean four transient PostBag directories.
 *  Address: 0x443470 */
void DPLAY_SendMessages()
{
#ifdef _WIN32
    char wildcard_path[0x504] = {};
    const char* const subdirectories[] = {
        "\\Sort\\In", "\\Sort\\Out", "\\Att_Out", "\\Att_In",
    };
    for (const char* subdirectory : subdirectories) {
        wsprintfA(wildcard_path, "%s%s%s\\*.*", g_install_path,
                  "PostBag", subdirectory);
        DPLAY_ReceiveMessage(wildcard_path);
    }
#else
    const std::filesystem::path postbag =
        std::filesystem::path(g_install_path) / "PostBag";
    const std::filesystem::path subdirectories[] = {
        postbag / "Sort" / "In", postbag / "Sort" / "Out",
        postbag / "Att_Out", postbag / "Att_In",
    };
    for (const std::filesystem::path& directory : subdirectories) {
        const std::string wildcard = (directory / "*.*").string();
        DPLAY_ReceiveMessage(wildcard.c_str());
    }
#endif
}
