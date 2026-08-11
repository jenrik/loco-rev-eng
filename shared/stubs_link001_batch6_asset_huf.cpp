/**
 * stubs_link001_batch6_asset_huf.cpp — LINK-001 batch 6: asset-loader /
 * Huffman-codec call-0 landmine cluster.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Fixes three call-0 landmines (main binary links with
 * -Wl,--unresolved-symbols=ignore-all, so any undefined reference silently
 * binds to address 0 instead of failing the link):
 *
 *   1. Huf_GetUncompressedSize(uint32_t*)   — real implementation, 0x45C820
 *   2. Huf_Decode(int32_t*, uint8_t*, int32_t*) — real implementation, 0x45C830
 *   3. AssetMgr_LoadFile(AssetMgr*, unsigned char*, int*) — thunk, see below
 *
 * === Huf_GetUncompressedSize / Huf_Decode ===
 *
 * Both are genuine internal game logic (a small custom Huffman decoder used
 * to decompress asset-tree entries), not OS/hardware wrappers, and are
 * called from native/assetmgr_loadfile.c's AssetMgr_LoadFile (0x45CD00) —
 * that file only forward-declares them as `extern`, it does not define them,
 * which is exactly what left them unresolved. Ghidra decompiles of both are
 * small and unambiguous, so they are implemented here for real rather than
 * stubbed, transcribed instruction-for-instruction against the decompiler
 * output (verified against the disassembly's bit/shift/mask widths):
 *
 *   - Huf_GetUncompressedSize (0x45C820): the compressed block's header
 *     stores the uncompressed size as its first native (little-endian)
 *     uint32_t; the function just reads it.
 *
 *   - Huf_Decode (0x45C830): header[0] = uncompressed byte count, header[1]
 *     = starting/root tree-node value, header[2..0x201] = a 16-bit-entry
 *     binary tree (leaf values 0-0xFF, internal-node values >0xFF encode
 *     2x the child node index), header[0x202+] = the bitstream, consumed
 *     32 bits (one uint32_t) at a time, LSB first. For each output byte,
 *     walk the tree from the root, taking the next bitstream bit to select
 *     left/right child, until a leaf (<=0xFF) is reached; emit that byte.
 *     Refill the 32-bit bit buffer from the bitstream every 32 bits
 *     consumed. Writes the total decompressed size to *out_size at the end
 *     (mirrors *param_3 = *param_1 in the decompile — the original header
 *     size field, unchanged by the decode loop).
 *
 * === AssetMgr_LoadFile(AssetMgr*, unsigned char*, int*) ===
 *
 * network/Netman.cpp's Netman::LoadScenario (0x43D820) and
 * ui/GameSetupPanel.cpp's GameSetupPanel::loadLayouts (0x409E70) both call
 * AssetMgr_LoadFile through network/Netman.h:353's declaration:
 *     uint8_t* AssetMgr_LoadFile(AssetMgr* self, uint8_t* filename, int32_t* out_size);
 * That first-param type (AssetMgr*) differs from the *real* implementation's
 * own signature in native/assetmgr_loadfile.c:
 *     uint8_t* __thiscall AssetMgr_LoadFile(void* _this, uint8_t* filename, int32_t* out_size);
 * In C++ these mangle to different symbols
 * (_Z16AssetMgr_LoadFileP8AssetMgrPhPi vs. _Z16AssetMgr_LoadFilePvPhPi), so
 * the AssetMgr*-typed call sites never bind to the real, already-decompiled
 * 0x45CD00 body — hence the call-0. Per assetmgr_loadfile.c's own header
 * comment, the object actually walked by that function (this+0x00 = open
 * CRT file handle, this+0x04 = linked-list-of-directory-entries head) is
 * NOT the resources/AssetMgr.h `AssetMgr` struct (whose +0x00/+0x04 are
 * entry_count/pair_matrix) — so Netman.h:353's `AssetMgr*` parameter type
 * is itself the wrong declaration for this call (this cluster already has
 * 3 mutually-incompatible first-param types across the tree, tracked at
 * docs/landmine-sweep-worklist.md line 251). The actual global passed at
 * both call sites, g_asset_mgr, is declared `void*` everywhere except in
 * Netman.h/HelpWnd.cpp — matching the real implementation, not the
 * caller-side declaration used here.
 *
 * Per this session's constraints, the caller declaration in Netman.h is not
 * editable from this file. This is therefore a thunk, not a fix at the
 * root: it reproduces the exact (AssetMgr*, uint8_t*, int32_t*) mangled
 * signature the call sites need, and forwards to the real void*-typed
 * implementation via a same-representation pointer reinterpretation (both
 * are plain data pointers to the same runtime object; only the static C++
 * type differs). This makes the call sites resolve to the real,
 * already-decompiled logic instead of silently binding to address 0, without
 * picking a side in the still-open, separately-tracked multi-signature
 * cluster. See SHOULD_BE_FIXED_AT below for the actual root-cause location.
 */

#include <cstdint>

/* ================================================================== */
/* Huf_GetUncompressedSize — Address: 0x45C820                         */
/* Reads the uncompressed-size field from a Huffman-compressed block's */
/* header (first uint32_t).                                            */
/* ================================================================== */
uint32_t Huf_GetUncompressedSize(uint32_t* data)
{
    return *data;
}

/* ================================================================== */
/* Huf_Decode — Address: 0x45C830                                      */
/* Decodes a Huffman-compressed block into dst. See file header for     */
/* the header/tree/bitstream layout. Transcribed instruction-for-       */
/* instruction from the Ghidra decompile (verified bit widths/masks):   */
/*                                                                       */
/*   local_c = *param_1;              // remaining output byte count    */
/*   iVar1   = param_1[1];            // tree root node value           */
/*   iVar4   = 0x20;                  // bits remaining in bit buffer   */
/*   uVar3   = param_1[0x202];        // initial 32-bit bit buffer      */
/*   puVar7  = (uint*)(param_1+0x203);// next bitstream dword           */
/*   puVar5  = dst - 1;                                                 */
/*   do {                                                                */
/*     puVar5++;                                                        */
/*     iVar6 = iVar1;                                                   */
/*     while (0xff < iVar6) {                                           */
/*       uVar2 = uVar3 & 1; uVar3 = (int)uVar3 >> 1;                    */
/*       iVar6 = (iVar6*2 + (uVar2!=0)) * 2;                            */
/*       iVar4 -= 1;                                                    */
/*       iVar6 = CONCAT22(hi16(iVar6), *(uint16*)((byte*)param_1+iVar6+8));*/
/*       if (iVar4 == 0) { iVar4 = 0x20; uVar3 = *puVar7++; }           */
/*     }                                                                */
/*     local_c -= 1; *puVar5 = (uint8_t)iVar6;                          */
/*   } while (local_c != 0);                                            */
/*   *param_3 = *param_1;                                               */
/* ================================================================== */
void Huf_Decode(int32_t* src, uint8_t* dst, int32_t* out_size)
{
    int32_t remaining = src[0];
    int32_t node = src[1];
    int32_t bitsLeft = 0x20;

    const uint32_t* bitstream32 = reinterpret_cast<const uint32_t*>(src);
    uint32_t bitBuf = bitstream32[0x202];
    const uint32_t* nextWord = bitstream32 + 0x203;

    uint8_t* out = dst - 1;
    const uint8_t* treeBase = reinterpret_cast<const uint8_t*>(src) + 8;

    do {
        out = out + 1;
        int32_t cur = node;
        while (0xff < cur) {
            uint32_t bit = bitBuf & 1u;
            bitBuf = static_cast<uint32_t>(static_cast<int32_t>(bitBuf) >> 1);
            cur = (cur * 2 + static_cast<int32_t>(bit != 0)) * 2;
            bitsLeft = bitsLeft - 1;

            uint16_t treeVal = *reinterpret_cast<const uint16_t*>(treeBase + cur);
            cur = static_cast<int32_t>((static_cast<uint32_t>(cur) & 0xFFFF0000u) |
                                        static_cast<uint32_t>(treeVal));

            if (bitsLeft == 0) {
                bitsLeft = 0x20;
                bitBuf = *nextWord;
                nextWord = nextWord + 1;
            }
        }
        remaining = remaining - 1;
        *out = static_cast<uint8_t>(cur);
    } while (remaining != 0);

    *out_size = src[0];
}

/* ================================================================== */
/* AssetMgr_LoadFile(AssetMgr*, unsigned char*, int*) — call-site thunk */
/*                                                                       */
/* Forwards to the real, already-decompiled implementation in           */
/* native/assetmgr_loadfile.c (0x45CD00), whose first parameter is       */
/* `void*` (see rationale in the file header above). `AssetMgr` is only  */
/* forward-declared — never defined here — because this thunk never      */
/* touches the object's fields, only passes the pointer through.        */
/* ================================================================== */
class AssetMgr;

extern uint8_t* AssetMgr_LoadFile(void* _this, uint8_t* filename, int32_t* out_size);

uint8_t* AssetMgr_LoadFile(AssetMgr* self, uint8_t* filename, int32_t* out_size)
{
    return AssetMgr_LoadFile(reinterpret_cast<void*>(self), filename, out_size);
}
