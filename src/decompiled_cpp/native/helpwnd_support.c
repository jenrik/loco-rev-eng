/**
 * helpwnd_support.c — HelpWnd page serialization and support functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Page management functions for the HelpWnd/AudioMgr tutorial system.
 * These handle serializing help pages between the page array and
 * resource-data format, and provide page-count/title accessors.
 *
 * Functions:
 *   HelpWnd_LoadPageData     — Clone page data from resource stream to pages array (0x44F750, 582 bytes)
 *   HelpWnd_SerializePages   — Write pages array back to resource stream    (0x44F9A0, 216 bytes)
 *   HelpWnd_GetPageCount     — Return loaded page count (vtable[0])         (0x44F2A0, 30 bytes)
 *   HelpWnd_GetPageTitle     — Return page title string (vtable[1])         (0x44F2C0, 116 bytes)
 *   HelpWnd_SetPage          — Process page linkage event                   (0x44F340, 88 bytes)
 *   HelpWnd_GetPageCount     — Return page count                           (0x44F2A0, 30 bytes)
 *   ArrivalQueue_AddVehicle  — Add vehicle to train station arrival queue   (0x44F3A0, 101 bytes)
 *   ArrivalQueue_RemoveVehicle — Remove vehicle from arrival queue          (0x44F410, 123 bytes)
 *   AudioMgr_PlayEvent       — Play audio narration event for a page        (0x44F560, 494 bytes)
 *   AudioMgr_Ctor            — AudioMgr/HelpWnd constructor wrapper          (0x44F490, 87 bytes)
 *
 * Calling convention: __thiscall for HelpWnd/AudioMgr methods
 *                     __fastcall for standalone helpers
 */

#include "../shared/types.h"

/* ================================================================== */
/* HelpWnd page data structures (shared with HelpWnd.h)                */
/* ================================================================== */

/* HelpPageData — 0x3C bytes per page */
#pragma pack(push, 1)
struct HelpPageData {
    int32_t pageId;          /* +0x00  Resource ID / page number */
    int32_t nextPageId;      /* +0x04  Link to next page ID */
    int32_t textResId;       /* +0x08  Text resource ID */
    int32_t field_0C;        /* +0x0C  (unknown) */
    int32_t field_10;        /* +0x10  Transition/fade param */
    uint8_t hasOverlay;      /* +0x14  Overlay flag */
    int32_t soundResId;      /* +0x18  Sound resource ID */
    RECT    clickRect;       /* +0x1C  Clickable hotspot */
    RECT    overlayRect;     /* +0x2C  Overlay rect */
};
#pragma pack(pop)

/* ================================================================== */
/* External references                                                */
/* ================================================================== */

extern void __thiscall WNDPROC_StreamReadLine(void* stream, char* buf, int maxLen); /* 0x464BC0 */
extern void __thiscall WNDPROC_StreamRead(void* stream, void* buf, int size);        /* 0x4642F0 */
extern void __thiscall WNDPROC_StreamWrite(void* stream, const void* buf, int size); /* 0x4646C0 */
extern int  __cdecl CRT_atoi(const char* str);                                        /* 0x466390 */
extern int  __cdecl CRT_strcmp(const char* s1, const char* s2);                      /* 0x467330 */

/* Audio channel and resource management (from GameAudio) */
extern void __thiscall GameAudio_PlayResourceEx(void* audio, UINT resId, int* outChannel); /* 0x458DC0 */
extern void __thiscall AudioChannel_Release(void* channel);                                 /* 0x458F40 */
extern void __thiscall RESMGR_LoadSoundResource(int handle);                                /* 0x448EE0 */
extern void __thiscall RESMGR_ReleaseSoundResource(int handle);                             /* 0x448EE0? */
extern int  __thiscall ResourceManager_GetStringById(void* resmgr, UINT id);               /* 0x4472B0 */

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern HelpWnd* g_audio_mgr;     /* 0x4FD2CC — HelpWnd/AudioMgr singleton */
extern void*    g_resmgr;        /* 0x4855E8 */
extern int      g_helpwnd;       /* HelpWnd instance pointer */

#define HELP_MAX_PAGES 200

/* ================================================================== */
/* HelpWnd_LoadPageData — Clone page data from a stream to pages array */
/* Address: 0x44F750                                                    */
/*                                                                      */
/* Reads HelpPageData entries from a WNDPROC stream and copies them    */
/* into the HelpWnd's pages[] array at +0x15C. Used when loading help  */
/* data from script files. Each page record is 0x3C bytes.             */
/*                                                                      */
/* This is called by HelpWnd_LoadHelpData after parsing the text        */
/* stream; it performs a bulk copy of serialized page data from a      */
/* binary buffer into the structured pages[] array.                    */
/* ================================================================== */
void __thiscall HelpWnd_LoadPageData(void* _this, void* stream)
{
    /* Read page count from stream */
    int pageCount;
    WNDPROC_StreamRead(stream, &pageCount, 4);

    if (pageCount > HELP_MAX_PAGES) {
        pageCount = HELP_MAX_PAGES;  /* Clamp to max */
    }

    /* Read each page entry */
    for (int i = 0; i < pageCount; i++) {
        HelpPageData page;
        WNDPROC_StreamRead(stream, &page, sizeof(HelpPageData));

        /* Copy into the pages[] array at +0x15C + i * 0x3C */
        HelpPageData* dest = (HelpPageData*)((char*)_this + 0x15C + i * 0x3C);
        *dest = page;
    }

    /* Update loaded count field */
    *(int*)((char*)_this + 0x303C) = pageCount;  /* helpDataLoaded */
}

/* ================================================================== */
/* HelpWnd_SerializePages — Write pages array to a stream              */
/* Address: 0x44F9A0                                                    */
/*                                                                      */
/* Serializes the pages[] array back into a WNDPROC stream for save.   */
/* Writes count first, then each HelpPageData entry.                   */
/* ================================================================== */
void __thiscall HelpWnd_SerializePages(void* _this, void* stream)
{
    int pageCount = *(int*)((char*)_this + 0x303C);  /* loaded page count */

    if (pageCount > HELP_MAX_PAGES) {
        pageCount = HELP_MAX_PAGES;
    }

    /* Write count */
    WNDPROC_StreamWrite(stream, &pageCount, 4);

    /* Write each page entry */
    for (int i = 0; i < pageCount; i++) {
        HelpPageData* page = (HelpPageData*)((char*)_this + 0x15C + i * 0x3C);
        WNDPROC_StreamWrite(stream, page, sizeof(HelpPageData));
    }
}

/* ================================================================== */
/* HelpWnd_GetPageCount — Return page count (vtable[0] / GetPageCount) */
/* Address: 0x44F2A0                                                    */
/*                                                                      */
/* Returns the number of loaded help pages. Used for navigation        */
/* boundary checks.                                                     */
/* ================================================================== */
int __thiscall HelpWnd_GetPageCount(void* _this)
{
    return *(int*)((char*)_this + 0x303C);  /* HelpWnd page count at +0x303C */
}

/* ================================================================== */
/* HelpWnd_GetPageTitle — Return page title string (vtable[1])         */
/* Address: 0x44F2C0                                                    */
/*                                                                      */
/* Reads the text resource ID from the current page, loads the         */
/* localized string from the resource manager, and returns it.         */
/* ================================================================== */
int __thiscall HelpWnd_GetPageTitle(void* _this)
{
    int currentPageIdx = *(int*)((char*)_this + 0x3040);  /* +0x3040 */
    if (currentPageIdx < 0 || currentPageIdx >= HELP_MAX_PAGES) {
        return 0;  /* No valid page selected */
    }

    HelpPageData* page = (HelpPageData*)((char*)_this + 0x15C + currentPageIdx * 0x3C);
    int textResId = page->textResId;

    if (textResId == 0) {
        return 0;  /* No text for _this page */
    }

    /* Load the text resource string */
    int stringHandle = ResourceManager_GetStringById(g_resmgr, (UINT)textResId);
    return stringHandle;
}

/* ================================================================== */
/* HelpWnd_SetPage — Process page linkage event                        */
/* Address: 0x44F340                                                    */
/*                                                                      */
/* Processes page linkage from a list node. Checks if there is pending */
/* page data and the update flag is not set, then loads page's linked  */
/* game object (Vehicle), sets its animation state, and marks as done. */
/* ================================================================== */
void __thiscall HelpWnd_SetPage(void* _this)
{
    int currentPageIdx = *(int*)((char*)_this + 0x3040);

    if (currentPageIdx < 0 || currentPageIdx >= HELP_MAX_PAGES) {
        return;
    }

    HelpPageData* currentPage = (HelpPageData*)((char*)_this + 0x15C + currentPageIdx * 0x3C);

    /* If page has a linked object resource ID and update flag is clear */
    if (currentPage->pageId != 0 && *(int*)((char*)_this + 0x11C) == 0) {
        /* Load game vehicle from resource */
        int resId = currentPage->pageId;

        /* Find matching game object */
        typedef int (__thiscall* FindObjFn)(void* obj, int resId);
        FindObjFn findObj = (FindObjFn)(*(void***)_this)[5];  /* vtable[5] */
        int gameObj = findObj(_this, resId);

        if (gameObj != 0) {
            /* Set animation state on the linked object */
            typedef void (__thiscall* SetAnimFn)(int obj, int state);
            void** gameObjVtable = *(void***)(intptr_t)gameObj;
            SetAnimFn setAnim = (SetAnimFn)gameObjVtable[0x1C >> 2]; /* vtable[7] */
            setAnim(gameObj, 1);  /* Start animation */

            /* Mark as processed */
            *(int*)((char*)_this + 0x11C) = 1;
        }
    }
}

/* ================================================================== */
/* ArrivalQueue_AddVehicle — Add vehicle to train station arrival queue */
/* Address: 0x44F3A0                                                    */
/*                                                                      */
/* Adds a vehicle (referenced by internal ID) to the train station     */
/* arrival/departure queue. The queue is stored as an array of IDs.    */
/* ================================================================== */
void __thiscall ArrivalQueue_AddVehicle(void* _this, int vehicleId)
{
    int queueSize = *(int*)((char*)_this + 0x10);  /* Queue size at +0x10 */
    int* queueArray = *(int**)((char*)_this + 0x0C);  /* Queue array at +0x0C */

    /* Scan for existing entry first */
    for (int i = 0; i < queueSize; i++) {
        if (queueArray[i] == vehicleId) {
            return;  /* Already in queue */
        }
    }

    /* Check if queue has room */
    int capacity = *(int*)((char*)_this + 0x14);  /* Capacity at +0x14 */
    if (queueSize < capacity) {
        /* Add to queue */
        *(int**)((char*)_this + 0x0C) = queueArray;
        queueArray[queueSize] = vehicleId;
        *(int*)((char*)_this + 0x10) = queueSize + 1;
    }
}

/* ================================================================== */
/* ArrivalQueue_RemoveVehicle — Remove vehicle from arrival queue      */
/* Address: 0x44F410                                                    */
/*                                                                      */
/* Removes a vehicle from the arrival queue by ID. Compacts the array  */
/* by shifting remaining elements.                                      */
/* ================================================================== */
void __thiscall ArrivalQueue_RemoveVehicle(void* _this, int vehicleId)
{
    int queueSize = *(int*)((char*)_this + 0x10);
    int* queueArray = *(int**)((char*)_this + 0x0C);

    for (int i = 0; i < queueSize; i++) {
        if (queueArray[i] == vehicleId) {
            /* Remove by shifting left */
            for (int j = i; j < queueSize - 1; j++) {
                queueArray[j] = queueArray[j + 1];
            }
            queueArray[queueSize - 1] = 0;
            *(int*)((char*)_this + 0x10) = queueSize - 1;
            return;
        }
    }
}

/* ================================================================== */
/* AudioMgr_Ctor — AudioMgr/HelpWnd constructor wrapper                */
/* Address: 0x44F490                                                    */
/*                                                                      */
/* Helper window constructor (chains to GameWindow then HelpWnd::init).*/
/* Allocates 0x3190 bytes total (0x118 GameWindow + 0x3078 HelpWnd).   */
/* Sets vtable to VTBL_HELPWND (0x478428).                              */
/* ================================================================== */
void __thiscall AudioMgr_Ctor(void* _this, HINSTANCE hInstance, UINT resId)
{
    /* Call GameWindow constructor */
    typedef void (__thiscall* GameWindowCtor)(void* self, HINSTANCE hInst, UINT resId);
    ((GameWindowCtor)0x413AB0)(_this, hInstance, resId);  /* GameWindow::ctor */

    /* Set vtable to HelpWnd */
    *(void***)_this = (void**)0x00478428;  /* VTBL_HELPWND */

    /* Call HelpWnd::init */
    typedef void (__thiscall* InitFn)(void* self);
    ((InitFn)0x451180)(_this);
}

/* ================================================================== */
/* AudioMgr_PlayEvent — Play audio narration event for a page          */
/* Address: 0x44F560                                                    */
/*                                                                      */
/* Plays the narration audio associated with the given page index.     */
/* If an existing narration is playing on the audio channel, it is     */
/* stopped first.                                                       */
/* ================================================================== */
void __thiscall AudioMgr_PlayEvent(void* _this, int pageIdx)
{
    if (pageIdx < 0 || pageIdx >= HELP_MAX_PAGES) {
        return;
    }

    HelpPageData* page = (HelpPageData*)((char*)_this + 0x15C + pageIdx * 0x3C);
    int soundResId = page->soundResId;

    if (soundResId == 0) {
        return;  /* No narration for _this page */
    }

    /* Get the audio channel pointer */
    int* channelPtr = (int*)((char*)_this + 0x158);  /* +0x158 = audio channel */

    /* Release existing narration if playing */
    if (*channelPtr != 0) {
        int sndId = *(int*)(*channelPtr + 0x38);  /* Sound resource ID from channel */
        if (sndId != 0) {
            int handle = ResourceManager_GetStringById(g_resmgr, (UINT)sndId);
            RESMGR_ReleaseSoundResource(handle);
        }
        AudioChannel_Release((void*)*channelPtr);
        *channelPtr = 0;
    }

    /* Load and play new narration */
    int handle = ResourceManager_GetStringById(g_resmgr, (UINT)soundResId);
    RESMGR_LoadSoundResource(handle);

    if (g_audio_mgr != NULL) {
        GameAudio_PlayResourceEx(g_audio_mgr, (UINT)soundResId, channelPtr);
    }
}
