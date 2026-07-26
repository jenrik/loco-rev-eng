/**
 * huf_decode.c — Huffman decompression helpers
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompression.
 *
 * These functions decompress a custom Huffman-compressed format found
 * in Lego Loco's resource files (.RES/.PKB). The compressed block has:
 *
 *   [0x00] uint32_t  uncompressed_size
 *   [0x04] uint32_t  initial_tree_index
 *   [0x08] uint16_t  tree[1024]   (0x800 bytes, packed 16-bit entries)
 *   [0x808] uint32_t  bitstream... (rest of data)
 *
 * The tree is a binary tree where each node is an int16:
 *   value < 256   → leaf (output byte)
 *   value >= 256  → internal node, index = value * 2
 *
 * Implementation is __cdecl with no object context.
 */

#include <stdint.h>

/* ================================================================== */
/* Huf_GetUncompressedSize                                             */
/* Address: 0x45C820                                                   */
/* Size: 7 bytes (3 insn)                                              */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Reads the first uint32 of the compressed data block, which is the   */
/* uncompressed output size.                                           */
/*                                                                     */
/* Called by: AssetMgr_LoadFile (0x45CDA2)                             */
/*                                                                     */
/* @param compressed_data  Pointer to Huffman-compressed data block    */
/* @return                 Uncompressed size in bytes                  */
/* ================================================================== */
uint32_t __cdecl Huf_GetUncompressedSize(uint32_t* compressed_data)
{
    return compressed_data[0];
}

/* ================================================================== */
/* Huf_Decode                                                          */
/* Address: 0x45C830                                                   */
/* Size: 105 bytes (46 insn)                                           */
/* Calling convention: __cdecl                                         */
/*                                                                     */
/* Decompresses a Huffman-compressed data block using a 16-bit tree.   */
/*                                                                     */
/* Tree traversal: start at initial_tree_index. For each bit consumed  */
/* from the bitstream, if bit = 0, index = index*2; if bit = 1,        */
/* index = index*2 + 2. The tree is stored as uint16 array starting    */
/* at compressed_data + 2 (offset 8 bytes), indexed by the computed    */
/* node position. When value < 256, it's a leaf byte.                  */
/*                                                                     */
/* Called by: AssetMgr_LoadFile (0x45CDD2)                             */
/*                                                                     */
/* @param src      Compressed data buffer (size + index + tree + bits) */
/* @param dst      Output buffer for decompressed data                  */
/* @param out_size Receives the decompressed byte count on return       */
/* ================================================================== */
void __cdecl Huf_Decode(
    int32_t* src,
    uint8_t* dst,
    int32_t* out_size)
{
    int32_t remaining;     /* number of bytes remaining to decode */
    int32_t node_index;    /* current tree traversal index */
    int32_t bits_remaining;/* bits remaining in current dword */
    uint32_t bit_buf;      /* current bitstream dword */
    uint32_t* bitstream_ptr; /* pointer into bitstream */

    /* Read header: count + initial index */
    remaining = src[0];
    node_index = src[1];

    /* Bitstream starts after tree (src + 2 + 0x800/4 uint32s = src + 0x202) */
    bitstream_ptr = (uint32_t*)(src + 0x202);
    bits_remaining = 32;
    bit_buf = *bitstream_ptr++;

    dst--;  /* pre-decrement for do-while loop increment */

    do {
        dst++;

        /* Walk the tree using bits from the bitstream */
        while (node_index >= 256) {
            /* Shift one bit out of the bit buffer */
            uint32_t bit = bit_buf & 1;
            bit_buf >>= 1;
            bits_remaining--;

            /* Node: index = (index * 2 + bit) * 2
             * The tree is an array of uint16 at src + 2 (offset 8 bytes) */
            node_index = (node_index * 2 + (int32_t)bit) * 2;
            node_index = *(int16_t*)((uint8_t*)src + 8 + node_index);

            /* Refill bit buffer when empty */
            if (bits_remaining == 0) {
                bits_remaining = 32;
                bit_buf = *bitstream_ptr++;
            }
        }

        remaining--;
        *dst = (uint8_t)node_index;
    } while (remaining != 0);

    *out_size = src[0];
}
