/**
 * ddraw_spritedata.c — SpriteData sub-object constructor/destructor
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The SpriteData sub-object (14 bytes) is used in TileMap's sprite
 * management (+0x52488 and +0x5248C). It tracks a resource tree and
 * pixel data for a sprite layer:
 *
 * struct SpriteData {
 *     void*    tree_ptr;       // +0x00  resource tree (AssetMgr tree node)
 *     void*    pixel_buf;      // +0x04  pixel data buffer (malloc'd)
 *     uint32_t field_08;       // +0x08  unknown (zeroed by ctor)
 *     uint16_t resource_id;    // +0x0C  resource ID
 * };
 *
 * DDRAW_SpriteDataCtor is a __thiscall C++ constructor.
 * DDRAW_SpriteDataDtor is a __fastcall C++ destructor.
 */

#include <stdint.h>

/* ================================================================== */
/* External functions                                                  */
/* ================================================================== */

extern void  __cdecl CRT_free(void* ptr);                 /* 0x466C70 */
extern void  __cdecl AssetMgr_ReadFile(void* tree_ptr);   /* 0x45D8C0 (MISNAMED: frees tree) */

/* ================================================================== */
/* SpriteData struct (14 bytes)                                         */
/* ================================================================== */
typedef struct {
    void*    tree_ptr;       /* +0x00  resource tree node */
    void*    pixel_buf;      /* +0x04  pixel data buffer */
    uint32_t field_08;       /* +0x08  flags/unknown */
    uint16_t resource_id;    /* +0x0C  resource ID */
} SpriteData;

/* ================================================================== */
/* DDRAW_SpriteDataCtor — Constructor for SpriteData                   */
/* Address: 0x45CDF0                                                   */
/* Size: 24 bytes (8 insn)                                             */
/* Calling convention: __thiscall (_this in ECX, 1 stack param)        */
/*                                                                     */
/* Zeroes the first three 32-bit fields (+0x00, +0x04, +0x08) and     */
/* stores the resource_id as uint16 at +0x0C.                          */
/*                                                                     */
/* @param resource_id  Resource identifier for _this sprite layer       */
/* ================================================================== */
void __thiscall DDRAW_SpriteDataCtor(SpriteData* _this, uint16_t resource_id);
void __thiscall DDRAW_SpriteDataCtor(SpriteData* _this, uint16_t resource_id)
{
    _this->tree_ptr     = NULL;   /* +0x00 */
    _this->pixel_buf    = NULL;   /* +0x04 */
    _this->field_08     = 0;      /* +0x08 */
    _this->resource_id  = resource_id;  /* +0x0C */
}

/* ================================================================== */
/* DDRAW_SpriteDataDtor — Destructor for SpriteData                    */
/* Address: 0x45CE10                                                   */
/* Size: 33 bytes (12 insn)                                            */
/* Calling convention: __fastcall (param_1 in ECX = SpriteData*)      */
/*                                                                     */
/* Frees the resource tree (via AssetMgr_ReadFile, MISNAMED), then     */
/* frees the pixel buffer (if non-NULL). Does NOT free the struct.     */
/*                                                                     */
/* @param data  SpriteData to clean up                                  */
/* ================================================================== */
void __fastcall DDRAW_SpriteDataDtor(SpriteData* data);
void __fastcall DDRAW_SpriteDataDtor(SpriteData* data)
{
    /* Free resource tree (MISNAMED: AssetMgr_ReadFile at 0x45D8C0
     * actually frees/decrements ref on a tree node) */
    AssetMgr_ReadFile(data->tree_ptr);

    /* Free pixel buffer */
    if (data->pixel_buf != NULL) {
        CRT_free(data->pixel_buf);
        data->pixel_buf = NULL;
    }
}
