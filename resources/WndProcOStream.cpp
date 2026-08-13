/**
 * WndProcOStream.cpp — WNDPROC_OStream implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation + disassembly.
 *
 * See WndProcOStream.h for the class/address map and the vbtable/size
 * evidence that this is a genuinely distinct class from WNDPROC_Stream.
 * This file transcribes:
 *   AttachBuffer   0x465A30
 */

// Status: VALIDATED

#include "WndProcOStream.h"

/* ================================================================== */
/* AttachBuffer — 0x465A30 (previously mislabeled "CRT_except_handler") */
/* ================================================================== */
void WNDPROC_OStream::AttachBuffer(WNDPROC_StreamBuf* newBuf)
{
    StreamObject::AttachBuffer(newBuf);
    _reserved_04 = 0;
}
