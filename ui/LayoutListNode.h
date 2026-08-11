/**
 * LayoutListNode.h — Linked list node for scenario/layout entries
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Each node is 0x0C bytes on the original x86 layout: next pointer,
 * padding, and a heap-allocated name string. sizeof(LayoutListNode) is
 * 0x18 on a 64-bit host (next/name pointers widen from 4 to 8 bytes) —
 * callers must allocate sizeof(LayoutListNode), not the original x86
 * literal. Used by GameSetupPanel for titleList (+0xEC) and
 * layoutList (+0xF0) linked lists.
 */

// Status: TRANSCRIBED

#pragma once

#include <cstdint>

struct LayoutListNode {
    LayoutListNode* next;    // +0x00  next node pointer
    int32_t         _pad_04; // +0x04  padding/unused
    char*           name;    // +0x08  heap-allocated name string
};
