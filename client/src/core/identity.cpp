#include "identity.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <vector>

namespace ugworld {
bool matches_target(const std::wstring& path) noexcept {
    HANDLE file = INVALID_HANDLE_VALUE;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<UCHAR> object;
    bool matches = false;
    try {
        // Deny writes/deletion during hashing; this authenticates the disk file only.
        file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        LARGE_INTEGER size{};
        if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &size) || size.QuadPart != 4800512)
            throw 0;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) throw 0;
        DWORD object_size = 0, received = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &received, 0) < 0) throw 0;
        object.resize(object_size);
        if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) throw 0;
        std::array<UCHAR, 65536> buffer{};
        DWORD count = 0;
        unsigned long long total = 0;
        bool ok = true;
        for (;;) {
            if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) { ok = false; break; }
            if (!count) break;
            total += count;
            if (BCryptHashData(hash, buffer.data(), count, 0) < 0) { ok = false; break; }
        }
        std::array<UCHAR, 32> digest{};
        constexpr std::array<UCHAR, 32> expected = {
            0xf9,0xdd,0x86,0xc0,0x54,0x87,0x8c,0xe6,0x27,0x6b,0xeb,0x07,0xc1,0xfd,0x61,0x87,
            0x4f,0x7a,0x1e,0x4b,0xf1,0xf2,0x41,0xb0,0x84,0xc6,0x5b,0x73,0xe2,0x41,0x68,0xa7};
        matches = ok && total == 4800512 && BCryptFinishHash(hash, digest.data(), 32, 0) >= 0 && digest == expected;
        // Destroy hash while its backing object is still alive.
        BCryptDestroyHash(hash);
        hash = nullptr;
    } catch (...) { matches = false; }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return matches;
}
}
