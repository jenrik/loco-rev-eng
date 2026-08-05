/**
 * WndProcStream.h — vtable-based stream object for file I/O
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Used by GameSetupPanel::loadLayouts() to read and validate stream data
 * from disk. The stream object wraps a memory-mapped or file-based data
 * buffer and provides a validity check via IsValid().
 */

// Status: TRANSCRIBED
// TODO: Full decompilation — verify layout in Ghidra and add remaining
//       vtable methods (slots [1]-[4]).

#pragma once

#include <cstdint>

/**
 * WndProcStreamMetadata — header/metadata block for WNDPROC stream objects.
 * The first field of a WndProcStream points to an instance of this struct.
 * Index 4 (offset 0x10) stores a byte offset used to locate the stream's
 * validity flags field within the stream object.
 */
struct WndProcStreamMetadata {
    int32_t field_00;        // +0x00
    int32_t field_04;        // +0x04
    int32_t field_08;        // +0x08
    int32_t field_0C;        // +0x0C
    int32_t flagsOffset;     // +0x10  byte offset to flags within WndProcStream
};

/**
 * WndProcStream — vtable-based stream object used for file I/O.
 * Vtable: [0] scalar deleting destructor, [4] returns flags offset info.
 */
class WndProcStream {
public:
    virtual ~WndProcStream() {}      // [0] scalar deleting destructor

    WndProcStreamMetadata* metadata; // +0x00  pointer to metadata block
    int32_t  field_04;               // +0x04
    int32_t  streamLength;           // +0x08  total stream length in bytes

    /** Check if the stream is valid (flags & 5 == 0). */
    bool IsValid() const {
        int32_t offset = this->metadata->flagsOffset;
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(this);
        const int32_t* flagsPtr = reinterpret_cast<const int32_t*>(bytes + offset + 8);
        int32_t flags = *flagsPtr;
        return (flags & 5) == 0;
    }
};
