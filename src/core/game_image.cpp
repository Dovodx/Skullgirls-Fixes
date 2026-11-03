#include "core/game_image.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace sgfixes::game_image {
namespace {

constexpr uintptr_t kPreferredBase = 0x00400000;

// Skullgirls 2nd Encore RETAIL-3.7.14, build R34085.
constexpr const char* kSupportedSha256 =
    "fb757433d2d11602ded18040078e70319afdae98c35fc22d789b38d012ad400b";

constexpr size_t kSha256Size = 32;

// Reads an entire file into memory.
//   path:     full path of the file to read.
//   contents: receives the file's bytes. Its previous contents are replaced.
// Returns true if the whole file was read.
bool ReadWholeFile(const wchar_t* path, std::vector<unsigned char>& contents) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size;
    bool ok = GetFileSizeEx(file, &size) != 0;
    if (ok) {
        contents.resize(static_cast<size_t>(size.QuadPart));
        DWORD bytesRead = 0;
        ok = ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &bytesRead, nullptr)
             && bytesRead == contents.size();
    }
    CloseHandle(file);
    return ok;
}

// Calculates the SHA-256 hash of a block of data using Windows' BCrypt library.
//   data:   the bytes to hash.
//   digest: output. The caller provides this 32-byte array, and BCryptHash fills it with
//           the hash. IsSupportedBuild then compares it with kSupportedSha256.
// Returns true if the hash was calculated. On failure, digest's contents are undefined.
bool ComputeSha256(const std::vector<unsigned char>& data, unsigned char (&digest)[kSha256Size]) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return false;
    }
    NTSTATUS status = BCryptHash(algorithm, nullptr, 0, const_cast<unsigned char*>(data.data()),
                                 static_cast<ULONG>(data.size()), digest, kSha256Size);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return BCRYPT_SUCCESS(status);
}

}  // namespace

bool IsSupportedBuild() {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return false;
    }
    std::vector<unsigned char> contents;
    unsigned char digest[kSha256Size];
    if (!ReadWholeFile(path, contents) || !ComputeSha256(contents, digest)) {
        return false;
    }
    char digestText[kSha256Size * 2 + 1];
    for (size_t i = 0; i < kSha256Size; i++) {
        std::snprintf(&digestText[i * 2], 3, "%02x", digest[i]);
    }
    return std::strcmp(digestText, kSupportedSha256) == 0;
}

uintptr_t LoadedBase() {
    return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
}

uintptr_t Rebase(uintptr_t preferredAddress) {
    return preferredAddress - kPreferredBase + LoadedBase();
}

uintptr_t ToPreferred(uintptr_t runtimeAddress) {
    return runtimeAddress - LoadedBase() + kPreferredBase;
}

}  // namespace sgfixes::game_image
