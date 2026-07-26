/**
 * math_huf_helpers.c — Math utility functions and Huffman decompression
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Contains small helper functions for distance checking, collision, and
 * Huffman-compressed resource decoding used by the game's resource manager.
 *
 * Functions:
 *   Math_DistSquared       (0x45C7A0) — Squared Euclidean distance
 *   Math_PointOnLineSegment (0x45C7C0) — Point-on-line-segment test
 *   Huf_GetUncompressedSize (0x45C820) — Read uncompressed size from Huffman header
 *   Huf_Decode              (0x45C830) — Custom Huffman decompressor
 *
 * Calling conventions: all __cdecl (C functions)
 */

#include "../shared/types.h"

/* ================================================================== */
/* Math_DistSquared                                                     */
/* Address: 0x45C7A0                                                    */
/*                                                                      */
/* Called by: Vehicle_CalcSpeed (0x44D6C0),                               */
/*            VehicleEditor_ProcessMove (0x40D940), etc.                */
/*                                                                      */
/* Simple quadratic distance metric. Avoids sqrt() for comparison use.  */
/*                                                                      */
/* @return (x1-x2)^2 + (y1-y2)^2                                       */
/* ================================================================== */
int __cdecl Math_DistSquared(int x1, int y1, int x2, int y2)
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    return dx * dx + dy * dy;
}


/* ================================================================== */
/* Math_PointOnLineSegment                                              */
/* Address: 0x45C7C0                                                    */
/*                                                                      */
/* Checks if point (px, py) lies on the line segment (x1, y1)-(x2, y2) */
/* using the cross-product = 0 collinearity test.                       */
/*                                                                      */
/* The line test: (y2 - y1) * px - (x2 - x1) * py + x2*y1 - y2*x1 == 0 */
/* But the actual implementation is approximate (>> 8 shift).           */
/*                                                                      */
/* @param x1,y1  Segment start                                          */
/* @param x2,y2  Segment end                                            */
/* @param px,py  Point to test                                          */
/* @return 1 if point lies on segment, 0 otherwise                      */
/* ================================================================== */
int __cdecl Math_PointOnLineSegment(int x1, int y1, uint x2, int y2, uint px, int py)
{
    /* Clamp px to [min(x1,x2), max(x1,x2)] — bounding box check */
    if ((int)px >= (int)x2 && (int)px <= (int)x1 ||
        (int)px >= (int)x1 && (int)px <= (int)x2)
    {
        /* Cross-product collinearity test (approximate) */
        int cross = (y2 - y1) * py;
        return (cross - ((x2 - x1) * (int)px + x2 * y1 - y2 * x1)) == 0;
    }
    return 0;  /* outside bounding box */
}


/* ================================================================== */
/* Huf_GetUncompressedSize — Read Huffman block header                  */
/* Address: 0x45C820                                                    */
/*                                                                      */
/* The first 4 bytes of a Huffman-compressed block store the            */
/* uncompressed size of the original data. This simple accessor reads   */
/* and returns that value.                                              */
/*                                                                      */
/* @param hdr  Pointer to compressed data block                         */
/* @return Uncompressed size in bytes                                   */
/* ================================================================== */
unsigned int __cdecl Huf_GetUncompressedSize(unsigned int* hdr)
{
    return *hdr;
}


/* ================================================================== */
/* Huf_Decode — Custom Huffman decompressor                             */
/* Address: 0x45C830                                                    */
/*                                                                      */
/* Custom Huffman decoding for compressed game resources. Block format: */
/*   offset[0]:      uncompressed_size (uint32)                         */
/*   offset[4]:      initial_tree_node (uint16?)                        */
/*   offset[8..0x20F]: 512-entry 16-bit tree (entry/2 = 256 nodes)     */
/*   offset[0x20C+]:   bit-packed input stream (uint32 LE words)       */
/*                                                                      */
/* Tree encoding:                                                       */
/*   value 0x00-0xFF = leaf node (output byte)                          */
/*   value > 0xFF = internal node (index*2, used for navigation)        */
/*                                                                      */
/* Navigation: read bits from input stream. Each bit chooses left (0)   */
/* or right (1) branch: new_idx = (idx*2 + bit) * 2, then look up       */
/* tree[new_idx]. Repeat until leaf (value <= 0xFF), output that byte.  */
/*                                                                      */
/* @param header     Pointer to Huffman-compressed data                 */
/* @param out_buffer Output buffer (must be at least uncompressed_size) */
/* @param out_size   Output: actual decompressed size written           */
/* ================================================================== */
void __cdecl Huf_Decode(int* header, int out_buffer, int* out_size)
{
    unsigned int remaining;        /* bytes remaining to decompress */
    unsigned int bit_buf;          /* current 32-bit bit buffer */
    int bits_left;                 /* bits remaining in bit_buf */
    unsigned int* input_ptr;       /* pointer to next input dword */
    unsigned char* out_ptr;        /* current output position */
    short* tree;                   /* Huffman tree (256 nodes, 2 entries each) */

    remaining = header[0];         /* uncompressed_size */
    int initial_node = header[1];  /* root node index */
    bits_left = 32;                /* start with empty buffer */
    bit_buf = header[0x202];       /* first 32 bits of compressed data */
    input_ptr = (unsigned int*)(header + 0x203);  /* stream continues */
    out_ptr = (unsigned char*)(out_buffer - 1);

    do {
        out_ptr++;
        int node = initial_node;

        /* Traverse tree until leaf */
        while (node > 0xFF) {
            unsigned int bit = bit_buf & 1;
            bit_buf >>= 1;
            bits_left--;

            /* Navigate: left(0) or right(1) */
            node = (node * 2 + (bit != 0 ? 1 : 0)) * 2;
            /* Look up tree entry (2-byte values at header+8+node) */
            node = *(short*)((int)header + node + 8);

            if (bits_left == 0) {
                bits_left = 32;
                bit_buf = *input_ptr;
                input_ptr++;
            }
        }

        remaining--;
        *out_ptr = (unsigned char)node;
    } while (remaining != 0);

    /* Store output size */
    *out_size = header[0];
}
