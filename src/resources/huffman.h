/*
 * Lego Loco (1998) - Native Linux Port
 * src/resources/huffman.h — Huffman decompressor for packed RFD assets
 *
 * Reverse-engineered from FUN_0045c830 at 0x0045c830 in loco.exe.
 *
 * All RFH entries with flags == 0x01 are Huffman-compressed:
 *   - 173 .but files (toolbar button sprites: 156×53 px, 8bpp BMP)
 *   - 2 of 3 .ani cursor files (RIFF ACON animated cursors)
 *
 * WIN32: FUN_0045c830 (called by FUN_0045cd00 when flags & 1)
 * LINUX: this implementation is a direct port of that function
 */

#ifndef LOCO_HUFFMAN_H
#define LOCO_HUFFMAN_H

#include <stdint.h>
#include <stddef.h>

#define HUFFMAN_HEADER_SIZE   8     /* uncompressed_size + tree_root */
#define HUFFMAN_TABLE_BYTES   2040  /* uint16 table: 1020 entries */
#define HUFFMAN_STREAM_OFFSET 0x808 /* byte offset where bit stream starts */

/*
 * Packed format header (first 8 bytes of any flags=0x01 asset):
 *   [0] uint32_le  uncompressed_size
 *   [4] uint32_le  tree_root
 */
typedef struct HuffmanHeader {
    uint32_t uncompressed_size;
    uint32_t tree_root;
} HuffmanHeader;

/*
 * Huffman_GetUncompressedSize
 * Returns the output size in bytes. Mirrors FUN_0045c820.
 *
 * WIN32: FUN_0045c820 — returns *param_1 (first uint32 of packed data)
 * LINUX: identical
 */
uint32_t Huffman_GetUncompressedSize(const uint8_t *packed_data);

/*
 * Huffman_Decompress
 * Decompresses packed_data into out_buf. Returns number of bytes written.
 * out_buf must be at least Huffman_GetUncompressedSize(packed_data) bytes.
 *
 * Mirrors FUN_0045c830. Algorithm:
 *   1. node = tree_root
 *   2. While node > 0xFF: read one bit (LSB-first); node = table[(node*2 + bit)*2]
 *   3. Output byte = node & 0xFF
 *   4. Repeat uncompressed_size times
 *
 * WIN32: FUN_0045c830(packed_data_ptr, out_buf, &out_size)
 * LINUX: same
 */
uint32_t Huffman_Decompress(const uint8_t *packed_data, uint8_t *out_buf);

#endif /* LOCO_HUFFMAN_H */
