/**
 * stubs_link001_batch2_streams.cpp — LINK-001 sweep, batch 2:
 * Win32/WNDPROC stream I/O free-function family (resources/Win32Stream*,
 * WndProcStream*, WndProcStreamBuf* callers).
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 *
 * =====================================================================
 * SCOPE NOTE FOR THE INTEGRATOR — read before merging
 * =====================================================================
 * This file was written against THIS WORKTREE's HEAD (commit 69f7556).
 * The batch brief that produced this file's symbol list assumed a newer,
 * uncommitted state of the main working tree that adds a real
 * resources/Win32Stream.h/.cpp (a WIN32_Stream/WIN32_StreamFile-backed
 * facade with real OpenPath/Read/CloseNow) and extends
 * resources/WndProcStream.{h,cpp} with a real WNDPROC_Stream::Read()/
 * AttachBuffer(). NEITHER EXISTS at this worktree's HEAD:
 * resources/Win32Stream.{h,cpp} are simply absent here, and this
 * worktree's WNDPROC_Stream is the earlier, partial reconstruction
 * (InputPrefix/ExtractToken/Flush/SkipWhitespace only — no Read(), no
 * AttachBuffer(), no gcount_-adjacent _reserved_04). Every definition
 * below is therefore a genuine "no real implementation reachable from
 * this commit" stub, verified against this worktree's actual files —
 * NOT a forwarding shim to real logic. When the newer Win32Stream work
 * lands in this tree, the entries marked "REPLACE ONCE MERGED" below
 * should be deleted from this file (not kept alongside the real ones)
 * in favor of forwarding to WIN32_Stream::OpenPath / WNDPROC_Stream::Read.
 *
 * Also note: several symbols named in the original batch dossier turned
 * out to be ALREADY RESOLVED at this commit (defined as no-ops in
 * shared/defsym_stubs.cpp or shared/link_stubs.cpp, which this file must
 * not touch or duplicate) — they are deliberately NOT redefined here.
 * See the per-symbol report delivered alongside this file for the full
 * already-resolved list and citations.
 *
 * Every signature below was derived by reading each real caller's own
 * forward declaration in this worktree (not assumed from the dossier),
 * and checking whether that declaration sits inside an `extern "C" { }`
 * block (unmangled linkage — binds by bare name only, ignoring parameter
 * types) or not (ordinary C++-mangled linkage, where parameter types are
 * part of the link-time identity).
 */

#include <cassert>
#include <cstdint>
#include <cstdio>

/* Opaque forward declaration only — its members are never accessed here.
 * native/wave_io.c (compiled as C++ via `-x c++`, see meson.build's
 * common_c_args) declares its OWN file-local `struct WNDPROC_Stream { ... }`
 * of the same name for its extern declarations below. Itanium C++ mangling
 * encodes a class-type parameter by its name, not by cross-TU structural
 * identity, so a same-named forward declaration here produces the exact
 * mangled symbols those extern declarations need, without pulling in
 * resources/WndProcStream.h or assuming any object layout (CLAUDE.md
 * forbids guessing at raw layouts without evidence — these pointers are
 * treated as fully opaque). */
class WNDPROC_Stream;

/* ================================================================== */
/* 1. WIN32_StreamOpenPath(void*, const char*, int, int)                */
/*    0x463AA0 (WIN32_Stream::OpenPath's original address).             */
/*    Callers (verified in this worktree, NOT inside extern "C", so      */
/*    C++-mangled): game/ScriptedObject.cpp (HandleEvent); ui/           */
/*    UIPANEL_Surface.cpp (UIPANEL_StretchBlit); ui/AboutDialog.cpp      */
/*    (extra unlisted caller with the identical shape — harmless to      */
/*    also satisfy).                                                     */
/*                                                                        */
/*    Every verified real caller passes a raw, never-placement-          */
/*    constructed stack buffer as `stream` — ScriptedObject::HandleEvent's*/
/*    `int stream_handle[2]` is only 8 bytes, nowhere near large enough   */
/*    for a real stream object even if one existed at this commit — so    */
/*    this cannot safely attempt real construction/open logic without     */
/*    risking a stack overflow, independent of whether Win32Stream.h      */
/*    exists. All verified callers discard the return value. REPLACE      */
/*    ONCE MERGED: once a real WIN32_Stream exists AND the raw-buffer     */
/*    callers are fixed to pass a real heap-allocated object (a separate, */
/*    already-tracked caller-unification pass — see WndProcStream.cpp's   */
/*    WNDPROC_CriticalSectionLock postmortem for the identical problem     */
/*    shape), this should forward to WIN32_Stream::OpenPath.              */
/* ================================================================== */
int WIN32_StreamOpenPath(void* stream, const char* path, int flags, int unk);
int WIN32_StreamOpenPath(void* stream, const char* path, int flags, int unk)
{
    (void)stream;
    (void)unk;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WIN32_StreamOpenPath(void*, const char*, int, int) not "
                "implemented (0x463AA0; TODO: forward to WIN32_Stream::OpenPath "
                "once resources/Win32Stream.{h,cpp} exist in this worktree AND "
                "callers stop passing raw undersized stack buffers) — "
                "path='%s' flags=0x%x, stream left unopened\n",
                path ? path : "(null)", flags);
        warned = true;
    }
    return 0;
}

/* ================================================================== */
/* 2. WIN32_StreamOpenPath(void*, const char*, int, const char*)        */
/*    0x463AA0. Caller (verified): native/wave_io.c's Game_LoadWaveFile  */
/*    (a function-local `extern` declaration, not wrapped in             */
/*    extern "C" — wave_io.c compiles as C++ but this declaration isn't   */
/*    inside a C-linkage block, so it mangles).                          */
/*                                                                        */
/*    The 4th parameter is really the numeric DAT_00479190 share-mode     */
/*    value smuggled through a pointer-sized slot (see wave_io.c's own    */
/*    comment at its call site) — never dereferenced here, only ever      */
/*    cast back to an integer by the caller. `stream` there is heap-      */
/*    allocated via operator_new(0x5C) but, at this commit, never          */
/*    placement-constructed into any real object either (WIN32_StreamOpen */
/*    (void*,int32_t) already resolves to shared/link_stubs.cpp's no-op   */
/*    returning nullptr without touching the buffer) — so this stub        */
/*    matches that existing no-op behavior rather than half-opening a      */
/*    stream the rest of the chain still treats as closed.                */
/* ================================================================== */
int WIN32_StreamOpenPath(void* stream, const char* path, int flags, const char* mode);
int WIN32_StreamOpenPath(void* stream, const char* path, int flags, const char* mode)
{
    (void)stream;
    (void)mode;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WIN32_StreamOpenPath(void*, const char*, int, const char*) "
                "not implemented (0x463AA0; TODO: forward to WIN32_Stream::OpenPath "
                "once it exists in this worktree) — path='%s' flags=0x%x, stream "
                "left unopened\n",
                path ? path : "(null)", flags);
        warned = true;
    }
    return 0;
}

/* ================================================================== */
/* 3. WIN32_StreamOpenFile(void*, char*, int, const char*, int)         */
/*    0x463970 (WIN32_Stream(path,flags,shareMask)'s original address). */
/*    Caller (verified, C++-mangled — not inside any extern "C" block):  */
/*    native/cgwnd_palette.c's CGWND_ValidatePaletteData.                */
/*                                                                        */
/*    Verified SAFE to return nullptr: the caller's own fallback path     */
/*    already null-checks the result (`if (streamObj == NULL)             */
/*    { successFlag = 0; }`) and every subsequent stream use is guarded    */
/*    by `if (streamObj != NULL)` — returning nullptr here reaches an      */
/*    existing, intentional error path, not undefined behavior.           */
/* ================================================================== */
void* WIN32_StreamOpenFile(void* stream, char* path, int mode, const char* flags, int flag2);
void* WIN32_StreamOpenFile(void* stream, char* path, int mode, const char* flags, int flag2)
{
    (void)stream;
    (void)mode;
    (void)flags;
    (void)flag2;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WIN32_StreamOpenFile(void*, char*, int, const char*, int) "
                "not implemented (0x463970; TODO: forward to WIN32_Stream(path,"
                "flags,shareMask) once resources/Win32Stream.{h,cpp} exist) — "
                "path='%s' not opened\n",
                path ? path : "(null)");
        warned = true;
    }
    return nullptr;
}

/* ================================================================== */
/* 4. WIN32_StreamRead(void*, void*, int)                                */
/*    0x463810 (WNDPROC_Stream::Read's original address).               */
/*    Callers (verified, C++-mangled): native/wave_io.c's                */
/*    Game_LoadWaveFile; ui/GameSetupPanel.cpp's loadLayouts.            */
/*    Safe default: report 0 bytes read (short-read/EOF signal), touch   */
/*    neither `stream` nor `buf`.                                        */
/* ================================================================== */
int WIN32_StreamRead(void* stream, void* buf, int size);
int WIN32_StreamRead(void* stream, void* buf, int size)
{
    (void)stream;
    (void)buf;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WIN32_StreamRead(void*, void*, int) not implemented "
                "(0x463810) — requested %d bytes, read 0\n",
                size);
        warned = true;
    }
    return 0;
}

/* ================================================================== */
/* 5. WIN32_StreamRead(WNDPROC_Stream*, void*, int)                      */
/*    0x463810, same underlying function as #4 above, but the caller     */
/*    (native/wave_io.c's Game_ReadChunk) types `stream` as its own       */
/*    file-local WNDPROC_Stream*, producing a distinct mangled symbol.    */
/* ================================================================== */
int WIN32_StreamRead(WNDPROC_Stream* stream, void* buf, int size);
int WIN32_StreamRead(WNDPROC_Stream* stream, void* buf, int size)
{
    (void)stream;
    (void)buf;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WIN32_StreamRead(WNDPROC_Stream*, void*, int) not "
                "implemented (0x463810) — requested %d bytes, read 0\n",
                size);
        warned = true;
    }
    return 0;
}

/* ================================================================== */
/* 6. WNDPROC_StreamReadLine(void*, short*)                              */
/*    0x464BC0. Caller (verified, C++-mangled): native/cgwnd_palette.c's */
/*    CGWND_ValidatePaletteData.                                         */
/*                                                                        */
/*    Distinct from shared/stubs_impl.cpp's existing                     */
/*    WNDPROC_StreamReadLine(void*, void*) — that is a different 2nd      */
/*    parameter type (void* vs short*), hence a different mangled         */
/*    symbol; that file is out of scope here and untouched.               */
/*                                                                        */
/*    Verified unreachable at this commit given the sibling stub choice   */
/*    above: CGWND_ValidatePaletteData's only path to a non-null          */
/*    `streamObj` before calling this is either (a) WNDPROC_StreamFromMemory*/
/*    (shared/defsym_stubs.cpp), which unconditionally assert(0)s before   */
/*    ever returning, or (b) WIN32_StreamOpenFile (#3 above), which this   */
/*    file makes return nullptr. Kept as a safe no-op rather than an       */
/*    assert, since that unreachability is contingent on this file's own   */
/*    sibling choice (#3), not independently guaranteed.                   */
/* ================================================================== */
void WNDPROC_StreamReadLine(void* stream, short* buf);
void WNDPROC_StreamReadLine(void* stream, short* buf)
{
    (void)stream;
    (void)buf;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WNDPROC_StreamReadLine(void*, short*) not implemented "
                "(0x464BC0) — output left unwritten\n");
        warned = true;
    }
}

/* ================================================================== */
/* 7. WNDPROC_StreamSeekForward(void*, void*, int32_t, int) — extern "C" */
/*    0x464C70. Callers (verified, INSIDE extern "C" blocks — unmangled, */
/*    binds by bare name "WNDPROC_StreamSeekForward" only):              */
/*    input/BuildingDescriptorEditor.cpp (Render); ui/UI_ChildWindow.cpp  */
/*    (Render's lambda).                                                 */
/*                                                                        */
/*    Distinct from shared/stubs_impl.cpp's existing C++-mangled          */
/*    `int WNDPROC_StreamSeekForward(void*, int, int, int)` (no            */
/*    extern "C" there) — different linkage, different symbol.            */
/* ================================================================== */
extern "C" void WNDPROC_StreamSeekForward(void* stream, void* buf, int32_t size, int ch);
extern "C" void WNDPROC_StreamSeekForward(void* stream, void* buf, int32_t size, int ch)
{
    (void)stream;
    (void)buf;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WNDPROC_StreamSeekForward(void*, void*, int32_t, int) "
                "extern \"C\" not implemented (0x464C70) — size=%d ch=%d, seek "
                "skipped\n",
                size, ch);
        warned = true;
    }
}

/* ================================================================== */
/* 8. WNDPROC_StreamSeekForward(WNDPROC_Stream*, int, int, int)          */
/*    0x464C70, same underlying function as #7, but native/wave_io.c's   */
/*    Game_ReadChunk types `stream` as its own file-local WNDPROC_Stream*,*/
/*    declared as a function-local `extern` (not extern "C"), producing   */
/*    a distinct C++-mangled symbol from both #7 above and from            */
/*    shared/stubs_impl.cpp's (void*, int, int, int) mangled overload.     */
/* ================================================================== */
int WNDPROC_StreamSeekForward(WNDPROC_Stream* stream, int a, int offset, int b);
int WNDPROC_StreamSeekForward(WNDPROC_Stream* stream, int a, int offset, int b)
{
    (void)stream;
    (void)a;
    (void)b;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: WNDPROC_StreamSeekForward(WNDPROC_Stream*, int, int, int) "
                "not implemented (0x464C70) — offset=%d, seek skipped\n",
                offset);
        warned = true;
    }
    return 0;
}

/* ================================================================== */
/* 9/10. WNDPROC_EnterCriticalSection(void*) / LeaveCriticalSection(void*)*/
/*    0x464D90 / 0x464DA0. Caller (verified, C++-mangled — function-     */
/*    local `extern` declarations inside native/wave_io.c's               */
/*    Game_ReadChunk, NOT wrapped in extern "C"): distinct symbols from    */
/*    the pre-existing extern "C" 0-arg no-ops already in                 */
/*    shared/defsym_stubs.cpp (which satisfy the *unmangled* bare name     */
/*    "WNDPROC_EnterCriticalSection"/"WNDPROC_LeaveCriticalSection" used   */
/*    by resources/StreamObject.cpp, resources/WndProcStream.cpp, and      */
/*    resources/WndProcStreamBuf.cpp — all of which wrap their own          */
/*    declarations in extern "C").                                         */
/*                                                                          */
/*    Real thin IAT forwarders to Win32 Enter/LeaveCriticalSection in the   */
/*    original; this host is single-threaded (no real OS thread ever runs   */
/*    the network/asset-loading code paths that reach Game_ReadChunk — see  */
/*    shared/link_stubs.cpp's WaitForSingleObject/ResumeThread comment for   */
/*    the same "no real second thread" fact used elsewhere in this tree),    */
/*    matching the existing 0-arg host no-op precedent exactly. Silent       */
/*    no-ops, not warn-once stubs — this mirrors the existing sibling        */
/*    declarations, which are also silent.                                   */
/* ================================================================== */
void WNDPROC_EnterCriticalSection(void* cs);
void WNDPROC_EnterCriticalSection(void* cs) { (void)cs; /* host no-op — single-threaded */ }

void WNDPROC_LeaveCriticalSection(void* cs);
void WNDPROC_LeaveCriticalSection(void* cs) { (void)cs; /* host no-op — single-threaded */ }

/* ================================================================== */
/* 11. Stream_BeginEnum(WNDPROC_Stream*)                                 */
/*     Address unknown (no xref/address comment found anywhere in this   */
/*     worktree for this specific function). Caller (verified, C++-      */
/*     mangled, function-local `extern` inside native/wave_io.c's         */
/*     Game_ReadChunk): distinct from shared/stubs_impl.cpp's existing    */
/*     `Stream_BeginEnum(void*)` (different parameter type).              */
/* ================================================================== */
void Stream_BeginEnum(WNDPROC_Stream* stream);
void Stream_BeginEnum(WNDPROC_Stream* stream)
{
    (void)stream;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: Stream_BeginEnum(WNDPROC_Stream*) not implemented "
                "(address unknown — TODO: locate real address) — enumeration "
                "not begun\n");
        warned = true;
    }
}

/* ================================================================== */
/* 12. Stream_BeginRead(WNDPROC_Stream*, int, int)                       */
/*     Address unknown. Caller (verified, C++-mangled, function-local     */
/*     `extern` inside native/wave_io.c's Game_ReadChunk): distinct from   */
/*     shared/stubs_impl.cpp's existing `Stream_BeginRead(void*, int,     */
/*     int)` (different first parameter type).                            */
/* ================================================================== */
void Stream_BeginRead(WNDPROC_Stream* stream, int a, int b);
void Stream_BeginRead(WNDPROC_Stream* stream, int a, int b)
{
    (void)stream;
    (void)a;
    (void)b;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: Stream_BeginRead(WNDPROC_Stream*, int, int) not "
                "implemented (address unknown — TODO: locate real address) — "
                "read not begun\n");
        warned = true;
    }
}

/* ================================================================== */
/* 13. Stream_BeginRead(void*, unsigned int, int)                        */
/*     Address unknown. Caller (verified, C++-mangled — NOT inside any    */
/*     extern "C" block): ui/UIPANEL_Surface.cpp's UIPANEL_StretchBlit.   */
/*     Distinct from shared/stubs_impl.cpp's `Stream_BeginRead(void*,     */
/*     int, int)` (2nd parameter is unsigned int here, which mangles       */
/*     differently from int) and from #12 above (first parameter type).   */
/* ================================================================== */
void Stream_BeginRead(void* stream, unsigned int offset, int mode);
void Stream_BeginRead(void* stream, unsigned int offset, int mode)
{
    (void)stream;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "STUB: Stream_BeginRead(void*, unsigned int, int) not "
                "implemented (address unknown — TODO: locate real address) — "
                "offset=%u mode=%d, read not begun\n",
                offset, mode);
        warned = true;
    }
}

/* 14. stream_vtable_scalar_dtor(int*) — REMOVED. WNDPROC_StreamFromMemory
 *     is now real (resources/Win32StreamMem.h/.cpp) and ui/HelpWnd.cpp's
 *     reset_pages() now calls real `delete streamObj;` (a WNDPROC_Stream*)
 *     instead of this raw vtable-slot stub — zero remaining references
 *     (confirmed via grep). ui/HelpWnd_stubs.cpp's own `static` same-named
 *     function was always dead code (internal linkage, never satisfied
 *     HelpWnd.cpp's extern reference) and remains unreferenced. */
