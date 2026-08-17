/**
 * stubs_link001_batch6_asset_huf.cpp — LINK-001 batch 6: Huffman-codec
 * call-0 landmine.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Fixes two call-0 landmines (main binary links with
 * -Wl,--unresolved-symbols=ignore-all, so any undefined reference silently
 * binds to address 0 instead of failing the link):
 *
 *   1. Huf_GetUncompressedSize(uint32_t*)   — real implementation, 0x45C820
 *   2. Huf_Decode(int32_t*, uint8_t*, int32_t*) — real implementation, 0x45C830
 *
 * (A third item, an AssetMgr_LoadFile(AssetMgr*, ...) thunk, previously
 * lived here — see the g_asset_mgr extern-global-type-mismatch cleanup,
 * dated 2026-08-17: the receiver is now a real, correctly-identified
 * `AssetArchive` value type (resources/AssetArchive.h/.cpp), every call
 * site uses `g_asset_mgr.LoadFile(...)` directly, and this thunk — along
 * with the wrong-signature no-op stubs it was bridging around in
 * shared/core_stubs.cpp/link_stubs.cpp — has been removed as orphaned.)
 *
 * === Huf_GetUncompressedSize / Huf_Decode ===
 *
 * Both are genuine internal game logic (a small custom Huffman decoder used
 * to decompress asset-tree entries), not OS/hardware wrappers, and are
 * called from resources/AssetArchive.cpp's AssetArchive::LoadFile
 * (0x45CD00) — that file only forward-declares them as `extern`, it does
 * not define them, which is exactly what left them unresolved. Ghidra
 * decompiles of both are small and unambiguous, so they are implemented
 * here for real rather than stubbed, transcribed instruction-for-instruction
 * against the decompiler output (verified against the disassembly's
 * bit/shift/mask widths):
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
