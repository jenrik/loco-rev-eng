/**
 * BuildingDescriptorEditor.cpp — see BuildingDescriptorEditor.h for the full
 * class-level evidence trail and naming rationale.
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "BuildingDescriptorEditor.h"

#include <cstdio>
#include <cstring>
#include <new>

/* GLOBAL_free has plain C++ linkage in this tree (see game/Panel.h,
 * shared/crt_stubs.h, game/TrainStation.cpp) — declared here to match,
 * not inside the extern "C" block below. */
extern void GLOBAL_free(void* ptr);                     /* 0x465CD0 */

/* GetResourceType also has plain C++ linkage (resources/ResourceManager.h). */
#include "../resources/ResourceManager.h"

/* UI_CreateChildWindow/UI_ChildWindow_Dtor/UI_ChildWindow_Render — the
 * canonical declarations now live in ui/UI_ChildWindow.h (a separate,
 * independently-verified pass this same session concluded the same thing
 * this class's header documents: no real C++ inheritance, free functions
 * shared by composition). Included rather than re-declared locally to
 * avoid a second, possibly-drifting declaration of the same symbols. */
#include "../ui/UI_ChildWindow.h"

/* ================================================================== */
/* External helpers (extern "C" — original Win32/CRT/internal ABI)     */
/* ================================================================== */
extern "C" {
    int   AssetMgr_LoadFile(void* mgr, const char* path, int* outSize); /* 0x45CD00 */
    void  CRT_free(void* ptr);
    int   CRT_sprintf_buf(void* buf, const void* fmt); /* chained sprintf-family helper, exact
                                                          * shape unresolved — matches other files'
                                                          * usage of the same call pattern */
    void  SetRectEmpty(RECT* rect);

    /* Win32-stream family (see resources/WndProcStreamBuf.h / Win32StreamFile.h
     * for the reconstructed class hierarchy; these free-function entry
     * points that DRIVE that hierarchy — WIN32_StreamOpen/OpenPath/Destroy —
     * are declared here matching their established call shape elsewhere
     * in this tree, e.g. game/TrainStation.cpp, ui/CursorEditWindow.cpp). */
    void  WIN32_StreamOpen(void* streamOut, int mode);
    void  WIN32_StreamOpenPath(void* streamOut, const char* path, int mode, int unk);
    void  WIN32_StreamDestroy(int streamHandle);
    void  WIN32_StreamDestroyImmediate(void* stream);
    void  WNDPROC_StreamCleanup(void* stream);
    void* WNDPROC_StreamFromMemory(void* stream, const char* data, int size, int mode);

    /* .dat directive-line scanning primitives. These are callees of
     * parse_dat_directive_line/draw_border_grid/paint_edit_regions/
     * edit_key_handler_parse — not part of this pass's assigned function
     * list. Their exact identity (return semantics, real names) is NOT
     * resolved here; signatures match the decompiler's own inferred call
     * shape exactly (chained pointer return = an advancing stream/token
     * cursor). TODO: decompile these directly in a follow-up pass instead
     * of trusting the inferred shape below. */
    void* WNDPROC_StreamPrintf(void* stream, void* outBuf);
    void  WNDPROC_StreamReadLine(void* stream, void* outBuf);
    void* WNDPROC_StreamWrite(void* stream, void* outBuf);
    void  WNDPROC_StreamSeekForward(void* stream, void* buf, int32_t size, int ch);
    void  WNDPROC_EnterCriticalSection(void* cs);
    void  WNDPROC_LeaveCriticalSection(void* cs);
    void* _CrtDbg_report_fmt_helper(void* buf, const void* fmt); /* sscanf-shaped; identity unresolved, see TODO above */
    void* CRT_wcsstr(const void* haystack, const void* needle);
    int   CRT_mbstowcs_s(void* buf, const void* dst, int n);     /* TODO: identity unresolved — see .h comment on field_28 */
    void  CRT_fmod(void* stream, void* outByte);                 /* TODO: identity unresolved — likely misdecompiled */
    void* CRT_fabs(void* a, void* b);                            /* TODO: identity unresolved — likely misdecompiled */
}

/* WNDPROC_CriticalSectionLock has C++ mangled linkage (real def at 0x4647A0:
 * _Z27WNDPROC_CriticalSectionLockPiPc, i.e. (int*, char*)).
 * Declared outside extern "C" to match the real definition. */
extern void WNDPROC_CriticalSectionLock(int* stream, char* buf);

/* Section-keyword string literals (from the original .rdata; addresses
 * documented for cross-reference, not reproduced here byte-for-byte —
 * these are ASCII renderings of the original wide-char keyword strings). */
static const char s_physical_occupancy[] = "physical_occupancy";   /* 0x0047E5F4 */
static const char s_bitmap_occupancy[]   = "bitmap_occupancy";     /* 0x0047E5E0 */
static const char s_entry_exit[]         = "entry_exit";           /* 0x0047E5D4 */
static const char s_RMBSeq[]             = "RMBSeq";               /* 0x0047E5CC */
static const char s_ClosedFS[]           = "ClosedFS";             /* 0x0047E5C0 */
static const char s_EEReplayDelay[]      = "EEReplayDelay";        /* 0x0047E5B0 */
static const char s_LeisureDestination[] = "LeisureDestination";   /* 0x0047E594 */
static const char s_MaxEmployees[]       = "MaxEmployees";         /* 0x0047E584 */
static const char s_PossibleEmployees[]  = "PossibleEmployees";    /* 0x0047E570 */
static const char s_PossibleMinifigs[]   = "PossibleMinifigs";     /* 0x0047E55C */
static const char s_shifts[]             = "shifts";               /* 0x0047E554 */
static const char s_FreeToRoam[]         = "FreeToRoam";           /* 0x0047E538 */
static const char s_ButtonVisible[]      = "ButtonVisible";        /* 0x0047E528 */
static const char s_InsertSeq[]          = "InsertSeq";            /* 0x0047E51C */
static const char s_MobileSeq[]          = "MobileSeq";            /* 0x0047E510 */
static const char s_TotalVisits[]        = "TotalVisits";          /* 0x0047E504 */

/* ================================================================== */
/* Constructor                                                          */
/* Address: 0x41E570 (Ghidra label "INPUT_ExitGame" — misnomer)         */
/* ================================================================== */
BuildingDescriptorEditor::BuildingDescriptorEditor(uint32_t resId, int32_t nameParam)
{
    UI_CreateChildWindow(this, resId, 0);

    TrackPos_Init(&this->track_pos_a);
    TrackPos_Init(&this->track_pos_b);

    /* Binary writes the vtable pointer here; C++ emits it. */

    this->insert_seq.key_ids  = nullptr;   /* +0x564 */
    this->mobile_seq.key_ids  = nullptr;   /* +0x598 */
    this->total_visits.key_ids = nullptr;  /* +0x5CC */

    handle_edit_message(resId, static_cast<int32_t>(nameParam));
}

/* ================================================================== */
/* Destructor body                                                      */
/* Address: 0x41E620 (Ghidra label "INPUT_CreateEditControl" — misnomer)*/
/* Wrapper (scalar deleting destructor, vtable[0]):                     */
/* Address: 0x41E600 (Ghidra label "INPUT_DtorWrapper")                 */
/* ================================================================== */
BuildingDescriptorEditor::~BuildingDescriptorEditor()
{
    if (this->insert_seq.key_ids != nullptr) {
        GLOBAL_free(this->insert_seq.key_ids);
        this->insert_seq.key_ids = nullptr;
    }
    if (this->mobile_seq.key_ids != nullptr) {
        GLOBAL_free(this->mobile_seq.key_ids);
        this->mobile_seq.key_ids = nullptr;
    }
    if (this->total_visits.key_ids != nullptr) {
        GLOBAL_free(this->total_visits.key_ids);
        this->total_visits.key_ids = nullptr;
    }

    TrackPos_BaseInit(&this->track_pos_a);
    TrackPos_BaseInit(&this->track_pos_b);

    UI_ChildWindow_Dtor(this);
}

/* ================================================================== */
/* handle_edit_message                                                  */
/* Address: 0x41E6E0 (Ghidra label "INPUT_HandleEditMessage")           */
/* ================================================================== */
void BuildingDescriptorEditor::handle_edit_message(uint32_t resId, int32_t nameParam)
{
    (void)resId;

    /* Reset all descriptor fields to their sentinel defaults. */
    this->max_employees      = 0;
    this->ee_replay_delay    = 0;
    for (int i = 0; i < 5; ++i) {
        this->possible_minifigs[i]  = -1;
        this->possible_employees[i] = -1;
    }
    this->border_width  = 0;
    this->border_height = 0;
    this->border_depth  = 0;
    this->rmb_seq             = 0;
    this->field_532           = 0;
    this->closed_fs           = -1;
    this->ee_replay_delay_data = 0;

    std::memset(&this->insert_seq, 0, sizeof(KeySequenceRecord));
    std::memset(&this->mobile_seq, 0, sizeof(KeySequenceRecord));
    std::memset(&this->total_visits, 0, sizeof(KeySequenceRecord));

    SetRectEmpty(&this->free_to_roam_rect);
    this->leisure_destination = 0;
    this->loaded_flag = 0;

    if (nameParam == 0) {
        return;
    }

    /* The original builds "%s%s.dat" / "%s%s.bmp" paths from the
     * install path and the resource-name string, tries the AssetMgr
     * archive first, then falls back to WIN32_StreamOpenPath. Both
     * paths dispatch through vtable slot [3] (parse_dat_directive_line)
     * and then UI_ChildWindow_Render. This host reconstruction preserves
     * that two-path structure faithfully; the exact sprintf/path-buffer
     * plumbing (CRT_sprintf_buf's chained-call shape) is not
     * independently re-verified in this pass beyond matching the
     * decompiled call sequence.
     *
     * The original persists the .bmp path into a field at +0x48 (the
     * same offset ui/CursorEditWindow.h documents as `bmpPath[280]` for
     * its sibling class — further confirming the shared ChildWindow-family
     * convention). No decompiled method in this pass reads that field
     * back, so it is intentionally kept as a local write-only buffer here
     * rather than a persistent (and otherwise dead) class member. */
    char datPath[264];
    char bmpPathBuf[264];
    CRT_sprintf_buf(datPath, "%s%s.dat");
    CRT_sprintf_buf(bmpPathBuf, "%s%s.bmp");

    bool loadedFromArchive = false;
    extern void* g_asset_mgr;
    if (g_asset_mgr != nullptr) {
        int fileSize = 0;
        char archivePath[264];
        CRT_sprintf_buf(archivePath, "%s.dat");
        int* fileData = reinterpret_cast<int*>(AssetMgr_LoadFile(&g_asset_mgr, archivePath, &fileSize));
        if (fileData != nullptr) {
            void* streamMem = ::operator new(0x5C, std::nothrow);
            if (streamMem != nullptr) {
                int* stream = reinterpret_cast<int*>(
                    WNDPROC_StreamFromMemory(streamMem, reinterpret_cast<const char*>(fileData), fileSize, 1));
                if (stream != nullptr) {
                    bool ok = parse_dat_directive_line(stream);
                    this->loaded_flag = ok ? 1 : 0;
                    if (ok) {
                        bool rendered = UI_ChildWindow_Render(this, stream) != 0;
                        this->loaded_flag = rendered ? 1 : 0;
                    }
                    loadedFromArchive = (this->loaded_flag != 0);
                    /* Original tail dispatches the stream's own scalar deleting
                     * destructor (vtable[0](1)) here; not reproduced with a raw
                     * vtable call per project policy — the stream is a
                     * WNDPROC_Stream-family object whose typed destructor is
                     * out of scope for this pass. */
                }
            }
            CRT_free(fileData);
        }
    }

    if (!loadedFromArchive) {
        int streamHandle[2];
        WIN32_StreamOpenPath(streamHandle, datPath, 0x20, 0 /* DAT_00479190 */);
        bool ok = parse_dat_directive_line(streamHandle);
        this->loaded_flag = ok ? 1 : 0;
        if (ok) {
            bool rendered = UI_ChildWindow_Render(this, streamHandle) != 0;
            this->loaded_flag = rendered ? 1 : 0;
        }
        WIN32_StreamDestroyImmediate(streamHandle);
    }

    /* If neither default edit-origin field was set by the .dat, derive a
     * default from the parsed occupancy dimensions (disassembly-evidenced,
     * see the field comment on edit_origin_x/edit_origin_y above). */
    if (this->edit_origin_x == 0 && this->edit_origin_y == 0) {
        this->edit_origin_x = static_cast<int16_t>((this->bitmap_occupancy_width >> 1) << 4);
        this->edit_origin_y = static_cast<int16_t>(this->bitmap_occupancy_height * 0x10 - 0x10);
    }
}

/* ================================================================== */
/* parse_dat_directive_line — vtable slot [3]                          */
/* Address: 0x41E9F0 (Ghidra label "INPUT_EditWndProc")                 */
/*                                                                      */
/* Preserves the exact cascading keyword-check control flow from the   */
/* decompilation; see the header for a description of what each        */
/* branch does. Not simplified per project policy even where the       */
/* "keyword not found -> parse as current section's data line" polarity */
/* reads unusually — that is the verified original control flow.       */
/* ================================================================== */
namespace {

/* Preserved verbatim from the disassembly:
 *   (*(byte*)(*(int*)(*(int*)stream + 4) + 8 + (int)stream) & 7) == 0
 * i.e. a per-instance state-flags byte read at an offset given by the
 * stream object's own vtable slot [1] — a WNDPROC_Stream-hierarchy
 * internal detail out of scope for this pass (see
 * resources/WndProcStreamBuf.h for the reconstructed class hierarchy,
 * which does not yet cover this specific slot).
 * TODO: replace with a typed accessor once that reconstruction extends
 * to this slot; kept as a raw offset expression with this TODO per
 * project policy in the meantime rather than guessing a typed shape. */
bool dat_stream_state_ok(void* stream)
{
    int32_t vtable = *reinterpret_cast<int32_t*>(stream);
    int32_t slot1  = *reinterpret_cast<int32_t*>(static_cast<intptr_t>(vtable) + 4);
    uint8_t flags  = *reinterpret_cast<uint8_t*>(
        static_cast<intptr_t>(slot1) + 8 + reinterpret_cast<intptr_t>(stream));
    return (flags & 7) == 0;
}

} // namespace

bool BuildingDescriptorEditor::parse_dat_directive_line(void* stream)
{
    bool keepProcessing = true;
    int minifigsFound = 0;
    char lineBuf[264];

    WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);

    /* Outer loop: process directive lines until the stream's own
     * terminator token is hit (matches the decompiled while loop). The
     * original also tests CRT_wcsstr(lineBuf, &DAT_0047e3cc) here; that
     * unnamed constant is not identified in this pass (plausibly an empty
     * marker string, which would make the wcsstr test tautologically
     * true) so only the identified stream-state check is preserved. */
    while (dat_stream_state_ok(stream)) {
        if (CRT_wcsstr(lineBuf, s_physical_occupancy) == nullptr) {
            draw_border_grid(stream);
        } else if (CRT_wcsstr(lineBuf, s_bitmap_occupancy) == nullptr) {
            uint16_t tmpW = 0, tmpH = 0;
            void* p = WNDPROC_StreamPrintf(stream, &tmpW);
            WNDPROC_StreamPrintf(p, &tmpH);
            this->bitmap_occupancy_width  = static_cast<uint8_t>(tmpW);
            this->bitmap_occupancy_height = static_cast<uint8_t>(tmpH);
            uint8_t* cell = this->bitmap_occupancy_grid;
            if (this->bitmap_occupancy_height != 0) {
                for (int row = 0; row < this->bitmap_occupancy_height; ++row) {
                    uint8_t* rowCell = cell;
                    if (this->bitmap_occupancy_width != 0) {
                        for (int col = 0; col < this->bitmap_occupancy_width; ++col) {
                            uint16_t v = 0;
                            WNDPROC_StreamPrintf(stream, &v);
                            *rowCell = static_cast<uint8_t>(v);
                            rowCell += 9;
                        }
                    }
                    /* Original advances the row start by only 1 byte, not by
                     * width*9 — rows overlap. Preserved faithfully. */
                    cell = cell + 1;
                }
            }
        } else if (CRT_wcsstr(lineBuf, s_entry_exit) == nullptr) {
            paint_edit_regions(stream);
        } else if (CRT_wcsstr(lineBuf, s_RMBSeq) == nullptr) {
            WNDPROC_StreamReadLine(stream, &this->rmb_seq);
        } else if (CRT_wcsstr(lineBuf, s_ClosedFS) == nullptr) {
            WNDPROC_StreamReadLine(stream, &this->closed_fs);
        } else if (CRT_wcsstr(lineBuf, s_EEReplayDelay) == nullptr) {
            WNDPROC_StreamWrite(stream, &this->ee_replay_delay_data);
        } else if (CRT_mbstowcs_s(lineBuf, nullptr, 4) == 0) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->ee_replay_delay = static_cast<uint8_t>(v);
            if (this->ee_replay_delay > 5) {
                this->ee_replay_delay = 5;
            }
        } else if (CRT_wcsstr(lineBuf, s_LeisureDestination) == nullptr) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->leisure_destination = static_cast<uint8_t>(v);
        } else if (CRT_wcsstr(lineBuf, s_MaxEmployees) == nullptr) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->max_employees = static_cast<uint8_t>(v);
            if (this->max_employees > 5) {
                this->max_employees = 5;
            }
        } else if (CRT_wcsstr(lineBuf, s_PossibleEmployees) == nullptr) {
            for (int i = 0; i < 5; ++i) {
                WNDPROC_StreamReadLine(stream, &this->possible_employees[i]);
                UINT type = GetResourceType(static_cast<UINT>(this->possible_employees[i]));
                if (type == 7 && this->possible_employees[i] != -1) {
                    /* Original's signed-parity check on the resource id;
                     * preserved as the equivalent odd/even test. */
                    if ((this->possible_employees[i] & 1) != 0) {
                        this->possible_employees[i] = -1;
                    }
                }
            }
        } else if (CRT_wcsstr(lineBuf, s_PossibleMinifigs) == nullptr) {
            minifigsFound = 0;
            for (int i = 0; i < 5; ++i) {
                WNDPROC_StreamReadLine(stream, &this->possible_minifigs[i]);
                UINT type = GetResourceType(static_cast<UINT>(this->possible_minifigs[i]));
                if (type == 7 && this->possible_minifigs[i] != -1) {
                    if ((this->possible_minifigs[i] & 1) != 0) {
                        this->possible_minifigs[i] = -1;
                    }
                }
                if (this->possible_minifigs[i] != -1) {
                    ++minifigsFound;
                }
            }
        } else if (CRT_wcsstr(lineBuf, s_shifts) == nullptr) {
            /* Reads 4 longs for logging/validation only — the decompiled
             * body never stores them into `this`. */
            WNDPROC_EnterCriticalSection(stream);
            WNDPROC_StreamSeekForward(stream, lineBuf, 0x104, 10);
            WNDPROC_LeaveCriticalSection(stream);
            _CrtDbg_report_fmt_helper(lineBuf, "%ld %ld %ld %ld");
        } else if (CRT_wcsstr(lineBuf, s_FreeToRoam) == nullptr) {
            WNDPROC_StreamWrite(stream, &this->free_to_roam_rect.left);
            WNDPROC_StreamWrite(stream, &this->free_to_roam_rect.top);
            WNDPROC_StreamWrite(stream, &this->free_to_roam_rect.right);
            WNDPROC_StreamWrite(stream, &this->free_to_roam_rect.bottom);
        } else if (CRT_wcsstr(lineBuf, s_ButtonVisible) == nullptr) {
            uint16_t v = 0;
            WNDPROC_StreamPrintf(stream, &v);
            this->button_visible = static_cast<uint8_t>(v);
        } else if (CRT_wcsstr(lineBuf, s_InsertSeq) == nullptr) {
            edit_key_handler_parse(stream, &this->insert_seq);
        } else if (CRT_wcsstr(lineBuf, s_MobileSeq) == nullptr) {
            edit_key_handler_parse(stream, &this->mobile_seq);
        } else if (CRT_wcsstr(lineBuf, s_TotalVisits) == nullptr) {
            edit_key_handler_parse(stream, &this->total_visits);
        } else {
            keepProcessing = false;
            break;
        }

        WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
    }

    if (!dat_stream_state_ok(stream)) {
        keepProcessing = false;
    }

    this->border_scale_byte = static_cast<uint8_t>(
        (this->border_height * 15 + this->bitmap_occupancy_height) * 16);

    if (minifigsFound == 0) {
        this->ee_replay_delay = 0;
    }

    return keepProcessing;
}

/* ================================================================== */
/* draw_border_grid                                                     */
/* Address: 0x41EFA0 (Ghidra label "INPUT_DrawEditBorder")              */
/* ================================================================== */
bool BuildingDescriptorEditor::draw_border_grid(void* stream)
{
    uint16_t depth = 0, width = 0, height = 0;
    void* p = WNDPROC_StreamPrintf(stream, &depth);
    p = WNDPROC_StreamPrintf(p, &width);
    WNDPROC_StreamPrintf(p, &height);

    this->border_height = static_cast<uint8_t>(width);   /* matches decompile's local_12[0] -> +0x16A ordering */
    this->border_width  = static_cast<uint8_t>(depth);   /* +0x168 */
    this->border_depth  = static_cast<uint8_t>(height);  /* +0x169 */

    std::memset(this->physical_occupancy_grid, 0, sizeof(this->physical_occupancy_grid));

    uint8_t* outer = this->physical_occupancy_grid;
    if (this->border_height != 0) {
        for (int o = 0; o < this->border_height; ++o) {
            uint8_t* middle = outer;
            if (this->border_depth != 0) {
                for (int m = 0; m < this->border_depth; ++m) {
                    uint8_t* inner = middle;
                    if (this->border_width != 0) {
                        for (int i = 0; i < this->border_width; ++i) {
                            uint16_t v = 0;
                            WNDPROC_StreamPrintf(stream, &v);
                            *inner = static_cast<uint8_t>(v);
                            inner += 0x3F;
                        }
                    }
                    middle += 7;
                }
            }
            outer += 1;
        }
    }

    return true;
}

/* ================================================================== */
/* paint_edit_regions                                                    */
/* Address: 0x41F0C0 (Ghidra label "INPUT_PaintEdit")                   */
/* ================================================================== */
bool BuildingDescriptorEditor::paint_edit_regions(void* stream)
{
    uint16_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;
    void* p = WNDPROC_StreamPrintf(stream, &v0);
    p = WNDPROC_StreamPrintf(p, &v1);
    p = WNDPROC_StreamPrintf(p, &v2);
    WNDPROC_StreamPrintf(p, &v3);

    for (int i = 0; i < 8; ++i) {
        this->edit_region[i] = -1;
    }

    if (v0 != 0) {
        this->edit_region[0] = 0;
        uint32_t val = (v0 == 2)
            ? static_cast<uint32_t>(this->bitmap_occupancy_height * 0x20) >> 2
            : v0;
        this->edit_region[1] = static_cast<int32_t>(val);
    }

    if (v1 != 0) {
        uint32_t val1 = (v1 == 2)
            ? static_cast<uint32_t>(this->bitmap_occupancy_width * 0x20) >> 2
            : v1;
        this->edit_region[2] = static_cast<int32_t>(val1);
        this->edit_region[3] = static_cast<int32_t>(this->bitmap_occupancy_height) * 0x10 - 1;
    }

    if (v2 != 0) {
        this->edit_region[4] = static_cast<int32_t>(this->bitmap_occupancy_width) * 0x10 - 1;
        if (v2 < 4) {
            this->edit_region[5] = static_cast<int32_t>(
                (static_cast<uint32_t>(this->bitmap_occupancy_height) * v2 * 0x10) >> 2);
        } else {
            this->edit_region[5] = v2;
        }
    }

    if (v3 != 0) {
        int32_t rowSpan = (static_cast<int32_t>(this->bitmap_occupancy_height) -
                            static_cast<int32_t>(this->border_depth)) * 0x10;
        if (v3 < 4) {
            this->edit_region[6] = static_cast<int32_t>(
                (static_cast<uint32_t>(this->bitmap_occupancy_width) * v3 * 0x10) >> 2);
        } else {
            this->edit_region[6] = v3;
        }
        this->edit_region[7] = rowSpan;
    }

    return true;
}

/* ================================================================== */
/* edit_key_handler_parse — free function, NOT a member                 */
/* Address: 0x41F2B0 (Ghidra label "INPUT_EditKeyHandler")              */
/* ================================================================== */
uint32_t edit_key_handler_parse(void* stream, KeySequenceRecord* record)
{
    if (record == nullptr) {
        return 0;
    }

    char lineBuf[264];

    /* Decompiled as two calls to a function Ghidra labeled CRT_fabs with
     * (stream, &record->key_count)-shaped arguments — almost certainly a
     * misidentified thunk (CRT_fabs takes a double, not these arguments).
     * Preserved as an opaque forwarding call pending direct verification;
     * TODO: decompile the real callee at this call site directly. */
    void* tmp = CRT_fabs(stream, &record->key_count);
    CRT_fabs(tmp, &record->key_count);

    if (record->key_count != 0 && record->key_count < 0x2D) {
        record->key_ids = static_cast<int32_t*>(::operator new(
            static_cast<size_t>(record->key_count) * 4, std::nothrow));
        if (record->key_ids != nullptr) {
            for (int32_t i = 0; i < record->key_count; ++i) {
                WNDPROC_StreamWrite(stream, &record->key_ids[i]);
            }
        } else {
            record->key_count = 0;
        }
    } else {
        record->key_count = 0;
    }

    WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
    WNDPROC_StreamWrite(stream, &record->field_0C);
    WNDPROC_StreamReadLine(stream, &record->field_10);
    WNDPROC_StreamWrite(stream, &record->resource_id_0);
    WNDPROC_StreamReadLine(stream, &record->field_18);
    WNDPROC_StreamWrite(stream, &record->field_1C);
    WNDPROC_StreamWrite(stream, &record->resource_id_1);
    WNDPROC_StreamReadLine(stream, &record->field_24);

    uint8_t derived = 0;
    CRT_fmod(stream, &derived);
    WNDPROC_StreamWrite(stream, &record->field_2C);
    WNDPROC_StreamWrite(stream, &record->field_30);
    record->field_28 = derived;

    if (record->resource_id_0 > 0) {
        UINT t = GetResourceType(static_cast<UINT>(record->resource_id_0));
        if (t != 2 && t != 4 && t != 0xD && t != 0xC) {
            record->resource_id_1 = -1;
        }
    }
    if (record->resource_id_1 > 0) {
        UINT t = GetResourceType(static_cast<UINT>(record->resource_id_1));
        if (t != 7) {
            record->resource_id_1 = -1;
        }
    }

    return 1;
}

/* ================================================================== */
/* BuildingDescriptorEditor_Ctor — placement-new compatibility bridge   */
/* ================================================================== */
void* BuildingDescriptorEditor_Ctor(void* memory, int32_t resId, int32_t strPtr)
{
    if (memory == nullptr) {
        return nullptr;
    }
    return new (memory) BuildingDescriptorEditor(static_cast<uint32_t>(resId), strPtr);
}
