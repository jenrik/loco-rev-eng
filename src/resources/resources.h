/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Resource System (.RFH/.RFD file loading)
 * Original: loco.exe (Windows 95/98, DirectX 5 era)
 * Developer: Intelligent Games for LEGO Media
 *
 * This file was produced by reverse engineering the original binary.
 * Windows API calls are marked with WIN32: comments.
 * Linux/SDL2 replacement suggestions are marked with LINUX: comments.
 */

#ifndef DDRAW_INIT_H
#define DDRAW_INIT_H

#include <stdint.h>

/* =========================================================================
 * Platform abstraction: Windows types on non-Windows hosts
 *
 * WIN32:  #include <windows.h>  -- provides HFONT, HWND, HMODULE, etc.
 * LINUX:  Minimal stubs below.  Replace with SDL2 / GLib equivalents
 *         when building the ported version.
 * ========================================================================= */
#ifdef _WIN32
/* WIN32 */
#include <windows.h>
#else
/* LINUX: minimal Win32 type stubs for cross-compilation / analysis builds */
#include <stddef.h>

typedef void           *HWND;
typedef void           *HMODULE;
typedef void           *HFONT;       /* LINUX: TTF_Font* from SDL_ttf */
typedef void           *HGDIOBJ;
typedef void           *HBRUSH;      /* LINUX: SDL_Color or uint32_t */
typedef unsigned int    UINT;
typedef unsigned long   DWORD;
typedef int             INT;
typedef int             BOOL;
typedef char           *LPSTR;
typedef const char     *LPCSTR;
typedef long            LONG;

/* LINUX: SDL_Rect replacement for WIN32 RECT */
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;

#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

/* WIN32: GetPrivateProfileStringA -> LINUX: custom INI parser */
static inline DWORD GetPrivateProfileStringA(LPCSTR s, LPCSTR k,
    LPCSTR d, LPSTR b, DWORD n, LPCSTR f)
{ (void)s;(void)k;(void)d;(void)b;(void)n;(void)f; return 0; }

/* WIN32: GetPrivateProfileIntA -> LINUX: custom INI parser */
static inline INT GetPrivateProfileIntA(LPCSTR s, LPCSTR k,
    INT d, LPCSTR f)
{ (void)s;(void)k;(void)f; return d; }

/* WIN32: CreateFontA -> LINUX: TTF_OpenFont("LiberationSans.ttf", height) */
static inline HFONT CreateFontA(int h, int w, int e, int o, int wt,
    DWORD i, DWORD u, DWORD so, DWORD cs, DWORD op, DWORD cp,
    DWORD q, DWORD pf, LPCSTR face)
{ (void)h;(void)w;(void)e;(void)o;(void)wt;(void)i;(void)u;(void)so;
  (void)cs;(void)op;(void)cp;(void)q;(void)pf;(void)face; return NULL; }

/* WIN32: GetModuleHandleA -> LINUX: not applicable; return NULL */
static inline HMODULE GetModuleHandleA(LPCSTR n) { (void)n; return NULL; }

/* WIN32: LoadStringA -> LINUX: g_StringTable_Lookup(id, buf, len) */
static inline int LoadStringA(HMODULE m, UINT id, LPSTR b, int n)
{ (void)m;(void)id;(void)b;(void)n; return 0; }

/* WIN32: PostMessageA -> LINUX: SDL_PushEvent with SDL_USEREVENT */
static inline BOOL PostMessageA(HWND h, UINT m, DWORD wp, DWORD lp)
{ (void)h;(void)m;(void)wp;(void)lp; return 0; }

/* WIN32: CreateSolidBrush -> LINUX: store SDL_Color packed value */
static inline HBRUSH CreateSolidBrush(DWORD color) { (void)color; return NULL; }

/* WIN32: DeleteObject -> LINUX: no-op for SDL_Color values */
static inline BOOL DeleteObject(HGDIOBJ obj) { (void)obj; return 0; }

/* WIN32: DestroyWindow -> LINUX: SDL_DestroyWindow */
static inline BOOL DestroyWindow(HWND h) { (void)h; return 0; }

/* WIN32: FreeLibrary -> LINUX: dlclose */
static inline BOOL FreeLibrary(HMODULE m) { (void)m; return 0; }

/* WIN32: SetRect -> LINUX: manual SDL_Rect assignment */
static inline BOOL SetRect(LPRECT r, int l, int t, int ri, int b)
{ r->left=l; r->top=t; r->right=ri; r->bottom=b; return 1; }

/* WIN32: CopyRect -> LINUX: direct struct copy */
static inline BOOL CopyRect(LPRECT dst, const RECT *src)
{ *dst = *src; return 1; }

/* WIN32: OffsetRect -> LINUX: manual SDL_Rect offset */
static inline BOOL OffsetRect(LPRECT r, int dx, int dy)
{ r->left+=dx; r->right+=dx; r->top+=dy; r->bottom+=dy; return 1; }

/* WIN32: GetFileAttributesA -> LINUX: access(path, F_OK) != -1 */
static inline DWORD GetFileAttributesA(LPCSTR p) { (void)p; return INVALID_FILE_ATTRIBUTES; }

/* WIN32: wsprintfA -> LINUX: snprintf */
#include <stdio.h>
#define wsprintfA snprintf

#define WM_USER 0x0400

#endif /* _WIN32 */

/* =========================================================================
 * Data Structures
 * ========================================================================= */

/*
 * RFHEntry  --  one entry from the .RFH index, stored in a linked list.
 * Allocated on the heap (0x10 bytes each).
 * Address: heap-allocated by RFHMGR_Load (0x0045caa0).
 */
typedef struct RFHEntry {
    char            *filename;  /* +0x00 heap-allocated null-terminated asset path,
                                 *        e.g. "roads\\half-vwint.dat" */
    uint32_t         rfdSize;   /* +0x04 byte size of this asset in the .RFD file */
    uint32_t         rfdOffset; /* +0x08 byte offset of this asset in the .RFD file */
    struct RFHEntry *next;      /* +0x0C next node; NULL = end of list */
} RFHEntry;

/*
 * CRFHFile  --  manages the open .RFH index and .RFD data file.
 * Embedded inside CResourceMgr at offset +0x18.
 * Struct size: at least 0x10 bytes.
 *
 * WIN32:  rfdHandle is a FILE* (CRT wrapper around CreateFile)
 * LINUX:  rfdHandle is a POSIX FILE* -- no changes needed
 */
typedef struct CRFHFile {
    void     *rfdHandle;  /* +0x00 open FILE* to the .RFD binary data file;
                           *        NULL before RFHMGR_Load is called */
    RFHEntry *entryHead;  /* +0x04 head of linked list of RFH index entries */
    uint32_t  reserved;   /* +0x08 set to 0 after load completes */
    char     *rfdPath;    /* +0x0C heap copy of the full .RFD file path */
} CRFHFile;

/*
 * CResourceBase  --  polymorphic header shared by all cached resource types.
 * All concrete resource types (generic, surface, animation, large UI) share
 * this layout.  vtable[0] is always a destructor callable as (*vtable[0])(1)
 * to destroy and free the object.
 *
 * Concrete sizes:
 *   0x168  generic data blobs / audio
 *   0x630  surface / BMP
 *   0x63C  special surface
 *   0x178  animation
 *   0x7AC  large UI element
 *
 * WIN32:  vtable dispatch is C++ virtual dispatch (MSVC thiscall)
 * LINUX:  same dispatch mechanism; vtable[0] called with free_mem=1
 */
typedef struct CResourceBase {
    void       **vtable;     /* +0x000 C++ vtable; vtable[0] = destructor(int free_mem) */
    uint32_t     resourceID; /* +0x004 game resource ID this object was created for */
    /* ... type-specific fields between +0x008 and +0x017 ... */
    uint8_t      persistent; /* +0x018 1 = do not evict from cache (cursor & UI resources) */
    /* ... more type-specific fields ... */
    uint8_t      loaded_ok;  /* +0x162 1 = asset data successfully decoded/uploaded */
    /* total allocation size varies by subtype */
} CResourceBase;

/*
 * CButton  --  button / sound-effect resource (0x12C = 300 bytes).
 * IDs 0x5000..0x6060.  Also used for path-based bitmap lookups.
 *
 * WIN32:  audio buffer at +0x0C points to an IDirectSoundBuffer
 * LINUX:  audio buffer at +0x0C should point to a Mix_Chunk (SDL_mixer)
 */
typedef struct CButton {
    void       **vtable;           /* +0x00 &PTR_FUN_00478278 (CButton vtable) */
    int32_t      resID;            /* +0x04 resource ID, or -1 for name-based init */
    uint8_t      keepLoaded;       /* +0x08 1 = do not release audio buffer on SOUND_Release */
    uint8_t      loaded;           /* +0x09 1 = .wav / asset loaded and ready */
    /* +0x0A..0x0B  padding */
    void        *audioBuffer;      /* +0x0C DirectSound buffer ptr (LINUX: Mix_Chunk*) */
    /* +0x10..0x17  internal audio state */
    char         filename[0xE8];   /* +0x18 inline buffer: expanded asset path,
                                    *        e.g. "C:\\lego\\sound\\click.wav" */
    /* ... more fields up to 0x11F ... */
    int32_t      refCount;         /* +0x120 reference count; managed by SOUND_Load/Release */
} CButton;

/*
 * CResourceMgr  --  the singleton resource manager (~150 KB inline object).
 * Global instance: g_ResMgr at DAT_004855e8.
 *
 * The three cache arrays are INLINE (not heap pointers), making this struct
 * very large.  The object is never copied; only accessed via pointer.
 *
 * WIN32:  fonts[] holds GDI HFONT handles from CreateFontA
 * LINUX:  fonts[] should hold TTF_Font* pointers from TTF_OpenFont
 */
typedef struct CResourceMgr {
    uint32_t       flags;                  /* +0x0000 miscellaneous init flags */
    HFONT          fonts[5];               /* +0x0004 Arial 12/14/16/24/20pt
                                            *  LINUX: TTF_Font* from TTF_OpenFont */
    CRFHFile       rfhFile;                /* +0x0018 embedded RFH/RFD archive manager */
    /* +0x0028 */ int32_t clockPhase;      /* clock animation frame; -1 = not yet shown */
    /* +0x002C : aliasTable[0x4001]
     * Per-ID pointer that redirects to a slot in resourceCache[].
     * Set by RESMGR_SetAlias.  RESMGR_GetResource dereferences this first. */
    CResourceBase *aliasTable[0x4001];     /* +0x002C  (0x10004 bytes) */
    /* +0x10030 : main resource cache
     * NULL  = never loaded
     * -1    = permanently failed (load attempted and failed)
     * else  = pointer to the loaded CResourceBase-derived object */
    CResourceBase *resourceCache[0x4001];  /* +0x10030 (0x10004 bytes) */
    /* +0x20034 : button/sound cache for IDs 0x5000..0x6060
     * NULL  = never loaded
     * -1    = permanently unavailable
     * else  = pointer to a 300-byte CButton object */
    CButton       *buttonCache[0x1061];    /* +0x20034 (0x4184 bytes) */
    uint32_t       languageID;             /* +0x241B8 0=default/English, 1-9=locale;
                                            *          drives string-table offset in
                                            *          RESMGR_LoadLocalizedString */
} CResourceMgr;

/* =========================================================================
 * Global Variables
 * ========================================================================= */

/*
 * g_ResMgr  --  global singleton resource manager (DAT_004855e8).
 * ~150 KB; initialized by RESMGR_Init.
 */
extern CResourceMgr g_ResMgr;    /* 0x004855E8 */

/*
 * g_IniFile  --  global INI file manager (DAT_004a9eec).
 * Stores the path to LEGO.INI at +0x04 (passed to GetPrivateProfile* APIs).
 * WIN32:  path used with GetPrivateProfileStringA
 * LINUX:  path used with custom INI parser
 */
extern void *g_IniFile;           /* 0x004a9eec  (CINIFile, iniPath at +4) */

/*
 * g_Renderer  --  global renderer / audio playback target.
 * Passed as the first argument to FUN_00413210 (play audio at position).
 * WIN32:  DirectDraw surface or DirectSound device wrapper
 * LINUX:  SDL_Renderer* or SDL_Window* equivalent
 */
extern void *g_Renderer;          /* 0x004fd3bc */

/*
 * g_RFDArchive  --  open RFD archive handle.
 * Non-zero when the .RFD data file has been successfully opened.
 * Used by BUTTON_LoadSound and BITMAP_LoadFromArchiveOrFile to choose
 * between archive-based and filesystem-based asset loading.
 */
extern void *g_RFDArchive;        /* referenced in BUTTON_LoadSound */

/*
 * g_MusicX / g_MusicY  --  default on-screen position for music playback.
 * Passed to FUN_00413210 when playing background tracks at the default position.
 */
extern int   g_MusicX;           /* 0x004aad2c */
extern int   g_MusicY;           /* 0x004aad30 */

/*
 * DAT_004855e8  --  raw address alias for g_ResMgr start; used in pointer arithmetic.
 * DAT_004851d0  --  default/blank pixel buffer for BITMAP_Init palette stub.
 * DAT_004851f4  --  locale flag; value 10 disables all string loading in RESMGR_Init.
 * DAT_004a99c8  --  game data directory path prefix (stripped from filenames in archive lookups).
 * DAT_004aa4a0  --  screensaver window handle storage; HWND at +8.
 * DAT_004aad2c  --  alias for g_MusicX.
 * DAT_004aad30  --  alias for g_MusicY.
 * DAT_00485600  --  RFD archive handle used in BITMAP_LoadFromArchiveOrFile.
 * DAT_0047a950  --  BMP header mismatch error message string.
 * DAT_0047a5e8  --  error dialog title string.
 */
extern void  DAT_004855e8;
extern void  DAT_004851d0;
extern int   DAT_004851f4;
extern void  DAT_004a99c8;
extern void *DAT_004aa4a0;
extern int   DAT_004aad2c;
extern int   DAT_004aad30;
extern void *DAT_00485600;
extern void  DAT_0047a950;
extern void  DAT_0047a5e8;

/* Vtable symbols referenced in constructors/destructors */
extern void  PTR_FUN_00478274;   /* RFHEntry vtable */
extern void  PTR_FUN_00478278;   /* CButton vtable */
extern void  PTR_FUN_00478280;   /* CBitmap vtable */

/* Error string referenced in BITMAP_LoadFromArchiveOrFile */
extern void *s_Unable_to_allocate_thumbnail_mem_0047ee0c;

/* =========================================================================
 * Forward Declarations for Internal Constructor/Helper Functions
 *
 * These are the original Ghidra-named functions that serve as
 * placement constructors and subsystem helpers.  They are called
 * from the documented functions below and defined elsewhere in loco.exe.
 * ========================================================================= */

/*
 * Generic resource placement constructor (0x168-byte objects).
 * WIN32:  __thiscall  LINUX:  standard C call convention
 */
extern CResourceBase *FUN_00424af0(void *mem, uint32_t resID, const char *name);

/*
 * Surface / bitmap resource placement constructor (0x630-byte objects).
 * WIN32:  __thiscall  LINUX:  standard C call convention
 */
extern CResourceBase *FUN_0041e570(void *mem, uint32_t resID, const char *name);

/*
 * Special surface resource placement constructor (0x63C-byte objects).
 * WIN32:  __thiscall  LINUX:  standard C call convention
 */
extern CResourceBase *FUN_0044b190(void *mem, uint32_t resID, const char *name);

/*
 * Large UI element placement constructor (0x7AC-byte objects).
 * WIN32:  __thiscall  LINUX:  standard C call convention
 */
extern CResourceBase *FUN_0040e600(void *mem, uint32_t resID, const char *name);

/*
 * Animation resource placement constructor (0x178-byte objects).
 * WIN32:  __thiscall  LINUX:  standard C call convention
 */
extern CResourceBase *FUN_00436400(void *mem, uint32_t resID, const char *name);

/*
 * Surface base-class initializer (sets dimensions and bit depth).
 * Called at the start of BITMAP_Init.
 */
extern void FUN_0040cfa0(void *this, int width, int height, uint16_t bitDepth);

/*
 * Pre-initialization check called at the start of RESMGR_Init.
 * Returns non-zero on success.
 */
extern int FUN_0045b500(void);

/*
 * Audio subsystem initializers called from RESMGR_Init (step 5).
 * Address 0x4A99B0 is the audio engine state block.
 */
extern void FUN_0041f7e0(uint32_t audioBase);
extern void FUN_0041f970(uint32_t audioBase);

/*
 * Release/reset asset data held by an RFHEntry or similar object.
 * Called by RFHEntry_Destructor, RFHEntry_Reset, and BITMAP_LoadFromArchiveOrFile.
 */
extern void FUN_00447fb0(int objPtr);

/*
 * Archive entry lookup: searches the RFD archive for a filename.
 * Returns a pointer to the archive entry, and stores the byte offset in *outOffset.
 * WIN32/LINUX: pure C, no platform-specific code.
 */
extern void *FUN_0045cd00(void *archive, const char *filename, int *outOffset);

/*
 * Opens a seekable stream over an archive entry for reading.
 * Allocates a 0x5C-byte stream object and returns it.
 * WIN32/LINUX: pure C.
 */
extern void *FUN_00464490(void *streamObj, char *archiveEntry,
                           int offset, int flags);

/*
 * Frees an archive lookup result returned by FUN_0045cd00.
 */
extern void FUN_00466c70(void *archiveEntry);

/*
 * Opens a .wav file from the filesystem, returning an internal stream.
 * LINUX: equivalent to fopen(filename, "rb") wrapped in a stream object.
 */
extern void *FUN_00463aa0(const char *filename, int blockSize, int bufSize);

/*
 * Closes a file stream opened by FUN_00463aa0.
 * LINUX: equivalent to fclose.
 */
extern void FUN_00463b10(void *fileStream);

/*
 * Opens a bitmap file from the filesystem for staged reading.
 * LINUX: equivalent to fopen(filename, "rb") wrapped in a stream object.
 */
extern void *FUN_00463970(void *streamObj, const char *filename,
                           int param3, int bufSize, int flags);

/*
 * Reads 'len' bytes from a stream into 'buf'.
 * LINUX: equivalent to fread(buf, 1, len, stream).
 */
extern void FUN_00463810(void *stream, void *buf, uint32_t len);

/*
 * Error/assertion handler called on unrecoverable conditions (e.g.,
 * BMP header mismatch, out-of-memory).
 * WIN32: may show a MessageBox.  LINUX: fprintf(stderr, ...) or abort().
 */
extern void FUN_00466ce0(char **msgPtr, void *titlePtr);

/*
 * Audio playback function: plays an audio clip at a screen position.
 * WIN32:  DirectSound / DirectDraw integration
 * LINUX:  Mix_PlayChannel(-1, chunk, loops)
 */
extern void FUN_00413210(void *renderer, int btnPtr, void *reserved,
                          int x, int y, int scale, int flags);

/*
 * Blit a sub-rectangle of a surface to the render target.
 * WIN32:  DirectDraw BltFast or similar
 * LINUX:  SDL_RenderCopy with SDL_Rect
 */
extern void FUN_0042c330(void *surface,
                          int dstLeft, int dstTop, int dstRight, int dstBottom,
                          int reserved1, int reserved2,
                          int srcLeft, int srcTop, int srcRight, int srcBottom);

/*
 * Returns a pointer to the thread-local error code slot.
 * WIN32:  TLS-based error storage (FUN_00467fd0)
 * LINUX:  &errno or a thread-local int variable
 */
extern int *GetLastErrorSlot(void);

/*
 * Triggers a batch load of resources covering the type group for startID.
 * Inlined from RESMGR_GetResource; calls RESMGR_LoadResource in a loop.
 */
extern void RESMGR_LoadBatch(CResourceMgr *this, uint32_t startID);

/* =========================================================================
 * Public Function Declarations
 * ========================================================================= */

/*
 * INI_GetString  (FUN_00452d80)
 *
 * Reads a string value from the INI file.
 * WIN32:  GetPrivateProfileStringA
 * LINUX:  custom INI parser
 *
 * @param iniFile    CINIFile instance (path at iniFile+4)
 * @param section    INI section name (e.g. "DIRECTORIES")
 * @param key        INI key name (e.g. "ResFile")
 * @param defaultVal value to use if key is not found
 * @param outBuf     output buffer for the string value
 * @param bufLen     size of outBuf in bytes
 */
void INI_GetString(void *iniFile,
                   LPCSTR section,
                   LPCSTR key,
                   LPCSTR defaultVal,
                   LPSTR  outBuf,
                   DWORD  bufLen);

/*
 * INI_GetInt  (FUN_00452d60)
 *
 * Reads an integer value from the INI file.
 * WIN32:  GetPrivateProfileIntA
 * LINUX:  custom INI parser
 *
 * @param iniFile    CINIFile instance (path at iniFile+4)
 * @param section    INI section name
 * @param key        INI key name
 * @param defaultVal value to use if key is not found
 * @return           integer value from INI, or defaultVal
 */
int INI_GetInt(void *iniFile, LPCSTR section, LPCSTR key, INT defaultVal);

/*
 * RFHMGR_Load  (FUN_0045caa0)
 *
 * Parses the .RFH resource index and opens the paired .RFD data file.
 * WIN32/LINUX:  uses CRT fopen/fread/fclose; no platform-specific code.
 *
 * @param rfhMgr     CRFHFile instance to initialize
 * @param rfhPath    path to the resource file (any extension; replaced with .rfh/.rfd)
 * @return           1 on success, 0 on failure
 */
uint32_t RFHMGR_Load(CRFHFile *rfhMgr, char *rfhPath);

/*
 * RESMGR_Init  (FUN_00446050)
 *
 * One-time initialization of the global resource manager.
 * WIN32:  CreateFontA, GetModuleHandleA, LoadStringA, GetPrivateProfileStringA
 * LINUX:  TTF_OpenFont; g_StringTable_Lookup; custom INI parser
 *
 * @param this   pointer to the CResourceMgr singleton
 * @return       1 on success, 0 on failure
 */
int RESMGR_Init(CResourceMgr *this);

/*
 * RESMGR_FlushAllCaches  (FUN_004467e0)
 *
 * Evicts every resource from both the main cache and the button cache.
 * WIN32/LINUX:  C++ vtable dispatch; no platform-specific code.
 *
 * @param this   pointer to the CResourceMgr singleton
 */
void RESMGR_FlushAllCaches(CResourceMgr *this);

/*
 * RESMGR_LoadResource  (FUN_00446840)
 *
 * Factory dispatcher: creates the right resource object for resID.
 * WIN32:  SEH for C++ exception safety during construction
 * LINUX:  setjmp/longjmp or C++ try/catch
 *
 * @param this        pointer to the CResourceMgr singleton
 * @param resID       logical resource ID (0x0000..0x3FFF)
 * @param stringName  display name from the EXE string table (may be NULL)
 * @return            1 if the cache slot is now valid, 0 on failure
 */
int RESMGR_LoadResource(CResourceMgr *this,
                        uint32_t      resID,
                        const char   *stringName);

/*
 * RESMGR_LoadButtonRange  (FUN_00446cc0)
 *
 * Pre-loads button/sound resources for IDs in [idFirst, idLast].
 * WIN32:  GetModuleHandleA, LoadStringA
 * LINUX:  g_StringTable_Lookup
 *
 * @param this     pointer to the CResourceMgr singleton
 * @param idFirst  first button resource ID to load (>= 0x5000)
 * @param idLast   last button resource ID to load (<= 0x6060)
 * @return         1 always
 */
int RESMGR_LoadButtonRange(CResourceMgr *this,
                           uint32_t      idFirst,
                           uint32_t      idLast);

/*
 * RESMGR_GetResource  (FUN_00446ea0)
 *
 * Returns a pointer to the loaded resource for resID via alias table.
 * Lazy-loads if needed.  Sets thread-local error code on failure.
 * WIN32:  GetModuleHandleA, LoadStringA (in batch-load path)
 * LINUX:  g_StringTable_Lookup
 *
 * @param this   pointer to the CResourceMgr singleton
 * @param resID  logical resource ID (0x0000..0x3FFF)
 * @return       pointer to loaded CResourceBase, or 0 on failure
 */
int RESMGR_GetResource(CResourceMgr *this, int resID);

/*
 * RESMGR_GetResourceByID  (FUN_004470b0)
 *
 * Direct cache lookup by ID without alias indirection.
 * WIN32/LINUX:  same as RESMGR_GetResource but bypasses aliasTable.
 *
 * @param this   pointer to the CResourceMgr singleton
 * @param resID  logical resource ID (0x0000..0x3FFF)
 * @return       pointer to loaded CResourceBase, or 0 on failure
 */
int RESMGR_GetResourceByID(CResourceMgr *this, uint32_t resID);

/*
 * RESMGR_SetAlias  (FUN_00447290)
 *
 * Makes aliasTable[aliasID] point to the cache slot for targetID.
 *
 * @param this      pointer to the CResourceMgr singleton
 * @param aliasID   the ID to create an alias for
 * @param targetID  the ID whose cache slot should be referenced
 */
void RESMGR_SetAlias(CResourceMgr *this, int aliasID, int targetID);

/*
 * RESMGR_GetButtonResource  (FUN_004472b0)
 *
 * Returns a pointer to the CButton for resID (0x5000..0x605F).
 * Lazy-loads if the slot is uninitialized.
 *
 * @param this   pointer to the CResourceMgr singleton
 * @param resID  button resource ID (0x5000..0x605F)
 * @return       pointer to CButton, or 0 on failure
 */
int RESMGR_GetButtonResource(CResourceMgr *this, uint32_t resID);

/*
 * RESMGR_LoadLocalizedString  (FUN_00447330)
 *
 * Loads a localized string from the EXE string table.
 * For baseID in [100, 500] and a non-zero languageID, applies a
 * per-language offset before calling LoadStringA.
 * WIN32:  GetModuleHandleA, LoadStringA
 * LINUX:  g_StringTable_Lookup
 *
 * @param this    pointer to the CResourceMgr singleton
 * @param baseID  base string resource ID
 * @param outBuf  output buffer for the string
 * @param bufLen  size of outBuf in bytes
 */
void RESMGR_LoadLocalizedString(CResourceMgr *this,
                                UINT   baseID,
                                LPSTR  outBuf,
                                int    bufLen);

/*
 * RESMGR_PlayBgMusic  (FUN_00447930)
 *
 * Plays a background music track by button resource ID.
 * WIN32:  FUN_00413210 (audio subsystem)
 * LINUX:  Mix_PlayChannel(-1, chunk, -1)
 *
 * @param musicID  button resource ID for the music track (0x5000..0x605F)
 */
void RESMGR_PlayBgMusic(uint32_t musicID);

/*
 * RESMGR_DrawClock  (FUN_00447400)
 *
 * Renders the animated town-clock overlay.
 * WIN32:  SetRect, CopyRect, OffsetRect (GDI), FUN_0042c330 (blit)
 * LINUX:  SDL_Rect arithmetic; SDL_RenderCopy
 *
 * @param this       pointer to the CResourceMgr singleton
 * @param timeTicks  elapsed game ticks (seconds); drives the 12-frame clock
 */
void RESMGR_DrawClock(CResourceMgr *this, int timeTicks);

/*
 * BUTTON_InitByID  (FUN_00448990)
 *
 * Placement constructor for CButton using a numeric resource ID.
 * WIN32/LINUX:  no platform-specific code.
 *
 * @param this      allocated 300-byte block for the CButton
 * @param resID     resource ID (0x5000..0x6060)
 * @param hasSound  non-zero if a .wav asset should be resolved
 * @return          this
 */
CButton *BUTTON_InitByID(CButton *this, int resID, int hasSound);

/*
 * BUTTON_InitByName  (FUN_00448a20)
 *
 * Placement constructor for CButton from a file path string.
 * WIN32/LINUX:  no platform-specific code.
 *
 * @param this      allocated 300-byte block for the CButton
 * @param filePath  full path to the .wav or bitmap file
 * @return          this
 */
CButton *BUTTON_InitByName(CButton *this, char *filePath);

/*
 * BUTTON_LoadSound  (FUN_00448a70)
 *
 * Resolves and loads the WAV audio clip for a CButton.
 * WIN32:  GetFileAttributesA for existence fallback
 * LINUX:  access(path, F_OK) != -1
 *
 * @param this  CButton instance whose filename buffer is already populated
 */
void BUTTON_LoadSound(CButton *this);

/*
 * SOUND_Load  (FUN_00448d60)
 *
 * Increments refcount; decodes and buffers audio on first reference.
 * WIN32:  DirectSound IDirectSoundBuffer vtable calls
 * LINUX:  Mix_LoadWAV / SDL_mixer
 *
 * @param btn  CButton instance for the sound
 * @return     1 on success, error code on failure
 */
int SOUND_Load(CButton *btn);

/*
 * SOUND_Release  (FUN_00448ee0)
 *
 * Decrements refcount; destroys audio buffer when it reaches 0.
 * WIN32:  IDirectSoundBuffer::Stop + Release vtable calls
 * LINUX:  Mix_HaltChannel; Mix_FreeChunk
 *
 * @param btn  CButton instance for the sound
 * @return     1 always
 */
int SOUND_Release(CButton *btn);

/*
 * BITMAP_Init  (FUN_00448f30)
 *
 * Placement constructor for a bitmap/surface resource.
 * WIN32/LINUX:  no platform-specific code; uses CRT malloc.
 *
 * @param this      allocated buffer for the bitmap object
 * @param dataLen   byte length of the pixel data (buffer = dataLen+1)
 * @param width     bitmap width in pixels
 * @param height    bitmap height in pixels
 * @param colorKey  transparent color value
 * @param bitDepth  bits per pixel
 * @return          this
 */
void *BITMAP_Init(void *this, int dataLen, int width, int height,
                  uint32_t colorKey, uint16_t bitDepth);

/*
 * BITMAP_LoadFromArchiveOrFile  (FUN_00447ba0)
 *
 * Two-stage bitmap loader: RFD archive first, then filesystem fallback.
 * WIN32/LINUX:  no platform-specific code; file I/O uses CRT wrappers.
 *
 * @param this      bitmap object to populate
 * @param filePath  full path to the .bmp asset
 * @return          1 on success, 0 on failure
 */
int BITMAP_LoadFromArchiveOrFile(void *this, char *filePath);

/*
 * RFHEntry_DefaultInit  (FUN_00447b20)
 *
 * Default-initializes an RFHEntry / bitmap object in place.
 * WIN32/LINUX:  no platform-specific code.
 *
 * @param entry  object to initialize
 */
void RFHEntry_DefaultInit(RFHEntry *entry);

/*
 * RFHEntry_Destructor  (FUN_00447b60)
 *
 * C++ scalar destructor for RFHEntry-family objects.
 * WIN32/LINUX:  no platform-specific code.
 *
 * @param this   object to destroy
 * @param flags  bit 0: also free the heap block
 * @return       this
 */
void *RFHEntry_Destructor(void *this, uint8_t flags);

/*
 * RFHEntry_Reset  (FUN_00447b90)
 *
 * Resets asset data without freeing the object.
 * WIN32/LINUX:  no platform-specific code.
 *
 * @param entry  object to reset
 */
void RFHEntry_Reset(void *entry);

/*
 * SCREENSAVER_Init  (FUN_00448040)
 *
 * Initializes the screensaver context block.
 * WIN32:  CreateSolidBrush(0xA8C4D8)
 * LINUX:  store SDL_Color { R=0xD8, G=0xC4, B=0xA8, A=0xFF } packed value
 *
 * @param ctx  screensaver context block (array of uint32_t)
 * @return     ctx
 */
void *SCREENSAVER_Init(uint32_t *ctx);

/*
 * SCREENSAVER_Cleanup  (FUN_00448080)
 *
 * Tears down screensaver: frees brush, destroys window, unloads DLL.
 * WIN32:  DeleteObject, DestroyWindow, FreeLibrary
 * LINUX:  (no-op for brush); SDL_DestroyWindow; dlclose
 *
 * @param ctx  screensaver context block
 */
void SCREENSAVER_Cleanup(uint32_t *ctx);

/*
 * SCREENSAVER_PostQuit  (FUN_00448970)
 *
 * Posts a quit message to the screensaver message loop.
 * WIN32:  PostMessageA(hWnd, WM_USER+5, 0, 0)
 * LINUX:  SDL_PushEvent with SDL_USEREVENT, user.code = 5
 */
void SCREENSAVER_PostQuit(void);

#endif /* DDRAW_INIT_H */
