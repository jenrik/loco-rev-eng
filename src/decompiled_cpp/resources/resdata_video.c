/**
 * resdata_video.c — RESDATA video player / MCIWnd window management
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Base constructor/init/destroy for an MCIWnd-based video playback
 * child window used by the RESDATA script engine to play AVI/audio
 * overlays in the game. All functions operate on a RESDATA object with:
 *   +0x04: MCIWnd HWND
 *   +0x18: parent HWND (window to embed video in)
 *   +0x1C: HINSTANCE
 *
 * Functions:
 *   RESDATA_InitScriptEngine        — Script engine constructor (0x454250, 220 bytes)
 *   RESDATA_VideoPlayer_ScalarDeletingDtor — Scalar deleting dtor (0x454330, 77 bytes)
 *   RESDATA_CtorBase                — Create MCIWnd child window (0x454380, 274 bytes)
 *   RESDATA_FreeWindow              — Destroy MCIWnd child window (0x4544A0, 51 bytes)
 *   RESDATA_BaseInit                — Panel-based subclass init (0x4544E0, 152 bytes)
 *   RESDATA_DtorBase                — Panel-based subclass destoy (0x454630, 75 bytes)
 *
 * Calling conventions: mixed (see individual function annotations)
 */

#include "../shared/types.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Win32 API via IAT */
extern HWND   __stdcall CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName,
                                         LPCSTR lpWindowName, DWORD dwStyle,
                                         int X, int Y, int nWidth, int nHeight,
                                         HWND hWndParent, HMENU hMenu,
                                         HINSTANCE hInstance, LPVOID lpParam); /* 0x4772D4 */
extern LRESULT __stdcall SendMessageA(HWND hWnd, UINT Msg,
                                       WPARAM wParam, LPARAM lParam); /* 0x4772C0 */
extern BOOL __stdcall CloseHandle(HANDLE hObject);                     /* 0x4770A0 */
extern HANDLE __stdcall CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
                                     DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurity,
                                     DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                     HANDLE hTemplateFile);            /* 0x4770B4 */
extern BOOL __stdcall GetClientRect(HWND hWnd, LPRECT lpRect);         /* 0x477368 */
extern BOOL __stdcall PostMessageA(HWND hWnd, UINT Msg,
                                    WPARAM wParam, LPARAM lParam);      /* 0x477340 */
extern void    MCIWndRegisterClass(void);                               /* 0x4637FC (thunk) */
extern MMRESULT __stdcall mciSendCommandA(MCIDEVICEID mciId, UINT uMsg,
                                           DWORD_PTR dwParam1,
                                           DWORD_PTR dwParam2);            /* 0x47738C */

/* Game functions */
extern void*    __cdecl operator_new(size_t size);            /* 0x465CE0 */
extern void*    __thiscall GameObject_BaseCtor(void* self, int a, int b,
                                                int c, int d); /* 0x405790 */
extern void     __thiscall SetRectEmpty(void* rect);           /* 0x477388 */
extern void     __thiscall UI_DestroyTooltip(void* mgr, int id); /* 0x423D20 */
extern void     __thiscall UI_CenterWindow(int* outer_rect, int* inner_rect); /* 0x425A50 */
extern void     __cdecl GLOBAL_free(void* ptr);                /* 0x465CD0 */

/* ================================================================== */
/* MCIWnd message constants                                            */
/* ================================================================== */
#define MCIWNDM_GETDEVICEID 0x464   /* Get MCI device ID from MCIWnd */
#define MCIWNDM_REFRESH     0x499   /* Refresh MCIWnd content */
#define MCIWNDM_SETOWNER    0x48F   /* Set owner window */
#define MCIWNDM_REALIZE     0x804   /* Realize palette */
#define WM_CLOSE            0x10    /* Close window message */

#define MCIWNDF_SHOW_PALETTE 0x4000400B  /* MCIWnd style: show palette + stretch */

#define MCI_RESOURCE 0x806
#define MCI_OPEN     0x1000001

/* Message sent to parent window if the script INI file is missing */
#define WM_SCRIPT_INI_MISSING 0x3B9  /* 953 — PostMessage to parent HWND +0x18 */

/* ================================================================== */
/* RESDATA_InitScriptEngine — Script engine constructor                */
/* Address: 0x454250                                                    */
/* RET: 0xC (__thiscall with 3 stack args)                             */
/*                                                                     */
/* Called by: UI_InitScrollPanel (0x421084, 0x421188, 0x421F7D)       */
/*                                                                     */
/* Initializes the RESDATA script engine subsystem. This sets up a     */
/* video player window for an MCI-based AVI playback engine. It:       */
/*   1. Sets vtable at +0x00 to 0x4784C4 (ptr to scalar dtor)         */
/*   2. Zeros the MCIWnd HWND at +0x04                                 */
/*   3. Copies param_3 (file path string) into buffer at +0x20        */
/*   4. Stores param_2 (parent HWND uint) at +0x18                    */
/*   5. Stores param_1 (HINSTANCE) at +0x1C                            */
/*   6. Opens the file at path +0x20 (existence check)                 */
/*      - If file not found: PostMessage WM_SCRIPT_INI_MISSING to parent */
/*      - If file exists: close handle                                 */
/*   7. Gets parent client rect                                        */
/*   8. Sets initial window rectangle at +0x08:                        */
/*      left=0, top=0, right=0x280 (640), bottom=0x1E0 (480)          */
/*   9. Centers window rect via UI_CenterWindow                        */
/*   10. Calls RESDATA_CtorBase(this, path, &rect) to create MCIWnd   */
/*   11. Returns this                                                  */
/*                                                                     */
/* The struct layout for the RESDATA video player:                     */
/*   +0x00: vtable (-> 0x4784C4)                                       */
/*   +0x04: MCIWnd HWND (initially 0)                                  */
/*   +0x08: window rect (RECT: left, top, right, bottom)               */
/*   +0x18: parent HWND                                                */
/*   +0x1C: HINSTANCE                                                  */
/*   +0x20: file path string (copied from param_3)                     */
/*                                                                     */
/* @param this      ECX = RESDATA video player object                 */
/* @param param_1   First stack arg — HINSTANCE for window creation   */
/* @param param_2   Second stack arg — parent HWND (cast to uint)     */
/* @param param_3   Third stack arg — script INI file path (char*)    */
/* @return          The this pointer.                                  */
/* ================================================================== */
void* __thiscall
RESDATA_InitScriptEngine(void* this, uint32_t param_1, uint32_t param_2, const char* param_3)
{
    HANDLE hFile;
    RECT client_rect;
    int window_rect[4];   /* local: left, top, right, bottom */
    size_t path_len;
    const char* src;
    char* dst;

    /* Step 1: Set vtable and zero MCIWnd HWND */
    *(void**)this = (void*)0x004784C4;  /* +0x00 — vtable ptr */
    *(uint32_t*)((char*)this + 4) = 0;  /* +0x04 — MCIWnd HWND = NULL */

    /* Step 2: Copy param_3 (file path) to buffer at +0x20 */
    dst = (char*)this + 0x20;
    path_len = strlen(param_3) + 1;
    memcpy(dst, param_3, path_len);

    /* Step 3: Store constructor params */
    *(uint32_t*)((char*)this + 0x18) = param_2;  /* +0x18 — parent HWND */
    *(uint32_t*)((char*)this + 0x1C) = param_1;  /* +0x1C — HINSTANCE */

    /* Step 4: Check if the script INI file exists by trying to open it */
    hFile = CreateFileA(dst,             /* +0x20 — file path */
                        GENERIC_READ,    /* 0x80000000 */
                        FILE_SHARE_READ, /* 1 */
                        NULL,
                        OPEN_EXISTING,   /* 3 */
                        FILE_FLAG_NO_BUFFERING, /* 0x8000000 */
                        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        /* File not found — notify parent window */
        PostMessageA((HWND)param_2,      /* parent HWND */
                     WM_SCRIPT_INI_MISSING,  /* 0x3B9 */
                     0,
                     0);
        return this;
    }

    /* File exists — close the check handle */
    CloseHandle(hFile);

    /* Step 5: Get parent window client rect */
    GetClientRect((HWND)param_2, &client_rect);

    /* Step 6: Set initial window rectangle (640x480, positioned at 0,0) */
    window_rect[0] = 0;             /* left */
    window_rect[1] = 0;             /* top */
    window_rect[2] = 0x280;         /* right = 640 */
    window_rect[3] = 0x1E0;         /* bottom = 480 */

    /* Step 7: Center the window rect within the parent client area */
    UI_CenterWindow(&client_rect.left, window_rect);

    /* Step 8: Create the MCIWnd video player child window */
    RESDATA_CtorBase(this, (LPARAM)dst, window_rect);

    return this;
}

/* ================================================================== */
/* RESDATA_VideoPlayer_ScalarDeletingDtor — Scalar deleting destructor */
/* Address: 0x454330                                                    */
/* Vtable entry: VTBL at 0x4784C4, slot [0]                           */
/*                                                                     */
/* Standard MSVC scalar deleting destructor for the RESDATA video      */
/* player. Closes the MCIWnd window at +0x04 (realize palette,         */
/* WM_CLOSE), zeros the HWND pointer, then frees memory if flags & 1.  */
/*                                                                     */
/* Called by: GLOBAL_free/vtable dispatch                              */
/*                                                                     */
/* @param this   ECX = RESDATA video player object                    */
/* @param flags  If bit 0 set, also free the object's memory           */
/* @return       The object pointer (this).                            */
/* ================================================================== */
void* __thiscall
RESDATA_VideoPlayer_ScalarDeletingDtor(void* this, byte flags)
{
    HWND hMCIWnd;

    /* Restore vtable defensively */
    *(void**)this = (void*)0x004784C4;  /* +0x00 */

    /* Close MCIWnd window if present */
    hMCIWnd = *(HWND*)((char*)this + 4);   /* +0x04 */
    if (hMCIWnd != NULL) {
        SendMessageA(hMCIWnd, MCIWNDM_REALIZE, 0, 0);  /* Realize palette */
        SendMessageA(hMCIWnd, WM_CLOSE, 0, 0);          /* Close window */
        *(HWND*)((char*)this + 4) = NULL;               /* +0x04 = NULL */
    }

    /* Conditionally free memory */
}

/* ================================================================== */
/* RESDATA_CtorBase — Create MCIWnd child window for video playback    */
/* Address: 0x454380                                                    */
/*                                                                     */
/* Creates an MCIWnd child window embedded in the parent HWND (+0x18). */
/* param_1 is an LPARAM (resource path or config data), param_2 is     */
/* an int[4] rect (left, top, right, bottom) for positioning.          */
/*                                                                     */
/* Steps:                                                              */
/*   1. If existing MCIWnd at +0x04, realize and close it              */
/*   2. Register MCIWnd window class                                   */
/*   3. Create embedded MCIWnd with WS_CHILD + MCIWNDF_SHOW_PALETTE    */
/*   4. Set refresh callback with param_1                              */
/*   5. Resize to given rect dimensions                                */
/*   6. Get MCI device ID, set MCI_OPEN params                         */
/* ================================================================== */
void __thiscall RESDATA_CtorBase(void* this, LPARAM param_1, int* param_2)
{
    HWND hExistingMCIWnd;

    /* Step 1: Destroy existing MCIWnd if present */
    hExistingMCIWnd = *(HWND*)((char*)this + 4);
    if (hExistingMCIWnd != NULL) {
        SendMessageA(hExistingMCIWnd, MCIWNDM_REALIZE, 0, 0);  /* Realize palette */
        SendMessageA(hExistingMCIWnd, WM_CLOSE, 0, 0);         /* Close window */
        *(int*)((char*)this + 4) = 0;
    }

    /* Step 2: Register MCIWnd window class */
    MCIWndRegisterClass();

    /* Step 3: Create embedded MCIWnd */
    HWND hParent = *(HWND*)((char*)this + 0x18);
    HWND hNewMCIWnd = CreateWindowExA(
        0,                          /* dwExStyle */
        "MCIWndClass",              /* lpClassName */
        (LPCSTR)&PTR_DAT_0047f100, /* lpWindowName (dword data, not a string) */
        MCIWNDF_SHOW_PALETTE,       /* dwStyle (0x4000400B) */
        *param_2,                   /* X (rect.left) */
        param_2[1],                 /* Y (rect.top) */
        param_2[2] - *param_2,      /* width = right - left */
        param_2[3] - param_2[1],    /* height = bottom - top */
        hParent,                    /* hWndParent (+0x18) */
        (HMENU)0,                   /* hMenu */
        *(HINSTANCE*)((char*)this + 0x1C),  /* hInstance */
        NULL                        /* lpParam */
    );

    *(HWND*)((char*)this + 4) = hNewMCIWnd;

    if (hNewMCIWnd != NULL) {
        /* Step 4: Set refresh callback parameter */
        SendMessageA(hNewMCIWnd, MCIWNDM_REFRESH, 0, param_1);

        /* Step 5: Set window rect */
        {
            int rectParams[5];  /* local_1c */
            int width  = param_2[2] - *param_2;  /* right - left */
            int height = param_2[3] - param_2[1]; /* bottom - top */
            rectParams[3] = 0;   /* x */
            rectParams[4] = 0;   /* y */
            rectParams[0] = 0;   /* unused */
            rectParams[1] = 0;   /* unused */
            rectParams[2] = 0;   /* unused */

            SendMessageA(*(HWND*)((char*)this + 4), MCIWNDM_SETOWNER, 0,
                         (LPARAM)(rectParams + 3));

            /* Step 6: Get MCI device ID and set it up */
            MCIDEVICEID mciId = SendMessageA(
                *(HWND*)((char*)this + 4),
                MCIWNDM_GETDEVICEID, 0, 0);

            MCI_OPEN_PARMS mciOpen;  /* local_1c reinterpreted */
            mciOpen.wDeviceID = (UINT)((uint)*(int*)((char*)this + 0x18) & 0xFFFF);
            mciSendCommandA(mciId, MCI_RESOURCE, MCI_OPEN,
                            (DWORD_PTR)&mciOpen);
        }
    }
}

/* ================================================================== */
/* RESDATA_FreeWindow — Destroy MCIWnd child window                   */
/* Address: 0x4544A0                                                   */
/*                                                                     */
/* Realizes palette and closes the MCIWnd window at +0x04.            */
/* Sets +0x04 to NULL after destruction.                               */
/*                                                                     */
/* @param this  ECX = RESDATA video player object                     */
/* ================================================================== */
void __thiscall RESDATA_FreeWindow(void* this)
{
    HWND hMCIWnd = *(HWND*)((char*)this + 4);
    if (hMCIWnd != NULL) {
        SendMessageA(hMCIWnd, MCIWNDM_REALIZE, 0, 0);
        SendMessageA(hMCIWnd, WM_CLOSE, 0, 0);
        *(HWND*)((char*)this + 4) = NULL;
    }
}

/* ================================================================== */
/* RESDATA_BaseInit — Panel subclass initializer (shares GameObject base) */
/* Address: 0x4544E0                                                    */
/*                                                                     */
/* Calls GameObject_BaseCtor then sets the vtable to Panel's destructor */
/* wrapper (VTBL_PANEL, 0x4784C8). Initializes various fields to 0:   */
/*   +0xD0 (0x34): child panel pointer                                 */
/*   +0x9C (0x27): field_9C                                           */
/*   +0xDC (0x37): field_DC                                           */
/*   +0xD8 (0x36): field_D8                                           */
/*   +0xA0 (0x28): tooltip ID                                          */
/*   +0xA4 (0x29): field_A4                                           */
/*   +0xA8 (0x2A): field_A8                                           */
/*   +0xB0 (0x2C): rect (SetRectEmpty)                                  */
/*   +0xC0 (0x30): rect (SetRectEmpty)                                  */
/*   +0xAD (byte): 0 (unknown flag)                                    */
/*   +0x8C (0x23): field_8C                                           */
/*                                                                     */
/* Uses MSVC SEH (FS:[0] push/pop structure handler).                  */
/*                                                                     */
/* Called by: UI_InitScrollPanel, GameView_Ctor,                       */
/*            RESDATA_ScriptedObject_Ctor, ScriptEngine_Init,           */
/*            DDRAW_InitSprites.                                        */
/*                                                                     */
/* @param self  ECX = pointer to object being initialized.             */
/* @return      The self pointer.                                      */
/* ================================================================== */
void* __fastcall RESDATA_BaseInit(void* self)
{
    /* MSVC SEH prologue: register exception handler at 0x4763E8 */
    /* (Abbreviated — SEH is a compiler artifact not relevant to logic) */

    /* Call GameObject base constructor with (-1, -1, 0, 0) */
    GameObject_BaseCtor(self, -1, -1, 0, 0);

    /* Set vtable to Panel's scalar-destructor wrapper (VTBL_PANEL) */
    *(void**)self = (void*)VTBL_PANEL;  /* +0x00 = 0x4784C8 */

    /* Zero all fields */
    *(void**)((char*)self + 0xD0) = NULL;  /* +0xD0 — child panel ptr */
    *(int*)((char*)self + 0x9C)   = 0;     /* +0x9C */
    *(int*)((char*)self + 0xDC)   = 0;     /* +0xDC */
    *(int*)((char*)self + 0xD8)   = 0;     /* +0xD8 — panel_state */
    *(int*)((char*)self + 0xA0)   = 0;     /* +0xA0 — tooltip handle */
    *(int*)((char*)self + 0xA4)   = 0;     /* +0xA4 */
    *(int*)((char*)self + 0xA8)   = 0;     /* +0xA8 */

    SetRectEmpty((LPRECT)((char*)self + 0xB0));  /* +0xB0 rect */
    SetRectEmpty((LPRECT)((char*)self + 0xC0));  /* +0xC0 rect */

    *(char*)((char*)self + 0xAD) = 0;  /* flag byte */
    *(int*)((char*)self + 0x8C)  = 0;  /* +0x8C */

    /* MSVC SEH epilogue: restore previous exception handler */
    return self;
}

/* ================================================================== */
/* RESDATA_DtorBase — Panel subclass destructor                       */
/* Address: 0x454630                                                    */
/*                                                                     */
/* If child panel pointer (+0xD0) is set, calls its scalar dtor.      */
/* If tooltip ID (+0xA0) is set, destroys tooltip.                    */
/* Then calls vtable[6] (InitBase chain) with (0, -1, 0) to clean up. */
/* ================================================================== */
void __fastcall RESDATA_DtorBase(int* self)
{
    /* Destroy child panel if present */
    if (*(void**)(self[0x34]) != NULL) {  /* +0xD0 */
        /* Call scalar deleting destructor on child panel */
        typedef void (__thiscall* DtorFn)(void* p, byte flags);
        DtorFn dtor = *(DtorFn*)(*(int*)self[0x34]);
        dtor((void*)self[0x34], 1);
        self[0x34] = 0;
    }

    /* Destroy tooltip if present */
    if (self[0x28] != 0) {  /* +0xA0 */
        UI_DestroyTooltip(&g_tooltip_mgr, self[0x28]);
        self[0x28] = 0;
    }

    /* Chain to parent InitBase cleanup (vtable slot 6) */
    typedef void (__thiscall* InitBaseFn)(void* p, int a, int b, int c);
    InitBaseFn initBase = (InitBaseFn)(*(void***)self)[6];
    initBase(self, 0, -1, 0);
}

/* External globals */
extern void* g_tooltip_mgr;   /* 0x4FD220 — global tooltip manager */
