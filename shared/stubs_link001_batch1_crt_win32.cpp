/* stubs_link001_batch1_crt_win32.cpp — LINK-001 batch 1: CRT + Win32 call-0 landmines
 *
 * Fixes call-0 landmines (undefined symbols silently bound to address 0 by
 * -Wl,--unresolved-symbols=ignore-all) for this batch's assigned CRT and
 * Win32 symbols. Every caller's OWN forward declaration (exact param types,
 * cv-qualifiers, extern "C" or not) was read directly from its source file
 * to determine the real linkage/signature needed — see the per-symbol
 * comments below for the caller and the evidence trail.
 *
 * Per CLAUDE.md's CRITICAL CONSTRAINT for this pass: this is the ONLY file
 * touched. Existing files (shared/link_stubs.cpp, shared/defsym_stubs.cpp,
 * caller .cpp/.h files) are read-only references, never edited here — even
 * when the real fix is "the caller's own declaration is wrong" (flagged
 * per-symbol below with SHOULD_BE_FIXED_AT in the final report instead).
 */

// Status: TRANSCRIBED

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cctype>
#include <ctime>
#include <cassert>
#include <strings.h>   /* POSIX strcasecmp/strncasecmp — host-only, matches this
                        * tree's existing use of other POSIX headers (unistd.h,
                        * pwd.h) in shared/defsym_stubs.cpp for host adapters. */

/* =========================================================== */
/* Local Win32 type shims — same technique as shared/link_stubs.cpp's own
 * top-of-file forward decls: only the struct/typedef *name* matters for
 * Itanium mangling (member layout doesn't), so a minimal local RECT with
 * the right leading-4-int32 layout is safe and link-compatible with every
 * caller's own (larger, real) RECT from shared/types.h. This file does
 * NOT include shared/types.h/stubs/windows_types.h, so these are declared
 * locally exactly like link_stubs.cpp already does. */
/* =========================================================== */
struct RECT { int32_t l, t, r, b; };
typedef void*           HANDLE;
typedef void*           HWND;
typedef void*           HINSTANCE;
typedef void*           HICON;
typedef uint32_t         DWORD;
typedef int32_t          BOOL;
typedef uint32_t         UINT;
typedef const char*      LPCSTR;
/* UINT_PTR is already typedef'd (uint32_t) by stubs/compat.h, force-included
 * ahead of this file by the build — redeclaring it here would be a harmless
 * but needless duplicate, so it is deliberately omitted. */

static inline int32_t rect_min(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t rect_max(int32_t a, int32_t b) { return a > b ? a : b; }

/* =========================================================== */
/* A. extern "C" symbols                                        */
/* =========================================================== */
extern "C" {

/* ----------------------------------------------------------- */
/* CRT_mbstowcs_s(void*, const void*, int) -> BuildingDescriptorEditor::Render
 * Caller decl (input/BuildingDescriptorEditor.cpp:69, inside its extern "C"
 * block): `int CRT_mbstowcs_s(void* buf, const void* dst, int n);`.
 *
 * Ghidra's decompiler names this call "CRT_mbstowcs_s" purely by symbol-
 * table lookup of its real target, 0x473E70 (confirmed via disassembly of
 * BuildingDescriptorEditor::Render/0x41E9F0: `PUSH 4; PUSH &DAT_0047e5a8;
 * PUSH <line_buf>; CALL 0x473e70`). Decompiling 0x473E70 directly shows it
 * is NOT the 5-arg secure `mbstowcs_s(ret,dest,destsz,src,count)` — it's a
 * 3-arg case-insensitive byte-string compare bounded by a max count
 * (uppercase/lowercase-folds both operands, loops up to `n` times, returns
 * 0 for "equal within n chars", nonzero otherwise) — i.e. the real CRT
 * `_strnicmp`/`strncasecmp`, reached here under a stale/misleading FID-
 * matched name (same class of bug as CRT_wcsstr below, and as
 * CRT_0x468610/CRT_0x468790's real _fread/_fseek identities). Implemented
 * for real as the genuine thin CRT wrapper it is. */
int CRT_mbstowcs_s(void* buf, const void* dst, int n)
{
    if (buf == nullptr || dst == nullptr) return 1;
    size_t count = n > 0 ? static_cast<size_t>(n) : 0;
    return strncasecmp(static_cast<const char*>(buf),
                        static_cast<const char*>(dst), count);
}

/* ----------------------------------------------------------- */
/* _CrtDbg_report_fmt_helper(void*, const void*) -> BuildingDescriptorEditor::Render
 * Caller decl (input/BuildingDescriptorEditor.cpp:67, extern "C"):
 * `void* _CrtDbg_report_fmt_helper(void* buf, const void* fmt);`.
 *
 * Real target 0x467490 (confirmed via disassembly of Render: `... CALL
 * 0x467490` inside the "shifts" section handler). Decompiling 0x467490
 * shows it builds a fake in-memory FILE record pointing at `buf` and calls
 * CRT_printf(&fakefile, fmt, &stack_args) — i.e. it IS a real sprintf-into-
 * buffer helper, but genuinely variadic: the real call site at 0x41EE0F
 * pushes 6 args (buf, fmt, and 4 more longs from this+0x538/0x53c/0x54c/
 * 0x550) matching the "%ld %ld %ld %ld" format string, while the CALLER's
 * own C++ transcription only captures 2 of those 6 args (a pre-existing
 * TRANSCRIBED-not-VALIDATED gap in that file, out of this pass's scope to
 * fix). Since the real 4 numeric args aren't available through the
 * caller's own 2-arg declaration, and the formatted result is never read
 * back afterward in that code path (diagnostic-only, matching the
 * `_CrtDbg`-report naming), a safe stub is used rather than fabricating
 * plausible-looking numeric output. Reachable via ordinary building-
 * descriptor loading, so this warns once instead of asserting. */
void* _CrtDbg_report_fmt_helper(void* buf, const void* fmt)
{
    (void)fmt;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: _CrtDbg_report_fmt_helper(0x467490) — real function is a "
            "variadic sprintf-into-buffer helper, but the caller's own 2-arg "
            "declaration (input/BuildingDescriptorEditor.cpp:67) can't supply "
            "the real 4 extra long args; result is diagnostic-only and unused "
            "downstream, so this is a safe no-op\n");
        warned = true;
    }
    return buf;
}

/* ----------------------------------------------------------- */
/* CRT_fabs(void*, void*) -> edit_key_handler_parse; ChildWindow::Render
 * Callers (both extern "C"):
 *   input/BuildingDescriptorEditor.cpp:71: `void* CRT_fabs(void* a, void* b);`
 *   ui/UI_ChildWindow.cpp:39:              `void* CRT_fabs(void* stream, void* outBuf);`
 *
 * Real target 0x464F70 (confirmed via disassembly of edit_key_handler_parse/
 * 0x41F2B0: `MOV ECX,EDI; PUSH EBP; CALL 0x464F70`, i.e. __thiscall with
 * ECX=stream, one pushed int* — NOT how a real `double fabs(double)` is
 * ever called). Decompiling 0x464F70 directly shows: calls
 * WNDPROC_Stream_InputPrefix(this,0), reads+converts a token to an int,
 * writes it to *param_1, does the same critical-section-unlock pattern seen
 * throughout resources/WndProcStream.cpp, then `return this` for chaining —
 * i.e. this is a genuine `WNDPROC_Stream::operator>>(int&)`-style numeric
 * stream-extraction method, not math fabs(). (crt_stubs.cpp's "0x464EF0-
 * 0x4655B8 = math library" range comment is itself a size-based guess that
 * doesn't hold for this address — the existing per-caller TODO comments in
 * both files above already flagged this identity as unresolved, and this
 * disassembly now explains why: correctly implementing it needs
 * WNDPROC_Stream's internal field layout, which lives in resources/
 * WndProcStream.h/.cpp — out of scope to add methods there in this pass.)
 * Safe default: return the stream pointer unchanged (preserves the real
 * function's chaining contract) and zero the destination int (matches the
 * memset(0) state the callers' own ctors already leave these fields in).
 * Both call sites are reachable from ordinary building-descriptor/cursor-
 * editor UI paths, so this warns once instead of asserting. */
void* CRT_fabs(void* stream, void* out)
{
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: CRT_fabs(0x464F70) — real identity is WNDPROC_Stream's "
            "int-extraction operator>>, not math fabs(); needs WNDPROC_Stream "
            "field layout to implement for real (see resources/WndProcStream.h). "
            "Returning stream unchanged + zeroed output as a safe placeholder.\n");
        warned = true;
    }
    if (out != nullptr) {
        std::memset(out, 0, sizeof(int32_t));
    }
    return stream;
}

/* ----------------------------------------------------------- */
/* CRT_fmod(void*, void*) -> edit_key_handler_parse
 * Caller (input/BuildingDescriptorEditor.cpp:70, extern "C"):
 * `void CRT_fmod(void* stream, void* outByte);`.
 *
 * Real target 0x464EF0 (confirmed via disassembly of edit_key_handler_parse:
 * `MOV ECX,EDI; PUSH <local byte>; CALL 0x464EF0`, same __thiscall shape as
 * CRT_fabs above). Decompiling 0x464EF0 shows: InputPrefix, then a call to
 * (mislabeled) "CRT_cos" whose result (checked against the 0xFFFFFFFF EOF
 * sentinel) is stored as a single byte into *param_1 — i.e. a
 * `WNDPROC_Stream::operator>>(char&)`-style single-byte extraction, the
 * sibling of CRT_fabs's int-extraction above. Same rationale: real fix
 * belongs in resources/WndProcStream.h/.cpp (out of scope here); safe
 * default zeroes the one output byte. Reachable, so warns once. */
void CRT_fmod(void* stream, void* outByte)
{
    (void)stream;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: CRT_fmod(0x464EF0) — real identity is WNDPROC_Stream's "
            "byte-extraction operator>>, not math fmod(); needs WNDPROC_Stream "
            "field layout to implement for real. Zeroing the output byte as a "
            "safe placeholder.\n");
        warned = true;
    }
    if (outByte != nullptr) {
        *static_cast<char*>(outByte) = 0;
    }
}

/* ----------------------------------------------------------- */
/* _isspace(int) -> WNDPROC_Stream::SkipWhitespace/ExtractToken
 * Caller (resources/WndProcStream.cpp:31, extern "C"): `int _isspace(int c);`
 * Genuine thin CRT wrapper — no existing definition anywhere in the tree. */
int _isspace(int c)
{
    return std::isspace(c) ? 1 : 0;
}

/* ----------------------------------------------------------- */
/* _strrchr(const char*, int) -> Town::save_received_postcard
 * Caller (town/Town.cpp:155, extern "C"):
 * `char* _strrchr(const char* s, int c);` (classic const-in/non-const-out
 * strrchr signature). Genuine thin CRT wrapper. */
char* _strrchr(const char* s, int c)
{
    return const_cast<char*>(std::strrchr(s, c));
}

/* ----------------------------------------------------------- */
/* sprintf_wrapper -> TrainStation::Init
 * Caller (game/TrainStation.cpp:49, extern "C"):
 * `void sprintf_wrapper(char* buffer, const char* format, ...);` documented
 * there as address 0x466D60 — the SAME real address several other files
 * (ui/CursorEditWindow.cpp, ui/AboutDialog.cpp, ui/GameSetupPanel.cpp,
 * game/ScriptedObject.cpp) already document for "CRT_sprintf_buf" — i.e.
 * this is the identical real sprintf, just referenced under a second,
 * differently-named extern "C" declaration in this one file. Real thin
 * CRT wrapper. */
void sprintf_wrapper(char* buffer, const char* format, ...)
{
    if (buffer == nullptr || format == nullptr) return;
    va_list args;
    va_start(args, format);
    std::vsprintf(buffer, format, args);
    va_end(args);
}

/* ----------------------------------------------------------- */
/* GetSaveFileNameA(void*) -> Town::save_postcard_as
 * Caller (town/Town.cpp:194, extern "C"): `BOOL GetSaveFileNameA(void* ofn);`
 * Genuine native Save-As file dialog — no SDL3 file-dialog capability is
 * established anywhere in this tree (only GetOpenFileNameA has a real SDL3
 * body, in graphics/sdl3_window.cpp; no GetSaveFileNameA counterpart exists
 * there or anywhere else). Reachable from normal postcard-saving gameplay,
 * so this is a safe-default stub (warn once, report "user cancelled") per
 * CLAUDE.md's stub policy rather than a hard assert — Town::save_postcard_as
 * already handles a FALSE/cancelled result safely (its caller checks
 * `save_result == 0` and returns). */
BOOL GetSaveFileNameA(void* ofn)
{
    (void)ofn;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: GetSaveFileNameA — no native save-file dialog implemented "
            "on this host; reporting the save dialog as user-cancelled\n");
        warned = true;
    }
    return 0;
}

/* ----------------------------------------------------------- */
/* CopyFileA(const char*, const char*, BOOL) -> Town::save_received_postcard
 * Caller (town/Town.cpp:191, extern "C"):
 * `BOOL CopyFileA(const char* src, const char* dest, BOOL fail_if_exists);`
 * Genuine thin file-copy wrapper — real implementation via fopen/fread/
 * fwrite, respecting fail_if_exists exactly like the real Win32 API. */
BOOL CopyFileA(const char* src, const char* dest, BOOL fail_if_exists)
{
    if (src == nullptr || dest == nullptr) return 0;
    if (fail_if_exists) {
        FILE* existing = std::fopen(dest, "rb");
        if (existing != nullptr) {
            std::fclose(existing);
            return 0;
        }
    }
    FILE* in = std::fopen(src, "rb");
    if (in == nullptr) return 0;
    FILE* out = std::fopen(dest, "wb");
    if (out == nullptr) {
        std::fclose(in);
        return 0;
    }
    char buf[8192];
    size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
        if (std::fwrite(buf, 1, n, out) != n) { ok = false; break; }
    }
    std::fclose(in);
    std::fclose(out);
    return ok ? 1 : 0;
}

} /* extern "C" */

/* =========================================================== */
/* B. Plain C++-linkage symbols (caller declared these WITHOUT             */
/*    extern "C" — mangled names must match exactly)                       */
/* =========================================================== */

/* ----------------------------------------------------------- */
/* CRT_localtime(unsigned int*) -> ResourceGameObject::UpdateScheduledAnimation
 * Caller (core/BuildingMgrObjectGroup.cpp:14, plain C++, NOT inside any
 * extern "C" block): `extern tm* CRT_localtime(uint32_t*);`. This is a
 * DIFFERENT (C++-mangled) symbol from shared/link_stubs.cpp's extern "C"
 * `CRT_localtime(unsigned int*)` — same param type, but C linkage vs C++
 * linkage means different link-time symbols. Real thin CRT wrapper over
 * localtime(), address 0x4674e0 per the caller's own comment. */
tm* CRT_localtime(uint32_t* t)
{
    static tm result;
    time_t tt = (t != nullptr) ? static_cast<time_t>(*t) : 0;
    tm* p = localtime(&tt);
    if (p != nullptr) {
        result = *p;
        return &result;
    }
    return nullptr;
}

/* ----------------------------------------------------------- */
/* CRT_localtime(long const*) -> Building::DecideAction
 * Caller (game/Building.cpp:87, plain C++, outside its extern "C" block):
 * `extern void* CRT_localtime(const time_t* timer);`. time_t on this host
 * resolves to `long`, matching the dossier's "long const*" — a genuinely
 * distinct overload from the uint32_t* one above (different parameter
 * type => different C++ mangled name). Real thin CRT wrapper. */
void* CRT_localtime(const time_t* t)
{
    static tm result2;
    time_t tt = (t != nullptr) ? *t : 0;
    tm* p = localtime(&tt);
    if (p != nullptr) {
        result2 = *p;
        return &result2;
    }
    return nullptr;
}

/* ----------------------------------------------------------- */
/* CRT_sprintf_buf(void*, char const*, ...) -> ScriptedObject::HandleEvent
 * Caller (game/ScriptedObject.cpp:28, plain C++):
 * `int __cdecl CRT_sprintf_buf(void* buf, const char* fmt, ...);` — takes
 * void* (not char*), so it's distinct from shared/link_stubs.cpp's/
 * shared/stubs_impl.cpp's own `char*`-taking overloads of the same name
 * (no collision). Real address 0x466D60 (same sprintf as sprintf_wrapper
 * above). Genuine thin CRT wrapper via vsprintf. */
int CRT_sprintf_buf(void* buf, const char* fmt, ...)
{
    if (buf == nullptr || fmt == nullptr) return 0;
    va_list args;
    va_start(args, fmt);
    int n = std::vsprintf(static_cast<char*>(buf), fmt, args);
    va_end(args);
    return n;
}

/* ----------------------------------------------------------- */
/* CRT_wcsstr(unsigned char*, unsigned char*) -> AssetMgr_LoadFile
 * Caller (native/assetmgr_loadfile.c:37, compiled as C++ per meson's
 * `-x c++` for native/*.c, and NOT inside any extern "C" block — a plain
 * top-level `extern`, so this is C++-mangled):
 * `extern uint32_t __cdecl CRT_wcsstr(uint8_t* str, uint8_t* sub);`.
 *
 * Real target 0x471480 (documented consistently across game/Building.cpp,
 * game/TrainStation.cpp, input/BuildingDescriptorEditor.cpp, tests/
 * persistence_fixtures.h, etc. — all under the SAME misleading "wcsstr"
 * name). Decompiling 0x471480 directly shows a byte-wise, case-folding
 * (uppercase-normalizing) compare loop terminating on a NUL or first
 * mismatch and returning 0 for "equal", -1/+1 otherwise — i.e. this is the
 * real CRT `_stricmp`/`strcasecmp`, NOT a substring search despite the
 * "wcsstr" name, and NOT actually operating on wide characters despite the
 * "wcs" prefix (it walks byte-at-a-time narrow chars). This matches every
 * caller's own "inverted-match"/"identity unresolved" comments exactly:
 * `if (CRT_wcsstr(line, keyword) == 0)` really means "line equals keyword,
 * case-insensitively". Implemented for real as the genuine CRT wrapper it
 * is (this exact (uint8_t*,uint8_t*) overload has no existing definition
 * anywhere in the tree — checked via grep before adding this). */
uint32_t CRT_wcsstr(uint8_t* str, uint8_t* sub)
{
    if (str == nullptr || sub == nullptr) return 1; /* non-zero = "not equal" */
    return static_cast<uint32_t>(
        strcasecmp(reinterpret_cast<const char*>(str),
                   reinterpret_cast<const char*>(sub)));
}

/* ----------------------------------------------------------- */
/* CRT_0x468790(int, int, unsigned int) -> AssetMgr_LoadFile
 * Caller (native/assetmgr_loadfile.c:40, plain C++, same rationale as
 * CRT_wcsstr above): `extern void __cdecl CRT_0x468790(int32_t handle,
 * int32_t offset, uint32_t origin);`.
 *
 * Real target 0x468790: Ghidra's OWN symbol table names the decompiled
 * function at this address "_fseek" directly (`int __cdecl _fseek(FILE*
 * _File, long _Offset, int _Origin)`, body: `_lock_file; _fseek_nolock;
 * _unlock_file`) — this is genuinely, unambiguously the real CRT fseek(),
 * consistent with every other file's own "fseek-like" comment for this
 * address (resources/AssetMgr.h, native/ddraw_filedata.c, graphics/
 * DDRAW.cpp). The int32_t "handle" here is a FILE* value carried as an
 * integer (this is a 32-bit-original binary; matches this tree's existing
 * HANDLE<->FILE* convention in shared/link_stubs.cpp's ReadFile/WriteFile).
 * Real thin wrapper via fseek(). */
void CRT_0x468790(int32_t handle, int32_t offset, uint32_t origin)
{
    if (handle == 0) return;
    std::fseek(reinterpret_cast<FILE*>(static_cast<intptr_t>(handle)),
               offset, static_cast<int>(origin));
}

/* ----------------------------------------------------------- */
/* CRT_0x468610(char*, unsigned int, unsigned int, int) -> AssetMgr_LoadFile
 * Caller (native/assetmgr_loadfile.c:38-39, plain C++, same rationale):
 * `extern void __cdecl CRT_0x468610(char* buf, uint32_t size,
 * uint32_t count, int32_t handle);`.
 *
 * Real target 0x468610: Ghidra's symbol table names it "_wcsnlen_locked"
 * (a stale FID-database match — a 4-param (ptr,uint,uint,FILE*) locked
 * wrapper cannot genuinely be wcsnlen(str,maxCount), which takes 2 params).
 * Decompiling 0x468610 shows body `_lock_file(param_4); CRT_wcsnlen(param_1,
 * param_2, param_3, param_4); _unlock_file(param_4);` — the (buf, size,
 * count, FILE*) parameter shape and lock/call/unlock structure is exactly
 * MSVC's real locked fread() wrapper (`fread` internally calls
 * `_lock_file`/`_fread_nolock`/`_unlock_file`), matching every other file's
 * "fread-like" comment for this exact address (resources/AssetMgr.h,
 * native/ddraw_filedata.c, graphics/DDRAW.cpp). Same int32_t-as-FILE*
 * handle convention as CRT_0x468790 above. Real thin wrapper via fread(). */
void CRT_0x468610(char* buf, uint32_t size, uint32_t count, int32_t handle)
{
    if (buf == nullptr || handle == 0) return;
    std::fread(buf, size, count,
               reinterpret_cast<FILE*>(static_cast<intptr_t>(handle)));
}

/* ----------------------------------------------------------- */
/* CRT_exit(void*, char const*) -> Game_LoadWaveFile
 * Caller (native/wave_io.c:165, compiled as C++ per meson, plain C++
 * local-extern, NOT inside extern "C"): `extern void __cdecl CRT_exit(void*
 * stack, const char* msg);`. Genuinely distinct from shared/link_stubs.cpp's
 * / shared/defsym_stubs.cpp's extern "C" `CRT_exit(const char**,const
 * char**)` overloads (different linkage AND different param types — no
 * collision). Game_LoadWaveFile's own doc comment: "may call CRT_exit on
 * error" at several RIFF/WAVE parse-failure sites, always passing a stack
 * context pointer and a descriptive message — this is a fatal-error
 * reporter, matching this tree's other `CRT_exit` overloads' exit(0)/exit(1)
 * behavior. Real implementation: report the message and terminate, mirror-
 * ing the sibling overloads' semantics. */
void CRT_exit(void* stack, const char* msg)
{
    (void)stack;
    if (msg != nullptr) {
        std::fprintf(stderr, "FATAL: %s\n", msg);
    }
    std::exit(1);
}

/* ----------------------------------------------------------- */
/* MessageBoxA(void*, char const*, char const*, unsigned int) -> WIN32_FatalError
 * Caller (native/win32_fatalerror.c:20-21, compiled as C++ per meson, plain
 * C++ local-extern, NOT inside extern "C"): `extern int32_t __stdcall
 * MessageBoxA(void* hWnd, const char* text, const char* caption,
 * uint32_t type);`.
 *
 * CALLER-DECLARATION-IS-WRONG: a real, correct MessageBoxA already exists
 * — graphics/sdl3_window.h declares `int MessageBoxA(HWND, LPCSTR, LPCSTR,
 * UINT)` inside `extern "C" { ... }` (guarded #ifndef _WIN32), and graphics/
 * sdl3_window.cpp:708 defines a real SDL3 message-box body under that same
 * extern "C" linkage. native/win32_fatalerror.c's own declaration above is
 * missing `extern "C"`, so it demands a C++-mangled symbol nothing defines.
 * SHOULD_BE_FIXED_AT: native/win32_fatalerror.c:20-21 — wrap in
 * `extern "C" { ... }` (matching the file's own WIN32_FatalError, which is
 * already correctly plain/no-args) so it binds to the real SDL3 body.
 * Per the CRITICAL CONSTRAINT, that file is not edited here; instead this
 * provides a functional (not just silent) fallback matching the caller's
 * CURRENT (wrong) plain-C++ shape: WIN32_FatalError is a real, reachable
 * WinMain-error-path function (not dead code), so printing the message
 * to stderr — rather than doing nothing — gives the user *some* visible
 * signal even before the real fix lands. */
int32_t MessageBoxA(void* hWnd, const char* text, const char* caption, uint32_t type)
{
    (void)hWnd;
    (void)type;
    std::fprintf(stderr, "[MessageBox] %s: %s\n",
                 (caption != nullptr) ? caption : "",
                 (text != nullptr) ? text : "");
    return 1; /* IDOK */
}

/* ----------------------------------------------------------- */
/* LoadIconA(void*, char const*) -> TrainStationWindow::Create
 * Caller (ui/TrainStationWindow.cpp:59, plain C++, no surrounding
 * extern "C" anywhere in that file): `extern HICON LoadIconA(HINSTANCE
 * hInstance, LPCSTR lpIconName);`.
 *
 * CALLER-DECLARATION-IS-WRONG: graphics/sdl3_window.h declares `HICON
 * LoadIconA(HINSTANCE, LPCSTR)` inside its file-spanning `extern "C" { ... }`
 * block, and graphics/sdl3_window.cpp:656 defines a real body under that
 * linkage. TrainStationWindow.cpp's own declaration lacks extern "C".
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67 — wrap this whole
 * block of Win32 API externs in `extern "C" { ... }` (see also ShowWindow/
 * SetFocus/SetTimer/KillTimer/SetRect/SetRectEmpty/UnionRect/IntersectRect
 * below, all declared in this same un-extern"C"'d block with the same
 * defect). Safe stub matching the caller's current (wrong) shape: returns
 * a null icon handle, which TrainStationWindow::Create already stores and
 * later passes on as an ordinary (possibly-null) HICON without dereferencing
 * it, so this is a safe default rather than a crash. */
HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    (void)hInstance;
    (void)lpIconName;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: LoadIconA (plain C++ linkage) — ui/TrainStationWindow.cpp's "
            "own declaration is missing extern \"C\"; the real SDL3 "
            "implementation in graphics/sdl3_window.cpp is unreachable from "
            "this call site until that's fixed\n");
        warned = true;
    }
    return nullptr;
}

/* ----------------------------------------------------------- */
/* SetRectEmpty(RECT*) -> TrainStationWindow::Create
 * Caller (ui/TrainStationWindow.cpp:65, plain C++): `extern void
 * SetRectEmpty(RECT* lprc);`.
 *
 * CALLER-DECLARATION-IS-WRONG (same defect/block as LoadIconA above): real
 * body is graphics/sdl3_window.cpp:634, extern "C".
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67 (see LoadIconA note).
 * This one is trivial pure rect math with zero behavioral ambiguity, so
 * rather than a dumb stub this provides the real (if duplicated) logic
 * directly. */
void SetRectEmpty(RECT* r)
{
    if (r != nullptr) {
        r->l = r->t = r->r = r->b = 0;
    }
}

/* ----------------------------------------------------------- */
/* ShowWindow(void*, int) -> TrainStationWindow::show
 * Caller (ui/TrainStationWindow.cpp:60, plain C++): `extern BOOL
 * ShowWindow(HWND hWnd, int nCmdShow);`.
 * CALLER-DECLARATION-IS-WRONG (same block): real body is graphics/
 * sdl3_window.cpp:225, extern "C".
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67.
 * Safe stub: report success (matches TrainStationWindow::show's fire-and-
 * forget usage — return value is discarded there). */
BOOL ShowWindow(HWND hWnd, int32_t nCmdShow)
{
    (void)hWnd;
    (void)nCmdShow;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: ShowWindow (plain C++ linkage) — ui/TrainStationWindow.cpp's "
            "own declaration is missing extern \"C\"; see LoadIconA note above\n");
        warned = true;
    }
    return 1;
}

/* ----------------------------------------------------------- */
/* SetFocus(void*) -> TrainStationWindow::show
 * Caller (ui/TrainStationWindow.cpp:61, plain C++): `extern HWND
 * SetFocus(HWND hWnd);`.
 * CALLER-DECLARATION-IS-WRONG (same block): real body is input/
 * Cursor_Editor.cpp:15 (`HWND SetFocus(HWND hWnd){ (void)hWnd; return
 * nullptr; }`, extern "C" via its own sdl3_window.h include).
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67.
 * Safe stub mirrors the real host body's own behavior exactly (return
 * nullptr) — TrainStationWindow::show discards the return value anyway. */
HWND SetFocus(HWND hWnd)
{
    (void)hWnd;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: SetFocus (plain C++ linkage) — ui/TrainStationWindow.cpp's "
            "own declaration is missing extern \"C\"; see LoadIconA note above\n");
        warned = true;
    }
    return nullptr;
}

/* ----------------------------------------------------------- */
/* SetTimer(void*, unsigned int, unsigned int, void*) -> TrainStationWindow::show
 * Caller (ui/TrainStationWindow.cpp:62, plain C++): `extern UINT_PTR
 * SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);`.
 * CALLER-DECLARATION-IS-WRONG (same block): real body is graphics/
 * sdl3_window.cpp:672, extern "C" (takes a typed TIMERPROC there, but that
 * doesn't matter for THIS caller's own mismatched, differently-mangled
 * plain-C++ symbol either way).
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67.
 * Safe stub: return a nonzero fake timer id (this->timer_id just stores it
 * for the matching KillTimer call below; a real periodic 200ms callback
 * isn't reproduced here, but nothing dereferences the id itself). */
uint32_t SetTimer(HWND hWnd, uint32_t nIDEvent, UINT uElapse, void* lpTimerFunc)
{
    (void)hWnd;
    (void)nIDEvent;
    (void)uElapse;
    (void)lpTimerFunc;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: SetTimer (plain C++ linkage) — ui/TrainStationWindow.cpp's "
            "own declaration is missing extern \"C\"; see LoadIconA note above\n");
        warned = true;
    }
    return 1;
}

/* ----------------------------------------------------------- */
/* KillTimer(void*, unsigned int) -> TrainStationWindow::hide
 * Caller (ui/TrainStationWindow.cpp:63, plain C++): `extern BOOL
 * KillTimer(HWND hWnd, UINT_PTR uIDEvent);`.
 * CALLER-DECLARATION-IS-WRONG (same block): real body is graphics/
 * sdl3_window.cpp:687, extern "C".
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67.
 * Safe stub: report success (return value discarded by the caller). */
BOOL KillTimer(HWND hWnd, uint32_t uIDEvent)
{
    (void)hWnd;
    (void)uIDEvent;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: KillTimer (plain C++ linkage) — ui/TrainStationWindow.cpp's "
            "own declaration is missing extern \"C\"; see LoadIconA note above\n");
        warned = true;
    }
    return 1;
}

/* ----------------------------------------------------------- */
/* SetRect(RECT*, int, int, int, int) -> TrainStationWindow::hide
 * Caller (ui/TrainStationWindow.cpp:64, plain C++): `extern void
 * SetRect(RECT* lprc, int x1, int y1, int x2, int y2);`.
 * CALLER-DECLARATION-IS-WRONG (same block): real body is graphics/
 * sdl3_window.cpp:624, extern "C".
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67.
 * Trivial pure rect math — real logic provided directly rather than a
 * dumb stub. */
void SetRect(RECT* r, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    if (r != nullptr) {
        r->l = x1;
        r->t = y1;
        r->r = x2;
        r->b = y2;
    }
}

/* ----------------------------------------------------------- */
/* UnionRect(RECT*, RECT const*, RECT const*) -> TrainStationWindow::hide
 * Caller (ui/TrainStationWindow.cpp:66, plain C++): `extern BOOL
 * UnionRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);`.
 * CALLER-DECLARATION-IS-WRONG (same block): real body is graphics/
 * sdl3_window.cpp:941, extern "C".
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67.
 * Trivial pure rect math (real Win32 UnionRect semantics: empty source
 * rects are excluded) — real logic provided directly. */
BOOL UnionRect(RECT* dst, const RECT* a, const RECT* b)
{
    if (dst == nullptr) return 0;
    if (a == nullptr || b == nullptr) return 0;
    bool aEmpty = (a->l >= a->r) || (a->t >= a->b);
    bool bEmpty = (b->l >= b->r) || (b->t >= b->b);
    if (aEmpty && bEmpty) {
        SetRectEmpty(dst);
        return 0;
    }
    if (aEmpty) { *dst = *b; return 1; }
    if (bEmpty) { *dst = *a; return 1; }
    dst->l = rect_min(a->l, b->l);
    dst->t = rect_min(a->t, b->t);
    dst->r = rect_max(a->r, b->r);
    dst->b = rect_max(a->b, b->b);
    return 1;
}

/* ----------------------------------------------------------- */
/* IntersectRect(RECT*, RECT const*, RECT const*) -> TrainStationWindow::hide
 * Caller (ui/TrainStationWindow.cpp:67, plain C++): `extern BOOL
 * IntersectRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT*
 * lprcSrc2);`.
 * CALLER-DECLARATION-IS-WRONG (same block): real body is graphics/
 * sdl3_window.cpp:963, extern "C".
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:56-67.
 * Trivial pure rect math (real Win32 IntersectRect semantics) — real logic
 * provided directly. */
BOOL IntersectRect(RECT* dst, const RECT* a, const RECT* b)
{
    if (dst == nullptr) return 0;
    if (a == nullptr || b == nullptr) {
        if (dst != nullptr) SetRectEmpty(dst);
        return 0;
    }
    int32_t l = rect_max(a->l, b->l);
    int32_t t = rect_max(a->t, b->t);
    int32_t r = rect_min(a->r, b->r);
    int32_t bo = rect_min(a->b, b->b);
    if (l < r && t < bo) {
        dst->l = l; dst->t = t; dst->r = r; dst->b = bo;
        return 1;
    }
    SetRectEmpty(dst);
    return 0;
}

/* ----------------------------------------------------------- */
/* IntersectRect(RECT*, RECT*, RECT*) -> Panel::DispatchEvent; UIPANEL_EndPaintEx
 * Callers (both plain C++, DISTINCT overload from the const one above —
 * non-const middle/last params, a genuinely different mangled symbol):
 *   game/Panel.h:226 / game/Panel.cpp:78:
 *     `extern int __stdcall IntersectRect(RECT* out, RECT* a, RECT* b);`
 *   ui/UIPANEL.cpp (no local declaration — picked up transitively via
 *     ui/UIPANEL.h's `#include "../game/Panel.h"`; UIPANEL_EndPaintEx's own
 *     calls `IntersectRect(&intersect1, restrict_rect, &dirty_rect)` pass
 *     two non-const RECT* args, so overload resolution prefers this exact
 *     (RECT*,RECT*,RECT*) match over the const-qualified one above).
 * game/World.cpp:60 independently declares this identical non-const
 * overload too (`int __stdcall IntersectRect(RECT* dst, RECT* src1, RECT*
 * src2);`) — not in this batch's assigned caller list, but the same fix
 * clears that landmine as a side effect.
 * No existing definition of this exact non-const overload exists anywhere
 * in the tree (checked via grep) — genuine gap, not a caller-declaration-
 * wrong case. Real logic (identical Win32 IntersectRect semantics),
 * delegating to the const overload above via an explicit const-qualifying
 * local copy of the pointers (calling IntersectRect(dst,a,b) directly here
 * would resolve back to THIS SAME non-const overload — infinite
 * recursion — so the args are first bound to `const RECT*` locals to force
 * overload resolution onto the sibling function above). */
int IntersectRect(RECT* dst, RECT* a, RECT* b)
{
    const RECT* ca = a;
    const RECT* cb = b;
    return IntersectRect(dst, ca, cb);
}

/* ----------------------------------------------------------- */
/* InvalidateRect(void*, RECT const*, int) -> TileMap::FullReset
 * Caller (world/tilemap.cpp:145, plain C++ — note lines 136-137 right
 * above it explicitly say `extern "C"` per-declaration, but this one and
 * UpdateWindow below do not): `extern BOOL InvalidateRect(HWND hWnd, const
 * RECT* lpRect, BOOL bErase);`.
 * CALLER-DECLARATION-IS-WRONG: graphics/sdl3_window.h declares this inside
 * its extern "C" block; graphics/sdl3_window.cpp:309 defines the real SDL3
 * body under that linkage.
 * SHOULD_BE_FIXED_AT: world/tilemap.cpp:145 — prefix with `extern "C"`,
 * matching the pattern already correctly applied to IntersectRect/UnionRect
 * on the two lines directly above it (136-137) — this looks like an
 * incomplete follow-through of that same earlier fix.
 * Safe stub: report success (TileMap::FullReset discards the return value). */
BOOL InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase)
{
    (void)hWnd;
    (void)lpRect;
    (void)bErase;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: InvalidateRect (plain C++ linkage) — world/tilemap.cpp's "
            "own declaration at line 145 is missing extern \"C\" (unlike "
            "IntersectRect/UnionRect right above it); real SDL3 implementation "
            "in graphics/sdl3_window.cpp is unreachable from this call site\n");
        warned = true;
    }
    return 1;
}

/* ----------------------------------------------------------- */
/* UpdateWindow(void*) -> TileMap::FullReset
 * Caller (world/tilemap.cpp:146, plain C++, same defect as InvalidateRect
 * immediately above): `extern BOOL UpdateWindow(HWND hWnd);`.
 * CALLER-DECLARATION-IS-WRONG: real body is graphics/sdl3_window.cpp:245,
 * extern "C".
 * SHOULD_BE_FIXED_AT: world/tilemap.cpp:146 — prefix with `extern "C"`.
 * Safe stub: report success. */
BOOL UpdateWindow(HWND hWnd)
{
    (void)hWnd;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: UpdateWindow (plain C++ linkage) — world/tilemap.cpp's own "
            "declaration at line 146 is missing extern \"C\"; see "
            "InvalidateRect note above\n");
        warned = true;
    }
    return 1;
}

/* ----------------------------------------------------------- */
/* InflateRect(RECT*, int, int) -> TileMap_ProcessDirtyRects
 * Caller (world/tilemap.cpp:142, plain C++): `extern void InflateRect(RECT*
 * rect, int dx, int dy);`.
 * CALLER-DECLARATION-IS-WRONG: shared/link_stubs.cpp already provides a
 * real, correct `int32_t InflateRect(RECT*,int32_t,int32_t)` body — but
 * under extern "C" linkage (inside link_stubs.cpp's big extern "C" block),
 * while tilemap.cpp's declaration here has plain C++ linkage.
 * SHOULD_BE_FIXED_AT: world/tilemap.cpp:142 — prefix with `extern "C"` so
 * it binds to link_stubs.cpp's real body instead.
 * Since the real logic is trivial and already known (link_stubs.cpp's own
 * `l-=dx;t-=dy;r+=dx;b+=dy`), it's duplicated here rather than stubbed —
 * behaviorally identical to the real implementation, just reachable via
 * this caller's actual (wrong) linkage until the real fix lands. */
void InflateRect(RECT* r, int32_t dx, int32_t dy)
{
    if (r != nullptr) {
        r->l -= dx;
        r->t -= dy;
        r->r += dx;
        r->b += dy;
    }
}

/* ----------------------------------------------------------- */
/* FormatMessageA(...) -> GameWindow::create
 * Caller (ui/GameWindow.cpp:66-67, plain C++, inside the `#ifndef _WIN32`
 * branch — note the sibling `#ifdef _WIN32` branch just above it, lines
 * 28-58, DOES correctly wrap the identical Win32 API set in extern "C";
 * only the non-Windows branch is missing it): `extern DWORD
 * FormatMessageA(DWORD flags, const void* source, DWORD message, DWORD
 * language, char* buffer, DWORD size, void* arguments);`.
 * CALLER-DECLARATION-IS-WRONG: graphics/sdl3_window.h declares this inside
 * extern "C"; graphics/sdl3_window.cpp:1032 defines the real SDL3-era body.
 * SHOULD_BE_FIXED_AT: ui/GameWindow.cpp:60-68 — wrap this `#ifndef _WIN32`
 * block in `extern "C" { ... }` to match its own `#ifdef _WIN32` sibling
 * block above.
 * Safe-default fallback (not a full behavioral duplicate — real
 * FormatMessageA has full Win32 error-table lookup logic that doesn't
 * belong reimplemented here): writes a generic "Error 0x<code>" string into
 * the caller-supplied buffer when one is given, returning the real
 * FormatMessageA return convention (chars written, or 0 = failure). */
DWORD FormatMessageA(DWORD flags, const void* source, DWORD messageId,
                      DWORD language, char* buffer, DWORD size, void* arguments)
{
    (void)flags;
    (void)source;
    (void)language;
    (void)arguments;
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
            "STUB: FormatMessageA (plain C++ linkage) — ui/GameWindow.cpp's "
            "own declaration (lines 66-67, #ifndef _WIN32 branch) is missing "
            "extern \"C\" (unlike its #ifdef _WIN32 sibling block above it); "
            "real SDL3 implementation in graphics/sdl3_window.cpp is "
            "unreachable from this call site\n");
        warned = true;
    }
    if (buffer != nullptr && size > 0) {
        int n = std::snprintf(buffer, size, "Error 0x%08X", messageId);
        return (n > 0) ? static_cast<DWORD>(n) : 0;
    }
    return 0;
}
