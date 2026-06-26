/*
 * Lego Loco (1998) - Native Linux Port
 * src/resources/huffman.c — Huffman decompressor for packed RFD assets
 *
 * Direct port of FUN_0045c830 (0x0045c830) from loco.exe.
 *
 * Verified against all 173 .but and 2 .ani files in resource.RFD.
 * All decompress correctly to valid Windows BMP / RIFF ACON data.
 */

#include "huffman.h"
#include <string.h>

uint32_t Huffman_GetUncompressedSize(const uint8_t *packed_data) {
    uint32_t size;
    memcpy(&size, packed_data, 4);
    return size;
}

/*
 * Huffman_Decompress
 *
 * Mirrors FUN_0045c830 verbatim. Variable names match the Ghidra decompile:
 *   param_1 = packed_data (int* in original = uint32_t* here)
 *   param_2 = out_buf
 *   local_c = remaining output bytes counter
 *   iVar1   = tree_root
 *   uVar3   = current 32-bit bit-stream word
 *   puVar7  = pointer into bit-stream (advancing 32 bits at a time)
 *   iVar6   = current Huffman tree node during traversal
 *   iVar4   = bits remaining in current word (countdown from 32)
 *
 * Table indexing (mirrors assembly `iVar6 = CONCAT22((uint)iVar6>>16, table[iVar6+8])`):
 *   node_index * 2 gives byte offset into table; + 8 skips the 8-byte header.
 *   Each tree entry is a uint16 at that byte offset.
 */
uint32_t Huffman_Decompress(const uint8_t *packed_data, uint8_t *out_buf) {
    const uint32_t *p32 = (const uint32_t *)packed_data;

    uint32_t  local_c = p32[0];     /* uncompressed_size = *param_1 */
    uint32_t  iVar1   = p32[1];     /* tree_root = param_1[1] */
    uint32_t  iVar4   = 32;         /* bits remaining in current word */
    uint32_t  uVar3   = p32[0x202]; /* bit stream start = param_1[0x202] */
    const uint32_t *puVar7 = p32 + 0x203; /* next word ptr = param_1 + 0x203 */

    uint8_t *out_ptr = out_buf - 1; /* puVar5 pre-decrement loop */

    const uint16_t *table = (const uint16_t *)(packed_data + 8); /* skip 8-byte header */

    uint32_t count = local_c;
    while (count--) {
        out_ptr++;

        uint32_t iVar6 = iVar1; /* start at tree root */

        while (iVar6 > 0xFF) {
            /* Read one bit (LSB-first) from the 32-bit stream */
            uint32_t uVar2 = uVar3 & 1;
            uVar3 = (int32_t)uVar3 >> 1; /* arithmetic right shift (preserves sign in original) */
            iVar4--;

            /* Navigate the Huffman tree:
             * iVar6 = (iVar6 * 2 + bit) * 2  (byte index into table, relative to header+8)
             * then read the uint16 at that offset as the new node.
             * Original assembly: iVar6 = CONCAT22((short)(iVar6>>16), *(uint16*)(param_1 + iVar6 + 8))
             * Since iVar6 starts small and table is uint16[], we use direct uint16 indexing. */
            uint32_t idx = (iVar6 * 2 + uVar2) * 2; /* byte offset */
            /* Read the uint16 at (packed_data + idx + 8) — same as table[idx/2] since table = packed_data+8 */
            const uint8_t *t = (const uint8_t *)packed_data + idx + 8;
            uint16_t next_node;
            memcpy(&next_node, t, 2);
            iVar6 = next_node;

            if (iVar4 == 0) {
                iVar4 = 32;
                uVar3 = *puVar7++;
            }
        }

        *out_ptr = (uint8_t)iVar6;
    }

    return local_c;
}
