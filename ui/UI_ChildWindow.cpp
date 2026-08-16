/**
 * UI_ChildWindow.cpp — ChildWindow base class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "UI_ChildWindow.h"

#include <cassert>
#include <cstdio>
#include <cstring>

// Status: INTEGRATED

/* ================================================================== */
/* External references (all already implemented elsewhere)             */
/* ================================================================== */

extern "C" {
void   __cdecl GLOBAL_free(void* ptr);                       /* 0x465CD0 */
void*  __cdecl operator_new(size_t size);                     /* 0x465CE0 */
void*  __thiscall ResourceManager_GetById(void* mgr, int32_t resId); /* 0x446EA0 */

void   WNDPROC_EnterCriticalSection(void* cs);
void   WNDPROC_LeaveCriticalSection(void* cs);
void   WNDPROC_StreamSeekForward(void* stream, void* buf, int32_t size, int ch);
/* Ghidra mislabels this call CRT_fabs (which really takes a double) —
 * same unresolved-identity caveat already flagged for the identical call
 * shape in input/BuildingDescriptorEditor.cpp's edit_key_handler_parse.
 * Left inside extern "C" (matches its actual C-linkage stub definition in
 * shared/stubs_impl.cpp) — NOT part of the 2026-08-11 linkage fix below,
 * since its own identity is still unresolved; do not assume it shares
 * the WNDPROC_Stream-family fix without re-checking. */
void*  CRT_fabs(void* stream, void* outBuf);
}
/* GetResourceType has plain C++ linkage (resources/ResourceManager.h) —
 * declared outside the extern "C" block above, not inside it. */
extern unsigned int GetResourceType(unsigned int resourceId);  /* 0x446030 */

/* CRT_wcsstr — real address 0x471480, Ghidra's stale auto-name (it is NOT
 * a wide-string search: disassembly shows a byte-wise, uppercase-
 * normalizing compare loop terminating on NUL/mismatch, i.e. the genuine
 * CRT `_stricmp`/`strcasecmp`, returning 0 for "equal, case-insensitively"
 * and nonzero otherwise). Confirmed directly against ChildWindow::Render's
 * own disassembly (0x424E00): every call site here is `CALL 0x471480;
 * TEST EAX,EAX; J[N]Z ...`, taking the "matched keyword" path precisely
 * when EAX == 0 (e.g. 0x424E43-0x424E4D for the terminator check,
 * 0x424E6D-0x424E77 for "button").
 *
 * FIXED 2026-08-16 (live call-0 landmine): this was previously declared
 * `void* CRT_wcsstr(const void*, const void*)` INSIDE the extern "C" block
 * above. C linkage discards parameter/return types from the link-time
 * symbol name, so that declaration collapsed onto the bare, unmangled
 * `CRT_wcsstr` symbol — which resolves to shared/defsym_stubs.cpp's
 * zero-argument, void-returning inert filler stub
 * (`extern "C" void CRT_wcsstr() { }`), not any real string-compare body.
 * Every one of this function's ~16 call sites below was therefore calling
 * a mismatched-arity C function through a stale prototype: both real
 * arguments were silently dropped and the "return value" tested afterward
 * was uninitialized/garbage in EAX — i.e. this entire `.dat`/descriptor
 * line-directive parser was undefined-behavior-driven, not merely
 * degraded, before this fix.
 *
 * The correct real implementation (case-fold 3-way compare via
 * `strcasecmp`, matching 0x471480's semantics) is defined in shared/
 * stubs_link001_batch1_crt_win32.cpp under PLAIN C++ linkage with this
 * exact `(uint8_t*, uint8_t*)` signature — matching that signature exactly
 * here (rather than reintroducing a differently-shaped overload) is what
 * makes this declaration bind to that real body instead of either the
 * defsym_stubs.cpp no-op or shared/stubs_impl.cpp's own differently-typed
 * (and separately known-broken, substring-only) `CRT_wcsstr(const char*,
 * const char*)` overload (see PROGRESS.md's tracked "wrong semantic"
 * item) — neither of those is the target here. */
int32_t CRT_wcsstr(uint8_t* str, uint8_t* sub);

/* ResourceManager_GetStringById/RESMGR_LoadSoundResource/
 * RESMGR_ReleaseSoundResource: same extern-"C"-linkage landmine as
 * GetResourceType above — all three call sites below are inside `#ifdef
 * _WIN32` (dead on this host, exercised only by the MinGW typecheck build),
 * so this was latent rather than live, but the declarations were still
 * wrong on their own terms: `uint32_t id`/`void*` return desynchronized
 * ResourceManager_GetStringById from the real facade's `int id -> int`
 * signature (shared/stubs_link001_batch3_resource_audio.cpp), and
 * RESMGR_LoadSoundResource/ReleaseSoundResource's `void* resHandle` doesn't
 * match the corrected int-handle convention either. Retyped for
 * consistency; neither RESMGR_LoadSoundResource nor
 * RESMGR_ReleaseSoundResource has a real implementation anywhere in the
 * tree yet regardless of signature (see PROGRESS.md). */
int  ResourceManager_GetStringById(void* mgr, int id);      /* 0x4472B0 */
int  RESMGR_LoadSoundResource(int resHandle);
void RESMGR_ReleaseSoundResource(int resHandle);

/* WNDPROC_CriticalSectionLock/StreamReadLine/StreamPrintf/StreamWrite have
 * C++ mangled linkage (matches every other file in this tree that calls
 * them — see input/BuildingDescriptorEditor.cpp, game/TrainStation.cpp;
 * WNDPROC_CriticalSectionLock's real def at 0x4649F0 -- NOT 0x4647A0, an
 * earlier session's transcription error, corrected 2026-08-10, see
 * PROGRESS.md "WNDPROC_Stream facade recovery" --
 * _Z27WNDPROC_CriticalSectionLockPiPc). Used only by Render() below.
 *
 * WNDPROC_StreamReadLine/StreamPrintf/StreamWrite were previously (wrongly)
 * inside the extern "C" block above, which bound them to unmangled,
 * undefined symbols at link time (a null-pointer call at every call site
 * in this file, via -Wl,--unresolved-symbols=ignore-all) — the exact same
 * defect class as the ui/HelpWnd.cpp landmine fixed in the 2026-08-10
 * "WNDPROC_Stream facade recovery" session. Fixed 2026-08-11.
 *
 * NOTE: WNDPROC_StreamReadLine is declared `void`-returning in
 * BuildingDescriptorEditor.cpp (whose call sites never use the return
 * value) — this function's "button" directive genuinely chains the
 * return through a second call
 * (`WNDPROC_StreamPrintf(WNDPROC_StreamReadLine(WNDPROC_StreamReadLine(...)))`),
 * so it's declared `void*`-returning here instead. Return type doesn't
 * affect C++ mangling for a free function, so both declarations bind to
 * the same symbol — that only fixes the *linkage* mismatch this comment
 * is about. It does NOT make reading the return value safe against the
 * real definition: shared/stubs_impl.cpp's WNDPROC_StreamReadLine is
 * `void`-returning and currently aborts before returning anything, so
 * this file's chained read is inert today, not correct. Whoever gives
 * that stub a real body MUST make it return `void*` (this, matching the
 * original x86 ABI's EAX) — otherwise this file's chained "button"
 * directive silently reads an unset register. */
extern void WNDPROC_CriticalSectionLock(int* stream, char* buf);
void*  WNDPROC_StreamReadLine(void* stream, void* outBuf);
void*  WNDPROC_StreamPrintf(void* stream, void* outBuf);
void*  WNDPROC_StreamWrite(void* stream, void* outBuf);

#ifdef _WIN32
extern void*  __thiscall UIPANEL_CreateSurface(void* panel);                    /* 0x42A110 */
extern uint8_t __thiscall UIPANEL_StretchBlit(void* surface, LPCSTR filePath,
                                               uint32_t param2, int32_t param3,
                                               int32_t param4);                  /* 0x42AB10 */
extern size_t UIPANEL_Surface_Size();  /* graphics/LOCOBITMAP.cpp — real sizeof(UIPANEL_Surface) */
#endif

class ResourceManager;
extern ResourceManager g_resmgr;    /* 0x4855E8 — object, not a pointer (was void*,
                                      * a widespread cross-TU landmine — see
                                      * PROGRESS.md's g_resmgr sweep) */
extern void* g_netman;      /* 0x4FD3AC — NetMan singleton (raw-offset view; see
                                game/ScriptedObject.cpp for the same
                                g_netman[0x17].scenarioId idiom) */
extern uint32_t g_game_time; /* 0x4A99B4 */

#ifdef _WIN32
extern void* g_asset_mgr;   /* 0x485600 — Asset manager singleton */
/* Only referenced from the faithful x86-offset bodies below; neither is
 * implemented anywhere else in this codebase yet (a separate, pre-
 * existing gap — not introduced by this file), so these are declaration-
 * only. The MinGW typecheck build compiles _WIN32 code but does not link
 * it, so this is sufficient for that build's purpose. */
extern "C" {
void __thiscall INPUT_EditScrollHandler(void* obj, uint32_t resId);
void __thiscall ResourceManager_AnimateClock(void* mgr, uint32_t gameTime);
}
extern void* DAT_004a99b0;
#endif

namespace {

#ifdef _WIN32
/* Call vtable slot 0 (scalar deleting destructor convention: flags=1
 * frees no memory, just releases sub-resources) on a sub-object whose
 * concrete type is not known here. Matches the original's
 * `(**(code**)*obj)(1)` idiom exactly. */
void ReleaseSubObject(void* obj)
{
    void** const vtbl = *reinterpret_cast<void***>(obj);
    using ScalarDtor = void (*)(void*, int32_t);
    reinterpret_cast<ScalarDtor>(vtbl[0])(obj, 1);
}
#endif

/* Directive-keyword string literals used only by Render() below — read
 * directly from the binary's .rdata this session via Ghidra's read_bytes,
 * not guessed. Two contain a literal '/', not '_': "cursor/default_frame_set"
 * and "must/cant_have" — confirmed via raw byte dump, not a typo. */
const char s_button[]                   = "button";                    /* 0x47E870 */
const char s_Name[]                     = "Name";                      /* 0x47E73C */
const char s_hotspot[]                  = "hotspot";                   /* 0x47E868 */
const char s_ShadowId[]                 = "ShadowId";                  /* 0x47E85C */
const char s_ShadowOffset[]             = "ShadowOffset";              /* 0x47E84C */
const char s_animation[]                = "animation";                 /* 0x47E840 */
const char s_semi_transparent[]         = "semi-transparent";          /* 0x47E82C */
const char s_shadows[]                  = "shadows";                   /* 0x47E824 */
const char s_must_cant_have[]           = "must/cant_have";            /* 0x47E814 */
const char s_MaxInstances[]             = "MaxInstances";              /* 0x47E804 */
const char s_total_number_of_frames[]   = "total_number_of_frames";    /* 0x47E7EC */
const char s_number_of_frame_sets[]     = "number_of_frame_sets";      /* 0x47E7D4 */
const char s_cursor_frame_set[]         = "cursor_frame_set";          /* 0x47E7C0 */
const char s_cursor_default_frame_set[] = "cursor/default_frame_set";  /* 0x47E7A4 */
/* Section-terminator sentinel string ("-9"), same DAT_0047e3cc address and
 * role as game/TrainStation.cpp's s_terminator. */
const char s_terminator[] = "-9";  /* 0x47E3CC */
/* Literal 2-character suffix Render()'s tail overwrites the composed
 * bitmap path's last 2 characters with — traced via raw disassembly
 * (0x425520-0x425542), not guessed; exact semantic purpose beyond "a
 * fixed suffix the original replaces the tail of bmpPath with" is not
 * otherwise evidenced. */
const char s_ut_suffix[] = "ut";  /* 0x47E7A0 */

/* Stream-state bit test used by Render()'s main loop — same raw
 * `*(byte*)(*(int*)(*stream+4) + 8 + (int)stream)` idiom already used by
 * game/TrainStation.cpp's trainstation_stream_flags() and
 * input/BuildingDescriptorEditor.cpp's dat_stream_state_ok(): an internal
 * WNDPROC_Stream-hierarchy detail not yet reconstructed with a typed
 * accessor (see resources/WndProcStreamBuf.h). This function tests both
 * bit 0x1 ("stream ended") and bit 0x4 ("stream error"), like
 * TrainStation's version, not BuildingDescriptorEditor's single-bit 0x7
 * check — each preserved verbatim from its own disassembly rather than
 * assumed to match.
 * TODO: replace with a typed accessor once that reconstruction extends to
 * this slot. */
uint8_t childwindow_stream_flags(void* stream)
{
    int32_t vtable = *reinterpret_cast<int32_t*>(stream);
    int32_t slot1  = *reinterpret_cast<int32_t*>(static_cast<intptr_t>(vtable) + 4);
    return *reinterpret_cast<uint8_t*>(
        static_cast<intptr_t>(slot1) + 8 + reinterpret_cast<intptr_t>(stream));
}

/* Case-fold 3-way keyword compare used by Render()'s directive loop below.
 * Thin wrapper around the real CRT_wcsstr (0x471480 == _stricmp; see the
 * declaration's doc comment above) — its verified real signature takes raw
 * `uint8_t*` (non-const) byte pointers rather than the `const char*` this
 * file's own line buffer/keyword literals naturally are, so the pointer
 * cast is centralized here instead of being repeated at all ~16 call
 * sites below. Returns 0 for "line equals keyword, case-insensitively",
 * nonzero otherwise — the exact same real-function contract, just
 * correctly typed/linked; no directional change from the original
 * `== nullptr` / `!= nullptr` idiom this file used before the fix. */
int ChildWindow_KeywordCompare(const char* line, const char* keyword)
{
    // ABI_BOUNDARY: CRT_wcsstr (0x471480) is a raw CRT byte-string compare
    // entry point matching the original MSVC CRT's non-const uint8_t*
    // prototype; the real implementation only ever reads through both
    // pointers, never writes.
    return static_cast<int>(CRT_wcsstr(
        reinterpret_cast<uint8_t*>(const_cast<char*>(line)),
        reinterpret_cast<uint8_t*>(const_cast<char*>(keyword))));
}

} // namespace

/* Returns sizeof(ChildWindow) on this host (0x180 bytes here vs. the
 * original x86's 0x168 — pointer-bearing fields widen from 4 to 8 bytes).
 * Exists so callers that only need to size an allocation for
 * UI_CreateChildWindow/InitFields can get the real size without
 * `#include`-ing this header. */
size_t ChildWindow_Size()
{
    return sizeof(ChildWindow);
}

/* ================================================================== */
/* ChildWindow::ChildWindow (Constructor)                             */
/* Address: 0x424AF0 (wrapper) + 0x424BF0 (init body)                  */
/* ================================================================== */
ChildWindow::ChildWindow(uint32_t resourceId, int32_t nameParam)
{
    /* Delegate to InitFields to populate all member variables */
    InitFields(resourceId, nameParam);
}

/* ================================================================== */
/* ChildWindow::InitFields (Factored init body)                       */
/* Address: 0x424BF0 (UI_ChildWindow_Create body)                     */
/* ================================================================== */
void ChildWindow::InitFields(uint32_t resourceId, int32_t nameParam)
{
    /* Common field initialization (both _WIN32 and host) */
    this->resourceId = resourceId;
    this->resourceType = static_cast<uint8_t>(GetResourceType(resourceId));
    this->shadowId = 0;
    this->renderSurface = nullptr;
    this->field_14 = 0;
    this->field_16 = 0;
    this->sticky = 0;
    this->frameSetCount = 0;
    this->cursorFrameSetIndex = 0;
    this->defaultFrameSetIndex = 0;
    this->heapBuffer = nullptr;
    this->bitmapSurface = nullptr;
    this->field_28 = 0;
    this->field_2A = 0;
    this->frameCount = 0;
    this->buttonParam1 = 0;
    this->buttonParam2 = 0;
    this->hotspotX = 0;
    this->hotspotY = 0;
    this->shadowOffsetX = 0;
    this->shadowOffsetY = 0;
    this->depResourceId1 = -1;
    this->depResourceId2 = -1;
    this->name[0] = 0;
    this->field_157 = 0;
    this->overlayRefCount = 0;
    this->maxInstances = -1;
    this->totalFrameCount = 1;
    this->loaded = 0;
    this->ready = 1;
    this->animFlags = 0;

    /* Conditional resource loading (nameParam != 0 path).
     * This branch is not exercised by any current caller in the codebase;
     * both CursorEditWindow and TrainStation pass nameParam=0 and handle
     * their own resource loading separately. The branch is deferred pending
     * recovery of CRT_sprintf_buf's vararg signature. */
    if (nameParam != 0) {
        std::fprintf(stderr,
            "STUB: ChildWindow InitFields (0x424BF0) nameParam!=0 resource-"
            "loading branch reached — not yet ported (see PROGRESS.md).\n");
        assert(false &&
               "ChildWindow InitFields: nameParam!=0 branch not yet ported");
    }
}

/* ================================================================== */
/* ChildWindow::~ChildWindow (Destructor)                             */
/* Address: 0x424BA0 (UI_ChildWindow_Dtor — real cleanup body)         */
/* ================================================================== */
ChildWindow::~ChildWindow()
{
    /* Clear the loaded flag — non-vtable operation, works on all platforms */
    loaded = 0;

#ifdef _WIN32
    /* Release renderSurface sub-object (if present).
     * Uses the scalar-deleting-destructor convention (vtable[0] with flags=1),
     * which is Windows x86 ABI specific. */
    if (renderSurface != nullptr) {
        ReleaseSubObject(renderSurface);
        renderSurface = nullptr;
    }

    /* Free heapBuffer (if present) — works on all platforms */
    if (heapBuffer != nullptr) {
        GLOBAL_free(heapBuffer);
        heapBuffer = nullptr;
    }

    /* Release bitmapSurface sub-object (if present) — Windows x86 ABI specific */
    if (bitmapSurface != nullptr) {
        ReleaseSubObject(bitmapSurface);
        bitmapSurface = nullptr;
    }
#else
    /* Host-path: The sub-object releases require the scalar-deleting-
     * destructor ABI (calling vtable[0] with flags=1), which is a Windows
     * x86 detail. On the host build, these objects are never created
     * (OnMouseMove is a no-op), so the releases are not reachable; free
     * heapBuffer as a safety measure. */
    if (heapBuffer != nullptr) {
        GLOBAL_free(heapBuffer);
        heapBuffer = nullptr;
    }
#endif
}

/* ================================================================== */
/* ChildWindow::OnMouseMove (Render handler)                          */
/* Address: 0x425670 (UI_PaintWindow)                                 */
/* Vtable slot: [1] +0x04                                              */
/* ================================================================== */
void* ChildWindow::OnMouseMove(int32_t x, int32_t y)
{
#ifdef _WIN32
    if (totalFrameCount == 0) {
        return nullptr;
    }

    if (renderSurface == nullptr) {
        /* 0x20 was the original x86 sizeof(UIPANEL_Surface); use the real
         * host size (see graphics/LOCOBITMAP.h). */
        void* const raw = operator_new(UIPANEL_Surface_Size());
        void* const surface = (raw != nullptr) ? UIPANEL_CreateSurface(raw) : nullptr;
        renderSurface = surface;
        if (surface == nullptr) {
            return nullptr;
        }
        UIPANEL_StretchBlit(surface, reinterpret_cast<LPCSTR>(&bmpPath[0]), 0,
                             static_cast<uint32_t>(x), y);
    }

    int32_t* const surfaceWords = static_cast<int32_t*>(renderSurface);
    if (surfaceWords[6] == 0 && surfaceWords[7] == 0) {
        ReleaseSubObject(renderSurface);
        renderSurface = nullptr;
        return nullptr;
    }

    field_14 = static_cast<int16_t>(
        static_cast<uint32_t>(surfaceWords[2]) /
        static_cast<uint16_t>(totalFrameCount));
    const int16_t surfaceField0C = *reinterpret_cast<const int16_t*>(&surfaceWords[3]);
    overlayRefCount += 1;
    field_16 = surfaceField0C;

    if (ready == 0) {
        INPUT_EditScrollHandler(&DAT_004a99b0, resourceId);
    }

    if (frameSetCount != 0) {
        const uint8_t* const entryTable = static_cast<const uint8_t*>(heapBuffer);
        for (uint16_t i = 0; i < frameSetCount; ++i) {
            const uint16_t stringId =
                *reinterpret_cast<const uint16_t*>(entryTable + i * 0x18 + 0x0E);
            const int strRes = ResourceManager_GetStringById(&g_resmgr, stringId);
            if (strRes != 0) {
                RESMGR_LoadSoundResource(strRes);
            }
        }
    }

    if (resourceId == 0x842) {
        ResourceManager_AnimateClock(&g_resmgr, g_game_time);
    }

    return renderSurface;
#else
    (void)x;
    (void)y;
    std::fprintf(stderr,
        "TODO: ChildWindow::OnMouseMove (0x425670) on host build — requires "
        "Windows UIPANEL rendering API (see PROGRESS.md).\n");
    return nullptr;
#endif
}

/* ================================================================== */
/* ChildWindow::OnMouseLeave                                           */
/* Address: 0x4257F0 (UI_OnMouseLeave)                                */
/* Vtable slot: [2] +0x08                                              */
/* ================================================================== */
void ChildWindow::OnMouseLeave()
{
    if (overlayRefCount != 0) {
        overlayRefCount -= 1;
    }

#ifdef _WIN32
    const bool isStickyWindow = (sticky == 1);
    if (overlayRefCount == 0 && renderSurface != nullptr && !isStickyWindow) {
        ReleaseSubObject(renderSurface);
        renderSurface = nullptr;

        if (frameSetCount != 0) {
            const uint8_t* const entryTable = static_cast<const uint8_t*>(heapBuffer);
            for (uint16_t i = 0; i < frameSetCount; ++i) {
                const uint16_t stringId =
                    *reinterpret_cast<const uint16_t*>(entryTable + i * 0x18 + 0x0E);
                const int strRes = ResourceManager_GetStringById(&g_resmgr, stringId);
                if (strRes != 0) {
                    RESMGR_ReleaseSoundResource(strRes);
                }
            }
        }
    }
#else
    /* Host-path: The renderSurface is never created (OnMouseMove is a no-op),
     * so the release and sound-cleanup branches are unreachable. */
#endif
}

/* ================================================================== */
/* ChildWindow::Render (Stream parsing handler)                       */
/* Address: 0x424E00 (UI_ChildWindow_Render)                          */
/* Vtable slot: [3] +0x0C                                              */
/* ================================================================== */
uint8_t ChildWindow::Render(void* stream)
{
    /* SEH prologue/epilogue (compiler-managed) not transcribed. */
    char lineBuf[264];

    /* Reads a raw line via the lower-level Enter/SeekForward/Leave triple
     * the original uses for this function's own re-reads (distinct from
     * WNDPROC_CriticalSectionLock, used only for the main directive-line
     * reads below) — matches input/BuildingDescriptorEditor.cpp's
     * identical established idiom for the same call shape. */
    const auto readLineRaw = [&]() {
        WNDPROC_EnterCriticalSection(stream);
        WNDPROC_StreamSeekForward(stream, lineBuf, 0x104, 10);
        WNDPROC_LeaveCriticalSection(stream);
    };

    uint8_t result = 1;

    WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);

    /* Main directive loop: continues while the current line is NOT the
     * terminator ("-9" — CRT_wcsstr's inverted-match convention, already
     * documented in input/BuildingDescriptorEditor.cpp and
     * game/TrainStation.cpp: a MATCH returns 0) and the stream's own
     * "ended" bit (0x1) is not set. */
    while (ChildWindow_KeywordCompare(lineBuf, s_terminator) != 0 &&
           (childwindow_stream_flags(stream) & 0x1) == 0) {
        if (ChildWindow_KeywordCompare(lineBuf, s_button) == 0) {
            void* const s1 = WNDPROC_StreamReadLine(stream, &buttonParam1);
            void* const s2 = WNDPROC_StreamReadLine(s1, &buttonParam2);
            WNDPROC_StreamPrintf(s2, &frameCount);
            if (frameCount == 0) {
                frameCount = 3;
            }
        } else if (ChildWindow_KeywordCompare(lineBuf, s_Name) == 0) {
            readLineRaw();
            /* Source is lineBuf+1, not lineBuf+0 — matches the original's
             * `_strncpy(this+0x14D, local_21b, 10)`, where local_21b is
             * exactly 1 byte into the same 264-byte line buffer Ghidra
             * split as local_21c(1)+local_21b(260)+acStack_117(3). */
            std::strncpy(name, &lineBuf[1], sizeof(name));
            field_157 = 0;
            /* Trim up to 2 trailing \r/\n characters (handles \r\n, \n\r,
             * or a lone \r or \n). Equivalent to the original's goto-based
             * double check-then-trim of the last character, traced
             * exhaustively: the original always trims exactly 0, 1, or 2
             * trailing characters, re-checking the (possibly already
             * trimmed) last character up to twice. */
            std::size_t len = std::strlen(name);
            if (len > 0 && (name[len - 1] == '\r' || name[len - 1] == '\n')) {
                name[len - 1] = '\0';
                len = std::strlen(name);
                if (len > 0 && (name[len - 1] == '\r' || name[len - 1] == '\n')) {
                    name[len - 1] = '\0';
                }
            }
        } else if (ChildWindow_KeywordCompare(lineBuf, s_hotspot) == 0) {
            WNDPROC_StreamReadLine(stream, &hotspotX);
            WNDPROC_StreamReadLine(stream, &hotspotY);
        } else if (ChildWindow_KeywordCompare(lineBuf, s_ShadowId) == 0) {
            WNDPROC_StreamWrite(stream, &shadowId);
        } else if (ChildWindow_KeywordCompare(lineBuf, s_ShadowOffset) == 0) {
            WNDPROC_StreamWrite(stream, &shadowOffsetX);
            WNDPROC_StreamWrite(stream, &shadowOffsetY);
        } else if (ChildWindow_KeywordCompare(lineBuf, s_animation) != 0) {
            /* NOTE: inverted polarity vs every other keyword check in this
             * function — preserved verbatim from disassembly. "animation"
             * itself is a recognized-but-otherwise-inert section marker;
             * every keyword below is only checked on lines that are NOT
             * literally "animation" (i.e. this whole nested cascade is
             * skipped for a bare "animation" line). */
            if (ChildWindow_KeywordCompare(lineBuf, s_semi_transparent) == 0) {
                animFlags |= 0x400;
            } else if (ChildWindow_KeywordCompare(lineBuf, s_shadows) == 0) {
                animFlags |= 2;
            } else if (ChildWindow_KeywordCompare(lineBuf, s_must_cant_have) == 0) {
                void* const s1 = WNDPROC_StreamWrite(stream, &depResourceId1);
                WNDPROC_StreamWrite(s1, &depResourceId2);
            } else if (ChildWindow_KeywordCompare(lineBuf, s_MaxInstances) == 0) {
                CRT_fabs(stream, &maxInstances);
            } else if (ChildWindow_KeywordCompare(lineBuf, s_total_number_of_frames) == 0) {
                WNDPROC_StreamPrintf(stream, &totalFrameCount);
                if (totalFrameCount == 0) {
                    totalFrameCount = 1;
                }
            } else if (ChildWindow_KeywordCompare(lineBuf, s_number_of_frame_sets) == 0) {
                WNDPROC_StreamPrintf(stream, &frameSetCount);
                if (frameSetCount != 0) {
                    void* const alloc = operator_new(static_cast<size_t>(frameSetCount) * 0x18);
                    heapBuffer = alloc;
                    if (alloc == nullptr) {
                        /* Matches the original exactly: allocation failure
                         * is a clean, immediate return (not a fallthrough
                         * into the rest of the loop). */
                        return result;
                    }
                }
            } else {
                const bool matchesCursorFrameSet =
                    ChildWindow_KeywordCompare(lineBuf, s_cursor_frame_set) == 0;
                const bool matchesDefaultFrameSet =
                    ChildWindow_KeywordCompare(lineBuf, s_cursor_default_frame_set) == 0;
                if (!matchesCursorFrameSet && !matchesDefaultFrameSet) {
                    /* Neither "cursor_frame_set" nor
                     * "cursor/default_frame_set" matched — this line is
                     * the section terminator; stop the whole directive
                     * loop (not just this branch). */
                    break;
                }

                void* const s1 = WNDPROC_StreamReadLine(stream, &cursorFrameSetIndex);
                WNDPROC_StreamReadLine(s1, &defaultFrameSetIndex);

                if (cursorFrameSetIndex != -1 &&
                    static_cast<int>(frameSetCount) <= static_cast<int>(cursorFrameSetIndex)) {
                    result = 0;
                }
                if (defaultFrameSetIndex != -1 &&
                    static_cast<int>(frameSetCount) <= static_cast<int>(defaultFrameSetIndex)) {
                    result = 0;
                }

                /* Populate every heapBuffer entry (frameSetCount entries,
                 * 0x18 bytes each — same entry table OnMouseMove/
                 * OnMouseLeave already read entry+0x0E "stringId" from). */
                uint8_t* const entryTable = static_cast<uint8_t*>(heapBuffer);
                for (uint16_t i = 0; i < frameSetCount; ++i) {
                    uint8_t* const entry = entryTable + i * 0x18;

                    std::memset(entry, 0, 0x18);

                    WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
                    WNDPROC_StreamPrintf(stream, entry + 0x00);
                    WNDPROC_StreamPrintf(stream, entry + 0x02);
                    WNDPROC_StreamPrintf(stream, entry + 0x04);
                    int16_t temp = 0;
                    WNDPROC_StreamPrintf(stream, &temp);
                    entry[0x17] = static_cast<uint8_t>(temp);
                    WNDPROC_StreamWrite(stream, entry + 0x08);
                    WNDPROC_StreamReadLine(stream, entry + 0x0C);
                    WNDPROC_StreamReadLine(stream, entry + 0x0E);
                    WNDPROC_StreamWrite(stream, entry + 0x10);
                    WNDPROC_StreamPrintf(stream, entry + 0x14);
                    WNDPROC_StreamPrintf(stream, &temp);
                    entry[0x16] = static_cast<uint8_t>(temp);

                    int16_t* const entry04 = reinterpret_cast<int16_t*>(entry + 0x04);
                    if (*entry04 == 0) {
                        *entry04 = 1;
                    }

                    const int16_t entry00 = *reinterpret_cast<int16_t*>(entry + 0x00);
                    const int16_t entry02 = *reinterpret_cast<int16_t*>(entry + 0x02);
                    int16_t* const entry0C = reinterpret_cast<int16_t*>(entry + 0x0C);
                    /* Self-reference detection: if entry[0]==entry[2] and
                     * entry[0xC] (as a frame-set index) equals this
                     * entry's own position in the array, clear it to
                     * -1/0xffff — a sentinel that also always passes the
                     * bounds check just below (frameSetCount, always
                     * non-negative, is never <= -1). */
                    if (entry00 == entry02 && *entry0C == static_cast<int16_t>(i)) {
                        *entry0C = -1;
                    }

                    if (static_cast<uint16_t>(totalFrameCount) <= static_cast<uint16_t>(entry00) ||
                        static_cast<uint16_t>(totalFrameCount) <= static_cast<uint16_t>(entry02)) {
                        result = 0;
                    }
                    if (static_cast<int>(frameSetCount) <= static_cast<int>(*entry0C)) {
                        result = 0;
                    }
                }
            }
        }
        /* No matching keyword: fall through and read the next line —
         * matches the original (no "unknown keyword" terminator branch
         * here, unlike BuildingDescriptorEditor::Render). */

        WNDPROC_CriticalSectionLock(reinterpret_cast<int*>(stream), lineBuf);
    }

    /* If the final line read is NOT the terminator, the loop exited via
     * the stream-ended bit rather than a genuine terminator match — mark
     * as failure (matches every sibling Render override's identical
     * post-loop check). */
    if (ChildWindow_KeywordCompare(lineBuf, s_terminator) != 0) {
        result = 0;
    }

    /* Skip forward past any leading '/'-prefixed comment/path lines before
     * the final bitmap load — a raw-idiom line read, then two do-while-
     * shaped skip loops (skip until a '/' line, then skip while '/'
     * lines), each bailing out immediately if the stream ends first. */
    readLineRaw();

    bool foundSlashLine = (lineBuf[0] == '/');
    if (!foundSlashLine) {
        for (;;) {
            if ((childwindow_stream_flags(stream) & 0x1) != 0) {
                break;
            }
            readLineRaw();
            if (lineBuf[0] == '/') {
                foundSlashLine = true;
                break;
            }
        }
    }
    if (foundSlashLine) {
        for (;;) {
            if ((childwindow_stream_flags(stream) & 0x1) != 0) {
                break;
            }
            readLineRaw();
            if (lineBuf[0] != '/') {
                break;
            }
        }
    }

    /* Compose a scratch copy of bmpPath with its last 2 characters
     * replaced by "ut", then load+blit the resulting bitmap into
     * bitmapSurface — only when bmpPath holds a non-trivial path
     * (length > 2, matching the original's exact threshold). */
    const std::size_t bmpPathLen = std::strlen(bmpPath);
    if (bmpPathLen > 2) {
        char composedPath[264];
        std::strncpy(composedPath, bmpPath, sizeof(composedPath) - 1);
        composedPath[sizeof(composedPath) - 1] = '\0';
        const std::size_t composedLen = std::strlen(composedPath);
        if (composedLen >= 2) {
            std::strcpy(&composedPath[composedLen - 2], s_ut_suffix);
        }

#ifdef _WIN32
        /* 0x20 was the original x86 sizeof(UIPANEL_Surface); use the real
         * host size (see graphics/LOCOBITMAP.h). */
        void* const raw = operator_new(UIPANEL_Surface_Size());
        bitmapSurface = (raw != nullptr) ? UIPANEL_CreateSurface(raw) : nullptr;
        if (bitmapSurface != nullptr) {
            UIPANEL_StretchBlit(bitmapSurface, composedPath, 0, 0, 0);
            int32_t* const surfaceWords = static_cast<int32_t*>(bitmapSurface);
            if (surfaceWords[6] == 0 && surfaceWords[7] == 0) {
                ReleaseSubObject(bitmapSurface);
                bitmapSurface = nullptr;
            }
        }
        if (bitmapSurface != nullptr && frameCount != 0) {
            const int32_t* const surfaceWords = static_cast<const int32_t*>(bitmapSurface);
            field_28 = static_cast<int16_t>(
                static_cast<uint32_t>(surfaceWords[2]) / static_cast<uint16_t>(frameCount));
            field_2A = *reinterpret_cast<const int16_t*>(&surfaceWords[3]);
        }
#else
        (void)composedPath;
        std::fprintf(stderr,
            "TODO: ChildWindow::Render (0x424E00) bitmap-surface load on "
            "host build — requires Windows UIPANEL rendering API (see "
            "PROGRESS.md, matches ChildWindow::OnMouseMove's identical "
            "gap).\n");
#endif
    }

    return result;
}

/* ================================================================== */
/* ChildWindow::IsBitmapReady (Non-virtual member)                     */
/* Address: 0x4255F0 (UI_IsBitmapReady)                               */
/* ================================================================== */
bool ChildWindow::IsBitmapReady() const
{
    /* This logic is the same on both _WIN32 and host paths, as it uses only
     * named fields and ResourceManager calls. */
    if (ready == 0) {
        return false;
    }
    if (bitmapSurface == nullptr) {
        return false;
    }
    if (frameCount == 0) {
        return false;
    }

    /* NOTE: ResourceManager_GetById is called unconditionally for dep1,
     * even when depResourceId1 == -1. The assembly at 0x42560B..0x425614
     * calls it always, then checks the ID afterward (CMP + JZ at 0x425619).
     * If ResourceManager_GetById has side effects, they must occur. */
    void* const res1 = ResourceManager_GetById(&g_resmgr, depResourceId1);
    if (depResourceId1 != -1) {
        const bool dep1Ready = res1 != nullptr &&
            *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(res1) + 0x158) != 0;
        if (!dep1Ready) {
            return false;
        }
    }

    void* const res2 = ResourceManager_GetById(&g_resmgr, depResourceId2);
    const bool dep2Ready = res2 != nullptr &&
        *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(res2) + 0x158) != 0;
    /* NOTE: Assembly at 0x425647 uses JA (unsigned >), so when dep2Ready is
     * true (res2[0x158] > 0), the function returns false (not ready).
     * This is the original behavior per disassembly 0x4255F0. */
    if (dep2Ready) {
        return false;
    }

    /* Scenario-mode special case for resource 0xC42 (matches the same
     * g_netman[0x17].scenarioId idiom already used in
     * game/ScriptedObject.cpp). */
    if (resourceId == 0xC42) {
        const int32_t scenarioId =
            *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(g_netman) + 0x7C4);
        if (scenarioId == 2) {
            return false;
        }
    }

    return true;
}

/* ================================================================== */
/* Compatibility Shims (extern "C")                                    */
/* ================================================================== */

extern "C" {

/**
 * UI_CreateChildWindow — ChildWindow "constructor" shim.
 * Address: 0x424AF0
 *
 * Forwards to ChildWindow's InitFields method after casting void*
 * to ChildWindow*. Kept for compatibility with existing callers
 * (CursorEditWindow, TrainStation_Ctor) that may not be converted to C++
 * in this batch. These shims can be removed as each derived class's
 * callers migrate to direct C++ constructor calls.
 */
void* UI_CreateChildWindow(void* self, uint32_t resourceId, int32_t nameParam)
{
#ifdef _WIN32
    /* Cast the pre-allocated derived-object pointer and call InitFields
     * to populate base-class fields. Mirrors the assembly behavior of
     * 0x424AF0: zeroes fields, stages the vtable, and delegates to 0x424BF0. */
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    obj->InitFields(resourceId, nameParam);
    return self;
#else
    (void)resourceId;
    (void)nameParam;
    std::fprintf(stderr,
        "STUB: UI_CreateChildWindow (0x424AF0) reached on host build — "
        "the ChildWindow cluster is not yet ported to a non-Windows "
        "receiver type (see PROGRESS.md).\n");
    assert(false && "UI_CreateChildWindow: host implementation not yet ported");
    return self;
#endif
}

/**
 * UI_ChildWindow_Create — Init-body shim.
 * Address: 0x424BF0
 *
 * This was the init-body helper called from UI_CreateChildWindow.
 * In the C++ class, the logic is in InitFields(). This shim is kept
 * for reference; no external caller invokes it directly in current code.
 */
void UI_ChildWindow_Create(void* self, uint32_t resourceId, int32_t nameParam)
{
#ifdef _WIN32
    /* If called directly (which shouldn't happen), delegate to InitFields */
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    obj->InitFields(resourceId, nameParam);
#else
    (void)self;
    (void)resourceId;
    (void)nameParam;
    std::fprintf(stderr,
        "STUB: UI_ChildWindow_Create (0x424BF0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_ChildWindow_Create: host implementation not yet ported");
#endif
}

/**
 * UI_ChildWindow_Dtor — Destructor shim.
 * Address: 0x424BA0
 *
 * Forwards to ChildWindow destructor after casting. Qualified to call
 * the base implementation directly (not virtual dispatch), which prevents
 * infinite recursion if called from a derived-class override's chain.
 */
void UI_ChildWindow_Dtor(void* self)
{
#ifdef _WIN32
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified destructor call forces base implementation, matching the
     * assembly's direct call to 0x424BA0. */
    obj->ChildWindow::~ChildWindow();
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_ChildWindow_Dtor (0x424BA0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_ChildWindow_Dtor: host implementation not yet ported");
#endif
}

/**
 * UI_ChildWindow_Render — Render method shim (stub).
 * Address: 0x424E00
 *
 * Qualified to call the base implementation directly (not virtual dispatch).
 */
uint8_t UI_ChildWindow_Render(void* self, void* stream)
{
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified call forces base implementation, matching the assembly. */
    return obj->ChildWindow::Render(stream);
}

/**
 * UI_IsBitmapReady — IsBitmapReady method shim.
 * Address: 0x4255F0
 *
 * NOTE: Original signature takes int32_t (truncated pointer); shim handles
 * the conversion back to ChildWindow* for the member call. On the host build,
 * real callers (Town::handle_tile_click, RESDATA_ScriptedObject::Start) pass
 * unrelated bridge objects, not ChildWindow instances, so this shim's host
 * path is a loud stub.
 */
int32_t UI_IsBitmapReady(int32_t self)
{
#ifdef _WIN32
    const uint8_t* const p =
        reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(self)));
    const ChildWindow* const obj = reinterpret_cast<const ChildWindow*>(p);
    return obj->IsBitmapReady() ? 1 : 0;
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_IsBitmapReady (0x4255F0) reached on host build — its "
        "real call sites (Town::handle_tile_click, "
        "RESDATA_ScriptedObject::Start) pass a resource_manager_sdl3.cpp "
        "bridge object, not a ChildWindow-shaped receiver, so an offset-"
        "based body would read unrelated bytes (see PROGRESS.md).\n");
    assert(false && "UI_IsBitmapReady: host implementation not yet ported");
    return 0;
#endif
}

/**
 * UI_PaintWindow — OnMouseMove method shim.
 * Address: 0x425670
 *
 * Qualified to call the base implementation directly (not virtual dispatch).
 */
void* UI_PaintWindow(void* self, int32_t param1, int32_t param2)
{
#ifdef _WIN32
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified call forces base implementation. */
    return obj->ChildWindow::OnMouseMove(param1, param2);
#else
    (void)param1;
    (void)param2;
    std::fprintf(stderr,
        "STUB: UI_PaintWindow (0x425670) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_PaintWindow: host implementation not yet ported");
    return nullptr;
#endif
}

/**
 * UI_OnMouseLeave — OnMouseLeave method shim.
 * Address: 0x4257F0
 *
 * Qualified to call the base implementation directly (not virtual dispatch).
 */
void UI_OnMouseLeave(void* self)
{
#ifdef _WIN32
    ChildWindow* const obj = reinterpret_cast<ChildWindow*>(self);
    /* Qualified call forces base implementation, matching the assembly's
     * direct call to 0x4257F0. */
    obj->ChildWindow::OnMouseLeave();
#else
    (void)self;
    std::fprintf(stderr,
        "STUB: UI_OnMouseLeave (0x4257F0) reached on host build "
        "(see PROGRESS.md).\n");
    assert(false && "UI_OnMouseLeave: host implementation not yet ported");
#endif
}

} // extern "C"
