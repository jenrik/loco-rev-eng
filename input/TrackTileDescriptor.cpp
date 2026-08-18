/**
 * TrackTileDescriptor.cpp — see TrackTileDescriptor.h for the full
 * class-level evidence trail and naming rationale.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: VALIDATED
 */

#include "TrackTileDescriptor.h"
#include "../resources/AssetArchive.h"
#include "../resources/Win32Stream.h"
#include "../resources/Win32StreamMem.h"
#include "../resources/StreamObject.h"
#include "../resources/WndProcStream.h"

#include <cstdio>
#include <cstring>
#include <new>

/* GLOBAL_free has plain C++ linkage in this tree (see game/Panel.h,
 * input/BuildingDescriptorEditor.cpp). */
extern void GLOBAL_free(void* ptr);                     /* 0x465CD0 */

/* g_scene_name — already-established global from game/ScriptedObject.cpp
 * (this file's HandleEvent logic used to live there, misattributed; the
 * global moves with it). g_asset_mgr is the real AssetArchive value object
 * (resources/AssetArchive.h, included below) — was previously declared
 * `void*` here and passed BY VALUE (not by address) to AssetMgr_LoadFile,
 * which only happened to "work" because the old global storage really was
 * a `void*`; now that it's the real value object, call sites use
 * g_asset_mgr.LoadFile(...) directly. */
extern char  g_scene_name[];                            /* 0x4A99C8 */
extern int   g_stream_open_flags;                       /* 0x479190 */

extern "C" {
    void CRT_free(void* ptr);                                          /* 0x466C70 */
}
int __cdecl CRT_sprintf_buf(void* buf, const char* fmt, ...);          /* 0x466D60 */

/* WNDPROC_CriticalSectionLock/StreamReadLine — same established
 * declarations as input/BuildingDescriptorEditor.cpp (see that file's doc
 * comment for the real-linkage rationale). */
extern void WNDPROC_CriticalSectionLock(int* stream, char* buf);
void        WNDPROC_StreamReadLine(void* stream, void* outBuf);

/* WNDPROC_Stream::ExtractInt (0x4646C0) — operator>>(int32_t*) on the
 * stream: reads a formatted number, stores the full 32-bit value into the
 * out-param, returns `this` for chaining. Not yet a method on the canonical
 * WNDPROC_Stream class (resources/WndProcStream.h) — declared as a free
 * function here matching this tree's established convention for stream
 * primitives not yet folded into that class (see BuildingDescriptorEditor.cpp's
 * WNDPROC_StreamReadLine/CriticalSectionLock declarations for the same
 * pattern). */
extern "C" void* WNDPROC_Stream__ExtractInt(void* stream, int32_t* out);

/* CRT_wcsstr (0x471480) — real _stricmp-style byte-wise case-insensitive
 * compare, 0 == equal. See BuildingDescriptorEditor.cpp's doc comment for
 * the full LINK-001 evidence trail on why this must NOT be declared
 * extern "C" here. */
extern int32_t CRT_wcsstr(uint8_t* str, uint8_t* sub);

extern size_t WIN32_MemoryStream_Size();

namespace {

/* Adapts this file's char* line buffers / keyword string literals to the
 * real CRT_wcsstr's uint8_t* byte-string signature — identical helper to
 * BuildingDescriptorEditor.cpp's line_is_keyword (file-local, since that
 * one has internal linkage).
 * ABI_BOUNDARY: byte-string CRT comparison over plain char buffers, not a
 * game-object cast — ordinary case-insensitive keyword matching. */
bool line_is_keyword(const char* line, const char* keyword)
{
    return CRT_wcsstr(
        reinterpret_cast<uint8_t*>(const_cast<char*>(line)),
        reinterpret_cast<uint8_t*>(const_cast<char*>(keyword))) == 0;
}

/* Preserved verbatim from the disassembly (`(bVar2 & 1) != 0` continuation
 * check): a per-instance state-flags byte read at an offset given by the
 * stream object's own vtable slot [1] — identical helper to
 * BuildingDescriptorEditor.cpp's dat_stream_state_ok (file-local there,
 * since that one has internal linkage), just testing eofbit instead of the
 * combined 7 (eof|fail|bad) mask, matching this loop's exact original
 * condition. A WNDPROC_Stream-hierarchy internal detail out of scope for
 * this pass (see resources/WndProcStreamBuf.h for the reconstructed class
 * hierarchy, which does not yet cover this specific slot).
 * TODO: replace with a typed accessor once that reconstruction extends to
 * this slot; kept as a raw offset expression with this TODO per project
 * policy in the meantime rather than guessing a typed shape. */
bool dat_stream_eof(void* stream)
{
    int32_t vtable = *reinterpret_cast<int32_t*>(stream);
    int32_t slot1  = *reinterpret_cast<int32_t*>(static_cast<intptr_t>(vtable) + 4);
    uint8_t flags  = *reinterpret_cast<uint8_t*>(
        static_cast<intptr_t>(slot1) + 8 + reinterpret_cast<intptr_t>(stream));
    return (flags & 1) != 0;
}

/* Track-tile classification keyword literals (from the original .rdata;
 * addresses documented for cross-reference — confirmed via read_bytes on
 * each address). */
const char s_tunnel[]        = "tunnel";         /* 0x0047F028 */
const char s_left[]          = "left";           /* 0x0047F020 */
const char s_top[]           = "top";            /* 0x0047F01C */
const char s_right[]         = "right";          /* 0x0047F014 */
const char s_bottom[]        = "bottom";         /* 0x0047F00C */
const char s_depot[]         = "depot";          /* 0x0047F004 */
const char s_bridge[]        = "bridge";         /* 0x0047EFFC */
const char s_horizontal[]    = "horizontal";     /* 0x0047EFF0 */
const char s_vertical[]      = "vertical";       /* 0x0047EFE4 */
const char s_points[]        = "points";         /* 0x0047EFDC */
const char s_switch[]        = "switch";         /* 0x0047EFD4 */
const char s_crosstrack[]    = "crosstrack";     /* 0x0047EFC8 */
const char s_levelcrossing[] = "levelcrossing";  /* 0x0047EFB8 */
const char s_path_x_h[]      = "path-x-h";       /* 0x0047EFAC */
const char s_path_x_v[]      = "path-x-v";       /* 0x0047EFA0 */
const char s_road_x_h[]      = "road-x-h";       /* 0x0047EF94 */
const char s_road_x_v[]      = "road-x-v";       /* 0x0047EF88 */
const char s_station[]       = "station";        /* 0x0047EF80 */
const char s_station_h[]     = "station-h";      /* 0x0047EF74 */
const char s_station_v[]     = "station-v";      /* 0x0047EF68 */

} // namespace

/* ================================================================== */
/* Constructor                                                          */
/* Derived-class portion of AddChild's (0x44B190) inlined construction   */
/* sequence — see class header for the full disassembly evidence.       */
/* ================================================================== */
TrackTileDescriptor::TrackTileDescriptor(uint32_t resId)
    : BuildingDescriptorEditor(resId, 0)
    , tile_type_entries(nullptr)
    , unknown_0x634{0, 0}
    , count1_minus1(0)   /* BUG (preserved, host-determinized): the original
                           * never explicitly initializes this — only
                           * ClassifyTileType (Render) ever writes it, and
                           * AddChild only zeroes tile_type_entries. If Render's
                           * cascade never reaches ClassifyTileType, the
                           * original reads raw operator_new heap garbage
                           * here. Zero-initialized on host for determinism. */
    , count2_field(0)    /* BUG (preserved, host-determinized): same as above. */
    , tile_type(kTileType_None)
    , _pad_63B(0)
{
}

/* ================================================================== */
/* Destructor                                                           */
/* Body: RemoveChild, 0x44B220.                                         */
/* ================================================================== */
TrackTileDescriptor::~TrackTileDescriptor()
{
    if (this->tile_type_entries != nullptr) {
        GLOBAL_free(this->tile_type_entries);
        this->tile_type_entries = nullptr;
    }

    /* Base class ~BuildingDescriptorEditor() runs automatically via the
     * compiler-generated destructor chain — matches the disassembly's
     * explicit `CALL BuildingDescriptorEditor__DtorBody` immediately after
     * this body. */
}

/* ================================================================== */
/* Render — vtable slot [3]                                             */
/* Address: 0x44B4F0 (Ghidra label "RESDATA_ScriptedObject_             */
/* ClassifyTileType")                                                   */
/*                                                                       */
/* Preserves the exact cascading keyword-check control flow from the    */
/* decompilation, matching this tree's established policy (see           */
/* BuildingDescriptorEditor::Render's doc comment) of not simplifying    */
/* verified original control flow even where it reads unusually.        */
/* ================================================================== */
uint8_t TrackTileDescriptor::Render(void* stream)
{
    char lineBuf[264];

    /* Idempotent re-entry guard: HandleEvent's three-phase cascade can
     * invoke this twice (once per archive/disk branch) if the first
     * attempt's earlier phases fail outright. */
    if (this->tile_type_entries != nullptr) {
        GLOBAL_free(this->tile_type_entries);
        this->tile_type_entries = nullptr;
    }

    int32_t count1 = 0;
    int32_t count2 = 0;
    WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
    WNDPROC_Stream__ExtractInt(stream, &count1);
    WNDPROC_Stream__ExtractInt(stream, &count2);

    this->count1_minus1 = static_cast<int16_t>(count1 - 1);
    this->count2_field  = (count2 == 0)
        ? static_cast<int16_t>(0)
        : static_cast<int16_t>(count2 - 1 + count1);

    if (count1 > 0 || count2 > 0) {
        this->tile_type_entries = static_cast<int16_t*>(
            ::operator new(static_cast<size_t>(count1 + count2) * 4, std::nothrow));
    }

    if (this->tile_type_entries != nullptr) {
        for (int32_t i = 0; i < count1; ++i) {
            WNDPROC_StreamReadLine(stream, &this->tile_type_entries[i * 2]);
            WNDPROC_StreamReadLine(stream, &this->tile_type_entries[i * 2 + 1]);
        }
    }

    int16_t terminatorLine = 0;
    WNDPROC_StreamReadLine(stream, &terminatorLine);

    if (this->tile_type_entries != nullptr) {
        for (int32_t i = count1; i < count1 + count2; ++i) {
            WNDPROC_StreamReadLine(stream, &this->tile_type_entries[i * 2]);
            WNDPROC_StreamReadLine(stream, &this->tile_type_entries[i * 2 + 1]);
        }
    }

    WNDPROC_StreamReadLine(stream, &terminatorLine);
    if (terminatorLine != -9) {
        /* Not the numeric -9 section-terminator sentinel (matches the
         * decompiled `(short)local_120 != -9` check exactly — this is a
         * plain integer comparison, unlike the keyword-string matches
         * below, since it's read via WNDPROC_StreamReadLine's numeric-field
         * path rather than WNDPROC_CriticalSectionLock's raw-line-text
         * path): leave tile_type at whatever it already was (matches the
         * original's early return before the classification loop below). */
        return 0;
    }

    /* Directive-keyword classification loop: matches the decompiled
     * do-while exactly — the eofbit-or-empty-line check runs BEFORE each
     * line read (including the very first), not after; a stream that is
     * already at EOF right after the terminator line processes zero
     * keyword lines and returns 1 with tile_type left at its prior value
     * (still kTileType_None from the constructor, on the normal path). */
    for (;;) {
        if (dat_stream_eof(stream)) {
            break;
        }
        WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
        if (lineBuf[0] == '\0') {
            break;
        }

        if (line_is_keyword(lineBuf, s_tunnel)) {
            char sub[16];
            WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), sub);
            if (line_is_keyword(sub, s_left)) {
                this->tile_type = kTileType_TunnelLeft;
            } else if (line_is_keyword(sub, s_top)) {
                this->tile_type = kTileType_TunnelTop;
            } else if (line_is_keyword(sub, s_right)) {
                this->tile_type = kTileType_TunnelRight;
            } else if (line_is_keyword(sub, s_bottom)) {
                this->tile_type = kTileType_TunnelBottom;
            }
        } else if (line_is_keyword(lineBuf, s_depot)) {
            char sub[16];
            WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), sub);
            if (line_is_keyword(sub, s_left)) {
                this->tile_type = kTileType_DepotLeft;
            } else if (line_is_keyword(sub, s_top)) {
                this->tile_type = kTileType_DepotTop;
            } else if (line_is_keyword(sub, s_right)) {
                this->tile_type = kTileType_DepotRight;
            } else if (line_is_keyword(sub, s_bottom)) {
                this->tile_type = kTileType_DepotBottom;
            }
        } else if (line_is_keyword(lineBuf, s_bridge)) {
            char sub[16];
            WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), sub);
            if (line_is_keyword(sub, s_horizontal)) {
                this->tile_type = kTileType_BridgeHorizontal;
            } else if (line_is_keyword(sub, s_vertical)) {
                this->tile_type = kTileType_BridgeVertical;
            }
        } else if (line_is_keyword(lineBuf, s_points)) {
            this->tile_type = kTileType_Points;
        } else if (line_is_keyword(lineBuf, s_switch)) {
            this->tile_type = kTileType_Switch;
        } else if (line_is_keyword(lineBuf, s_crosstrack)) {
            this->tile_type = kTileType_Crosstrack;
        } else if (line_is_keyword(lineBuf, s_levelcrossing)) {
            char sub[16];
            WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), sub);
            if (line_is_keyword(sub, s_path_x_h)) {
                this->tile_type = kTileType_LevelCrossingPathH;
            } else if (line_is_keyword(sub, s_path_x_v)) {
                this->tile_type = kTileType_LevelCrossingPathV;
            } else if (line_is_keyword(sub, s_road_x_h)) {
                this->tile_type = kTileType_LevelCrossingRoadH;
            } else if (line_is_keyword(sub, s_road_x_v)) {
                this->tile_type = kTileType_LevelCrossingRoadV;
            }
        } else if (line_is_keyword(lineBuf, s_station)) {
            char sub[16];
            WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), sub);
            if (line_is_keyword(sub, s_station_h)) {
                this->tile_type = kTileType_StationHorizontal;
            } else if (line_is_keyword(sub, s_station_v)) {
                this->tile_type = kTileType_StationVertical;
            }
        }
    }

    return 1;
}

/* ================================================================== */
/* HandleEvent — Load and parse this child's .dat script                */
/* Address: 0x44B290                                                    */
/*                                                                       */
/* This logic was previously (mis-)transcribed onto ScriptedObject::     */
/* HandleEvent in game/ScriptedObject.cpp; that transcription's stream-  */
/* handling substance was already correct (real WIN32_Stream RAII,       */
/* real state_bits/kBadBit checks), just attached to the wrong class and */
/* using the wrong field offsets/names — ported here verbatim with the   */
/* field/call corrections documented in the class header.               */
/* ================================================================== */
void TrackTileDescriptor::HandleEvent(uint32_t resId, const char* name_suffix)
{
    (void)resId;
    char dat_path[260];
    char asset_path[260];
    int  loaded_size;

    /* Real WIN32_Stream object (resources/Win32Stream.h) — real C++ RAII
     * construction/destruction, matching this tree's established
     * WIN32_StreamOpen/Destroy replacement pattern (see resources/
     * Win32Stream.h's doc comment). */
    WIN32_Stream stream_handle;

    this->tile_type = kTileType_None;   /* +0x63A */
    this->loaded     = 0;                /* +0x162, inherited from ChildWindow */

    /* stream_handle's destructor runs automatically here (real C++ RAII)
     * on every path, including this early return. */
    if (name_suffix == nullptr) {
        return;
    }

    /* Build path strings:
       dat_path  = g_scene_name + name_suffix + ".dat"
       bmpPath   = g_scene_name + name_suffix + ".bmp" (inherited from
                   ChildWindow, +0x48 — NOT a new field on this class) */
    CRT_sprintf_buf(dat_path, "%s%s.dat", g_scene_name, name_suffix);
    CRT_sprintf_buf(this->bmpPath, "%s%s.bmp", g_scene_name, name_suffix);

    /* Try loading from RFD archive (asset manager) first */
    if (g_asset_mgr.archive_file != 0) {
        uint8_t* file_data;     /* AssetArchive::LoadFile's real return type */
        void* stream_obj;
        WNDPROC_Stream* parsed_stream;

        CRT_sprintf_buf(asset_path, "%s.dat", name_suffix);
        file_data = g_asset_mgr.LoadFile(
            reinterpret_cast<uint8_t*>(asset_path), &loaded_size);

        if (file_data != nullptr) {
            stream_obj = ::operator new(WIN32_MemoryStream_Size(), std::nothrow);
            if (stream_obj != nullptr) {
                // ABI_BOUNDARY: WNDPROC_StreamFromMemory's `char* data` param is
                // this codebase's older byte-buffer convention; file_data is the
                // same raw bytes under AssetArchive::LoadFile's real `uint8_t*`
                // return type.
                parsed_stream = WNDPROC_StreamFromMemory(
                    stream_obj, reinterpret_cast<char*>(file_data), loaded_size, 1);

                if (parsed_stream != nullptr) {
                    if ((parsed_stream->state_bits & StreamObject::kBadBit) == 0) {
                        uint8_t ok = this->BuildingDescriptorEditor::Render(parsed_stream);
                        this->loaded = ok;

                        if (ok != 0) {
                            ok = this->ChildWindow::Render(parsed_stream);
                        }
                        this->loaded = ok;

                        if (ok != 0) {
                            ok = this->Render(parsed_stream);  /* virtual self — ClassifyTileType */
                        }
                        this->loaded = ok;
                    }
                    delete parsed_stream;
                }
            }
            CRT_free(file_data);
        }
    }

    /* Fall back to disk file I/O if archive load didn't succeed */
    if (this->loaded == 0) {
        stream_handle.OpenPath(dat_path, 0x20, g_stream_open_flags);

        if ((stream_handle.state_bits & StreamObject::kBadBit) == 0) {
            uint8_t ok = this->BuildingDescriptorEditor::Render(&stream_handle);
            this->loaded = ok;

            if (ok != 0) {
                ok = this->ChildWindow::Render(&stream_handle);
            }
            this->loaded = ok;

            if (ok != 0) {
                ok = this->Render(&stream_handle);  /* virtual self — ClassifyTileType */
            }
            this->loaded = ok;
        }

        stream_handle.CloseNow();
    }

    /* stream_handle's destructor runs automatically here (real C++ RAII). */
}

/* ================================================================== */
/* TrackTileDescriptor_Ctor — placement-new + load compatibility bridge */
/* ================================================================== */
void* TrackTileDescriptor_Ctor(void* memory, int32_t resId, const char* name)
{
    if (memory == nullptr) {
        return nullptr;
    }

    TrackTileDescriptor* obj = new (memory) TrackTileDescriptor(static_cast<uint32_t>(resId));

    /* `name` is ResourceManager::AddString's resource-dispatch string
     * parameter (resources/ResourceManager.cpp), a real `const char*` (see
     * ui/UI_ChildWindow.h's ChildWindow constructor doc for why the
     * original's int32_t ABI slot is not reproduced as a pointer-through-
     * integer round trip here). This resource type's real function
     * (0x44B290) is proven — via its CRT_sprintf_buf "%s%s.dat"/g_scene_name/
     * name_suffix call shape, already independently reverse-engineered in
     * game/ScriptedObject.cpp before being moved here — to dereference it as
     * the child's name-suffix string. */
    obj->HandleEvent(static_cast<uint32_t>(resId), name);

    return obj;
}
