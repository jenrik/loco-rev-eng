/**
 * LayoutListNode.h — Linked list node for scenario/layout entries
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Each node is 0x0C bytes: next pointer, padding, and a heap-allocated
 * name string. Used by GameSetupPanel for titleList (+0xEC) and
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
