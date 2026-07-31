#pragma once

#include <cstdint>

/** TrainMessage — queue node used by TrainSubsystem and Netman.
 *  Original x86 size: 0x1C bytes. */
struct TrainMessage {
    int32_t type;
    union { int32_t data_len; int32_t size; };
    union { void* data_ptr; void* data; };
    union { int32_t target_dpId; int32_t to_player; };
    int32_t flags;
    int32_t _pad_14;
    void* next;
#ifndef _WIN32
    // Pointer widening moves flags to +0x14 on the host. Preserve the
    // original packed metadata in distinct native fields.
    uint8_t host_metadata_0;
    uint8_t host_metadata_1;
#endif

    TrainMessage()
        : type(0), data_len(0), data_ptr(nullptr), target_dpId(0), flags(0),
          _pad_14(0), next(nullptr)
#ifndef _WIN32
          , host_metadata_0(0), host_metadata_1(0)
#endif
    {}

    uint8_t metadata0() const {
#ifdef _WIN32
        return reinterpret_cast<const uint8_t*>(this)[0x14];
#else
        return host_metadata_0;
#endif
    }
    uint8_t metadata1() const {
#ifdef _WIN32
        return reinterpret_cast<const uint8_t*>(this)[0x15];
#else
        return host_metadata_1;
#endif
    }
    uint16_t metadata16() const {
        return static_cast<uint16_t>(metadata0()) |
               static_cast<uint16_t>(metadata1() << 8);
    }
    void setMetadata0(uint8_t value) {
#ifdef _WIN32
        reinterpret_cast<uint8_t*>(this)[0x14] = value;
#else
        host_metadata_0 = value;
#endif
    }
    void setMetadata1(uint8_t value) {
#ifdef _WIN32
        reinterpret_cast<uint8_t*>(this)[0x15] = value;
#else
        host_metadata_1 = value;
#endif
    }
};
