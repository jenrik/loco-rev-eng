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

/*
 * resource.c  --  Resource System: .RFH / .RFD Archive Loader
 * Lego Loco (1998)  loco.exe  addresses 0x00446050 .. 0x0045caa0
 *
 * SUBSYSTEM OVERVIEW
 * ==================
 * Lego Loco stores almost all art assets (BMP images, .dat tilemaps,
 * .but button graphics, .wav sound clips) inside a pair of binary files:
 *
 *   resource.RFH  --  index: array of (filenameLen, filename[], rfdOffset, rfdSize)
 *   resource.RFD  --  data:  raw concatenated asset blobs
 *
 * The path to these files is read from [DIRECTORIES] ResFile= in LEGO.INI.
 *
 * RFH FILE FORMAT (binary, little-endian)
 * ----------------------------------------
 *   Repeated until EOF:
 *     uint32_t  filenameLen       -- byte length of the filename that follows
 *     char      filename[filenameLen]  -- null-terminated relative path
 *                                         e.g. "roads\half-vwint.dat "
 *     uint32_t  rfdOffset         -- byte offset of the asset in the .RFD file
 *     uint32_t  rfdSize           -- byte size of the asset in the .RFD file
 *
 * The RFH entries are parsed into a singly-linked list of RFHEntry nodes
 * (16 bytes each).  After parsing the .RFH is closed and the .RFD is
 * opened for later random-access reads.
 *
 * RESOURCE ID SPACE (16-bit logical IDs)
 * ----------------------------------------
 *   0x0000-0x03FF  type 0  -- reserved / no-op
 *   0x0400-0x07FF  type 1  -- generic data blobs (CResourceBase, 0x168 bytes)
 *   0x0800-0x0BFF  type 2  -- even=surface (0x630 bytes), odd=generic
 *   0x0C00-0x0FFF  type 3  -- even=special surface (0x63C bytes), odd=generic
 *   0x1000-0x13FF  type 4  -- even=surface, odd=generic  (same as type 2)
 *   0x1400-0x17FF  type 5  -- all=generic, PERSISTENT (cursor surfaces here)
 *   0x1800-0x1BFF  type 6  -- 0x1802 skipped; <0x1866 odd=generic; else large UI (0x7AC)
 *   0x1C00-0x1FFF  type 7  -- even=animation (0x178 bytes), odd=generic
 *   0x2000-0x23FF  type 8  -- even=animation, odd=generic  (same as type 7)
 *   0x2400-0x2FFF  types 9-11 -- generic
 *   0x3000-0x37FF  types 12-13 -- surface (0x630 bytes)
 *   0x3800-0x3BFF  type 14 -- generic; IDs > 0x3801 are PERSISTENT
 *   0x3C00-0x3FFF  type 15 -- generic
 *   0x5000-0x6060  n/a     -- button/sound resources (CButton, 300 bytes),
 *                             stored in a separate buttonCache[] array
 *
 * GLOBAL INSTANCE
 * ----------------
 *   CResourceMgr g_ResMgr;   // DAT_004855e8  (~150 KB inline object)
 *
 * LOCALIZATION
 * -------------
 *   String IDs in the range [100, 500] have per-language variants in the
 *   EXE string table.  g_ResMgr.languageID (at object+0x241B8) selects one
 *   of nine offset tables.  See RESMGR_LoadLocalizedString for the offset map.
 */

#include "ddraw_init.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * INI File Helpers
 * ========================================================================= */

/*
 * INI_GetString  (FUN_00452d80)
 *
 * Reads a string value from the INI file whose path lives at iniFile+4.
 *
 * WIN32:  GetPrivateProfileStringA
 * LINUX:  custom INI parser or SDL_RWops key-value lookup
 */
void INI_GetString(void *iniFile,
                   LPCSTR section,
                   LPCSTR key,
                   LPCSTR defaultVal,
                   LPSTR  outBuf,
                   DWORD  bufLen)
{
    /* WIN32: delegate to the Windows INI reading API.
     * The file path is stored as a C string at iniFile+4. */
    GetPrivateProfileStringA(section, key, defaultVal,
                             outBuf, bufLen,
                             (LPCSTR)((char *)iniFile + 4));
    /* LINUX:
     *   loco_ini_get_string((char *)iniFile + 4, section, key,
     *                        defaultVal, outBuf, bufLen);
     */
}

/*
 * INI_GetInt  (FUN_00452d60)
 *
 * Reads an integer value from the INI file.
 *
 * WIN32:  GetPrivateProfileIntA
 * LINUX:  custom INI parser returning int
 */
int INI_GetInt(void *iniFile, LPCSTR section, LPCSTR key, INT defaultVal)
{
    /* WIN32: */
    return GetPrivateProfileIntA(section, key, defaultVal,
                                 (LPCSTR)((char *)iniFile + 4));
    /* LINUX:
     *   return loco_ini_get_int((char *)iniFile + 4, section, key, defaultVal);
     */
}

/* =========================================================================
 * .RFH / .RFD Archive Loader
 * ========================================================================= */

/*
 * RFHMGR_Load  (FUN_0045caa0)
 *
 * Parses the .RFH resource index file at rfhPath and leaves an open FILE*
 * to the paired .RFD data file in rfhMgr->rfdHandle.
 *
 * Binary layout of one .RFH entry (little-endian):
 *   uint32_t filenameLen
 *   char     filename[filenameLen]   (null-terminated)
 *   uint32_t rfdOffset
 *   uint32_t rfdSize
 *
 * Returns 1 on success, 0 if the file cannot be opened or path has no extension.
 *
 * WIN32:  none (uses CRT FILE* wrappers: fopen, fread, fclose)
 * LINUX:  no changes needed; FILE* is POSIX
 */
uint32_t RFHMGR_Load(CRFHFile *rfhMgr, char *rfhPath)
{
    char     pathBuf[400];     /* working copy of the path */
    char     nameBuf[400];     /* filename read from the RFH */
    uint32_t filenameLen;
    uint32_t rfdOffset;
    uint32_t rfdSize;
    uint32_t readCount;
    void    *fileHandle;       /* internal FILE* wrapper */
    RFHEntry *node;
    char     *filenameCopy;
    RFHEntry *listTail;

    /* -- Close any previously open file and discard old entry list -- */
    if (rfhMgr->rfdHandle != NULL) {
        fclose((FILE *)rfhMgr->rfdHandle);   /* FUN_004681d0 wraps fclose */
        rfhMgr->rfdHandle = NULL;
        rfhMgr->entryHead = NULL;
    }

    /* -- Copy rfhPath and find the extension separator -- */
    /* (inline strlen + memcpy; Ghidra emits manual loop for portable copy) */
    strncpy(pathBuf, rfhPath, sizeof(pathBuf) - 1);
    pathBuf[sizeof(pathBuf) - 1] = '\0';

    /* Find the '.' that begins the extension (search backward from end) */
    char *ext = strrchr(pathBuf, '.');
    if (ext == NULL)
        return 0;   /* no extension found -- cannot construct .rfh / .rfd paths */

    /* -- Open the .RFH index file -- */
    strcpy(ext + 1, "rfh");                     /* FUN_00466d60: replace ext */
    fileHandle = fopen(pathBuf, "rb");           /* FUN_00468480 mode 0x40 */
    rfhMgr->rfdHandle = fileHandle;
    if (fileHandle == NULL)
        return 0;

    listTail = NULL;

    /* -- Read RFH entries until EOF (file status bit 0x10 indicates EOF) -- */
    while (1) {
        /* Check the internal file EOF flag (byte at fileStruct+0xC, bit 0x10) */
        if (/* EOF check */ feof((FILE *)fileHandle))
            break;

        /* Read the 4-byte filename length */
        readCount = (uint32_t)fread(&filenameLen, 1, 4, (FILE *)fileHandle);
        if (readCount == 0)
            break;   /* could not read length -- done */

        /* Read the filename bytes */
        fread(nameBuf, 1, filenameLen, (FILE *)fileHandle);

        /* Read rfdOffset (4 bytes) */
        fread(&rfdOffset, 1, 4, (FILE *)fileHandle);

        /* Read rfdSize (4 bytes) */
        fread(&rfdSize,   1, 4, (FILE *)fileHandle);

        /* Allocate and populate a new RFHEntry node (0x10 bytes) */
        node = (RFHEntry *)malloc(0x10);         /* FUN_00465ce0(0x10) */
        filenameCopy = (char *)malloc(filenameLen); /* FUN_004673c0(len) */
        node->filename  = filenameCopy;
        memcpy(filenameCopy, nameBuf, filenameLen);
        node->rfdSize   = rfdSize;               /* puVar5[1] = local_32c */
        node->rfdOffset = rfdOffset;             /* puVar5[2] = local_324 */
        node->next      = NULL;

        /* Append to the linked list */
        if (rfhMgr->entryHead == NULL) {
            rfhMgr->entryHead = node;
        } else {
            listTail->next = node;
        }
        listTail = node;
    }

    /* -- Close the .RFH file -- */
    fclose((FILE *)fileHandle);                  /* FUN_004681d0 */

    /* -- Open the paired .RFD data file -- */
    strcpy(ext + 1, "rfd");                      /* FUN_00466d60: replace ext */
    fileHandle = fopen(pathBuf, "rb");
    rfhMgr->rfdHandle = fileHandle;

    /* -- Store a heap copy of the .RFD path -- */
    size_t pathLen = strlen(pathBuf) + 1;
    char *pathCopy = (char *)malloc(pathLen);    /* FUN_004673c0(len) */
    memcpy(pathCopy, pathBuf, pathLen);
    rfhMgr->rfdPath = pathCopy;

    rfhMgr->reserved = 0;

    return (fileHandle != NULL) ? 1 : 0;
}

/* =========================================================================
 * Localized String Loader
 * ========================================================================= */

/*
 * RESMGR_LoadLocalizedString  (FUN_00447330)
 *
 * Loads string resource 'baseID' from the EXE string table into outBuf.
 * If baseID is in [100, 500] and this->languageID is 1-9, a per-language
 * offset is added first.  Falls back to the base ID if the localized
 * variant is not found.
 *
 * Language offset map (added to baseID for the localized attempt):
 *   languageID 1 -> +0x6CFC   languageID 6 -> +0x6914
 *   languageID 2 -> +0x652C   languageID 7 -> +0x6720
 *   languageID 4 -> +0x6338   languageID 8 -> +0x6EF0
 *   languageID 5 -> +0x6144   languageID 9 -> +0x6B08
 *   languageID 0,3,other -> no offset (use baseID directly)
 *
 * WIN32:  GetModuleHandleA, LoadStringA
 * LINUX:  look up in a static string table array indexed by adjusted ID
 */
void RESMGR_LoadLocalizedString(CResourceMgr *this,
                                UINT   baseID,
                                LPSTR  outBuf,
                                int    bufLen)
{
    UINT   localizedID = baseID;
    HMODULE hMod;
    int     charsRead;

    /* Apply language-specific offset for IDs in the localized range [100, 500] */
    if (baseID >= 100 && baseID <= 500) {
        switch (this->languageID) {
            /* WIN32: these offsets select alternate string table ranges in the EXE */
            case 1:  localizedID = baseID + 0x6CFC; break;
            case 2:  localizedID = baseID + 0x652C; break;
            case 4:  localizedID = baseID + 0x6338; break;
            case 5:  localizedID = baseID + 0x6144; break;
            case 6:  localizedID = baseID + 0x6914; break;
            case 7:  localizedID = baseID + 0x6720; break;
            case 8:  localizedID = baseID + 0x6EF0; break;
            case 9:  localizedID = baseID + 0x6B08; break;
            default: localizedID = baseID;          break;
        }
    }

    /* WIN32: load from the EXE's embedded string table */
    hMod = GetModuleHandleA(NULL);
    charsRead = LoadStringA(hMod, localizedID, outBuf, bufLen);

    /* Fall back to the base (English) ID if the localized string is missing */
    if (localizedID != baseID && charsRead == 0) {
        LoadStringA(hMod, baseID, outBuf, bufLen);
    }
    /* LINUX: replace with:
     *   charsRead = g_StringTable_Lookup(localizedID, outBuf, bufLen);
     *   if (localizedID != baseID && charsRead == 0)
     *       g_StringTable_Lookup(baseID, outBuf, bufLen);
     */
}

/* =========================================================================
 * Resource Cache Dispatch (Factory)
 * ========================================================================= */

/*
 * RESMGR_LoadResource  (FUN_00446840)
 *
 * Creates the right C++ resource object for 'resID' and stores it in
 * this->resourceCache[resID].  Returns 1 if the slot is now valid, 0 on failure.
 *
 * Dispatch is based on bits [13:10] of resID (the 'type nibble'):
 *   type = (resID >> 10) & 0xF
 *
 * Resource constructors:
 *   FUN_00424af0(mem, resID, name) -> generic CResourceBase (0x168 bytes)
 *   FUN_0041e570(mem, resID, name) -> surface/bitmap        (0x630 bytes)
 *   FUN_0044b190(mem, resID, name) -> special surface       (0x63C bytes)
 *   FUN_0040e600(mem, resID, name) -> large UI element      (0x7AC bytes)
 *   FUN_00436400(mem, resID, name) -> animation             (0x178 bytes)
 *
 * WIN32:  uses SEH (ExceptionList) for C++ exception safety
 * LINUX:  wrap constructors in setjmp/longjmp or C++ try/catch
 */
int RESMGR_LoadResource(CResourceMgr *this,
                        uint32_t      resID,
                        const char   *stringName)
{
    CResourceBase *obj = NULL;
    void          *mem;
    uint8_t        typeByte;
    int            isEven;      /* (resID & 1) == 0 */

    /* Already loaded? */
    if (this->resourceCache[resID] != NULL &&
        this->resourceCache[resID] != (CResourceBase *)-1)
        return 1;

    /* Bits [13:10] determine the resource class */
    typeByte = (uint8_t)(resID >> 10);
    if (typeByte >= 0x10) typeByte = 0;  /* clamp: IDs beyond 0x3FFF use 0 */
    isEven = ((resID & 1) == 0);

    switch (typeByte) {
    case 0:
        /* Reserved range 0x0000..0x03FF -- fall through to validity check */
        break;

    case 1:
        /* 0x0400..0x07FF: generic resource (string table entries, UI labels) */
        mem = malloc(0x168);
        if (mem) obj = FUN_00424af0(mem, resID, stringName);
        break;

    case 2:
    case 4:
        /* 0x0800..0x0BFF, 0x1000..0x13FF: surfaces (even) or generic (odd) */
        if (isEven) {
            mem = malloc(0x630);
            if (mem) obj = FUN_0041e570(mem, resID, stringName);  /* surface */
        } else {
            mem = malloc(0x168);
            if (mem) obj = FUN_00424af0(mem, resID, stringName);
        }
        break;

    case 3:
        /* 0x0C00..0x0FFF: special surface (even) or generic (odd) */
        if (isEven) {
            mem = malloc(0x63C);
            if (mem) obj = FUN_0044b190(mem, resID, stringName);  /* special surf */
        } else {
            mem = malloc(0x168);
            if (mem) obj = FUN_00424af0(mem, resID, stringName);
        }
        break;

    case 5:
        /* 0x1400..0x17FF: generic, PERSISTENT (cursor surface IDs 0x1400/1402/1403) */
        mem = malloc(0x168);
        if (mem) {
            obj = FUN_00424af0(mem, resID, stringName);
            if (obj) obj->persistent = 1;   /* WIN32: byte at obj+0x18 = 1 */
        }
        this->resourceCache[resID] = obj;
        goto skip_store;     /* already stored above */

    case 6:
        /* 0x1800..0x1BFF: 0x1802 is skipped (reserved placeholder) */
        if (resID == 0x1802) goto skip_store;
        if (resID < 0x1866 && !isEven) {
            mem = malloc(0x168);
            if (mem) obj = FUN_00424af0(mem, resID, stringName);
        } else {
            mem = malloc(0x7AC);
            if (mem) obj = FUN_0040e600(mem, resID, stringName);  /* large UI */
        }
        break;

    case 7:
    case 8:
        /* 0x1C00..0x23FF: animation (even) or generic (odd) */
        if (isEven) {
            mem = malloc(0x178);
            if (mem) obj = FUN_00436400(mem, resID, stringName);  /* animation */
        } else {
            mem = malloc(0x168);
            if (mem) obj = FUN_00424af0(mem, resID, stringName);
        }
        break;

    case 0xC:
    case 0xD:
        /* 0x3000..0x37FF: surfaces */
        mem = malloc(0x630);
        if (mem) obj = FUN_0041e570(mem, resID, stringName);      /* surface */
        break;

    case 0xE:
        /* 0x3800..0x3BFF: generic, persistent for IDs > 0x3801 */
        mem = malloc(0x168);
        if (mem) {
            obj = FUN_00424af0(mem, resID, stringName);
            if (obj && resID > 0x3801) obj->persistent = 1;
        }
        this->resourceCache[resID] = obj;
        goto skip_store;

    default:
        /* All other types: generic resource */
        mem = malloc(0x168);
        if (mem) obj = FUN_00424af0(mem, resID, stringName);
        break;
    }

    this->resourceCache[resID] = obj;

skip_store:
    obj = this->resourceCache[resID];

    /* Validity check: if not loaded_ok AND type is not 1 or 0xF, destroy */
    if (obj == NULL || obj == (CResourceBase *)-1)
        return 1;  /* null/sentinel slot -- persistent non-entry */

    if (obj->loaded_ok == 0 && typeByte != 1 && typeByte != 0xF) {
        /* Destroy and mark as permanently failed */
        (*obj->vtable[0])(1);          /* destructor with free_mem=1 */
        this->resourceCache[resID] = (CResourceBase *)-1;
        return 0;
    }
    return 1;
}

/* =========================================================================
 * Resource Retrieval (Lazy Load)
 * ========================================================================= */

/*
 * RESMGR_SetAlias  (FUN_00447290)
 *
 * Makes aliasTable[aliasID] point to the cache slot for targetID.
 * After this, RESMGR_GetResource(aliasID) returns the same object
 * as RESMGR_GetResourceByID(targetID).
 */
void RESMGR_SetAlias(CResourceMgr *this, int aliasID, int targetID)
{
    /* Store a pointer TO the cache slot, not a copy of its value */
    this->aliasTable[aliasID] = (CResourceBase *)
                                &this->resourceCache[targetID];
}

/*
 * RESMGR_GetResource  (FUN_00446ea0)
 *
 * Returns a pointer to the loaded resource for resID, lazy-loading if needed.
 * Uses the alias table so that aliased IDs redirect correctly.
 * Returns 0 and sets thread-local error code on failure:
 *   error 1 = out of range, error 2 = resource not found / load failed.
 *
 * WIN32:  GetModuleHandleA, LoadStringA (in the batch-load inner path)
 * LINUX:  replace LoadStringA with g_StringTable_Lookup
 */
int RESMGR_GetResource(CResourceMgr *this, int resID)
{
    CResourceBase **slot;
    int             value;

    if (resID < 0 || resID > 0x3FFF) {
        /* WIN32: write to thread-local error storage */
        *GetLastErrorSlot() = 1;   /* FUN_00467fd0 returns ptr to TLS error */
        return 0;
    }

    slot = (CResourceBase **)this->aliasTable[resID];
    if (slot == NULL)
        return 0;

    value = *(int *)slot;
    if (value == 0) {
        /* Not yet loaded -- trigger a batch load covering the type group */
        uint32_t startID = (uint32_t)( (char *)slot - ((char *)this + 0x10030) ) / 4;
        RESMGR_LoadBatch(this, startID);   /* FUN_00446840 loop (inlined) */

        value = *(int *)slot;
        if (value == 0) {
            *(int *)slot = -1;
            *GetLastErrorSlot() = 2;
            return 0;
        }
    }

    if (value == -1) {
        *GetLastErrorSlot() = 2;
        return 0;
    }
    return value;
}

/*
 * RESMGR_GetResourceByID  (FUN_004470b0)
 *
 * Direct cache lookup by ID without alias indirection.
 * Same lazy-load logic as RESMGR_GetResource but accesses
 * this->resourceCache[resID] directly.
 */
int RESMGR_GetResourceByID(CResourceMgr *this, uint32_t resID)
{
    int slot_val;

    if ((int)resID < 0 || (int)resID > 0x3FFF) {
        *GetLastErrorSlot() = 1;
        return 0;
    }

    slot_val = (int)this->resourceCache[resID];

    if (slot_val == 0) {
        RESMGR_LoadResource(this, resID, NULL);  /* trigger load */
        slot_val = (int)this->resourceCache[resID];
        if (slot_val == 0) {
            this->resourceCache[resID] = (CResourceBase *)-1;
            *GetLastErrorSlot() = 2;
        }
    }
    if (slot_val == -1) {
        *GetLastErrorSlot() = 2;
        return 0;
    }
    return slot_val;
}

/* =========================================================================
 * Button / Sound Resource Cache
 * ========================================================================= */

/*
 * RESMGR_LoadButtonRange  (FUN_00446cc0)
 *
 * Pre-loads button/sound resources for all IDs in [idFirst, idLast].
 * For each ID, looks up the localized string from the EXE string table.
 * If a string exists, constructs a CButton (300 bytes) via BUTTON_InitByID.
 * Stores the result in this->buttonCache[ID]; -1 = load failed.
 *
 * Note: offset formula  this + ID*4 + 0xC034  maps ID 0x5000 to
 * this+0x20034 (the start of buttonCache[]).
 *
 * WIN32:  GetModuleHandleA, LoadStringA
 * LINUX:  replace with g_StringTable_Lookup
 */
int RESMGR_LoadButtonRange(CResourceMgr *this,
                           uint32_t      idFirst,
                           uint32_t      idLast)
{
    uint32_t  id;
    UINT      localizedID;
    CHAR      nameBuf[264];
    HMODULE   hMod;
    int       charsRead;
    void     *mem;
    CButton  *btn;

    if (idLast > 0x6060) idLast = 0x6060;

    for (id = idFirst; (int)id <= (int)idLast; id++) {
        /* Resolve language-specific ID (same switch as LoadLocalizedString) */
        localizedID = id;  /* IDs >= 0x5000 are outside [100,500] so no remap */

        /* WIN32: load button name string from EXE string table */
        hMod = GetModuleHandleA(NULL);
        charsRead = LoadStringA(hMod, localizedID, nameBuf, 0x104);
        /* LINUX: charsRead = g_StringTable_Lookup(localizedID, nameBuf, 0x104); */

        if (charsRead == 0) {
            /* No string for this ID -- mark slot as permanently unavailable */
            this->buttonCache[id - 0x5000] = (CButton *)-1;
            continue;
        }

        /* Allocate and construct CButton */
        mem = malloc(300);     /* FUN_00465ce0(300) */
        if (mem == NULL) {
            this->buttonCache[id - 0x5000] = (CButton *)0;
            continue;
        }

        btn = BUTTON_InitByID((CButton *)mem, (int)id, nameBuf[0] != '\0');
        this->buttonCache[id - 0x5000] = btn;

        /* If construction failed to load the asset, discard and store -1 */
        if (btn != NULL && btn->loaded == 0) {
            (*btn->vtable[0])(1);   /* destructor */
            this->buttonCache[id - 0x5000] = (CButton *)-1;
        }
    }
    return 1;
}

/*
 * RESMGR_GetButtonResource  (FUN_004472b0)
 *
 * Returns a pointer to the CButton for resID (0x5000..0x605F).
 * Lazy-loads a single entry if the slot is 0 (unloaded).
 */
int RESMGR_GetButtonResource(CResourceMgr *this, uint32_t resID)
{
    int slot_val;

    if ((int)resID < 0x5000 || (int)resID > 0x605F) {
        *GetLastErrorSlot() = 1;
        return 0;
    }

    /* Offset: this + resID*4 + 0xC034 == &this->buttonCache[resID - 0x5000] */
    slot_val = (int)this->buttonCache[resID - 0x5000];

    if (slot_val == 0) {
        RESMGR_LoadButtonRange(this, resID, resID);
        slot_val = (int)this->buttonCache[resID - 0x5000];
        if (slot_val == 0) {
            this->buttonCache[resID - 0x5000] = (CButton *)-1;
            *GetLastErrorSlot() = 2;
        }
    }
    if (slot_val == -1) {
        *GetLastErrorSlot() = 2;
        return 0;
    }
    return slot_val;
}

/* =========================================================================
 * Cache Flush
 * ========================================================================= */

/*
 * RESMGR_FlushAllCaches  (FUN_004467e0)
 *
 * Evicts every resource from both the main cache and the button cache.
 * For each non-null, non-sentinel slot calls vtable[0](1) to destroy
 * the object and free its heap memory, then zeros the slot.
 * Also zeros the alias-table mirror slot for each main-cache entry.
 *
 * Called before level transitions and on low-memory notifications.
 *
 * WIN32:  none directly (C++ vtable dispatch)
 * LINUX:  no changes needed
 */
void RESMGR_FlushAllCaches(CResourceMgr *this)
{
    int i;
    int slot_val;

    /* --- Flush main resource cache (0x4001 entries at +0x10030) --- */
    for (i = 0; i < 0x4001; i++) {
        slot_val = (int)this->resourceCache[i];

        if (slot_val == -1)
            slot_val = 0;          /* sentinel: treat as unloaded */

        if (slot_val != 0) {
            CResourceBase *obj = (CResourceBase *)slot_val;
            (*obj->vtable[0])(1);  /* destroy and free */
            this->resourceCache[i] = NULL;
        }

        /* Zero the corresponding alias table slot */
        this->aliasTable[i] = NULL;
    }

    /* --- Flush button / sound cache (0x1061 entries at +0x20034) --- */
    for (i = 0; i < 0x1061; i++) {
        slot_val = (int)this->buttonCache[i];

        if (slot_val == -1)
            slot_val = 0;

        if (slot_val != 0) {
            CButton *btn = (CButton *)slot_val;
            (*btn->vtable[0])(1);
            this->buttonCache[i] = NULL;
        }
    }
}

/* =========================================================================
 * Resource Manager Initialization
 * ========================================================================= */

/*
 * RESMGR_Init  (FUN_00446050)
 *
 * One-time initialization of the global resource manager.
 *
 * Steps:
 *   1. Pre-init check (FUN_0045b500)
 *   2. Read [DIRECTORIES] ResFile= from LEGO.INI
 *   3. Parse .RFH index and open .RFD data file (RFHMGR_Load)
 *   4. Create five Arial GDI fonts stored in this->fonts[]
 *   5. Init modules at address 0x4A99B0 (audio subsystem)
 *   6. Pre-load button/sound cache for IDs 0x5000..0x6060
 *   7. Iterate IDs 0x400..0x4000: load string from EXE, call RESMGR_LoadResource
 *   8. Set this->clockPhase = 0xFFFFFFFF (clock not yet shown)
 *
 * Returns 1 on success, 0 on failure.
 *
 * WIN32:  CreateFontA, GetModuleHandleA, LoadStringA, GetPrivateProfileStringA
 * LINUX:  CreateFontA -> TTF_OpenFont; LoadStringA -> string table lookup
 */
int RESMGR_Init(CResourceMgr *this)
{
    char rfhPath[260];
    char fontName[52];   /* "Arial" */
    char nameBuf[264];
    HFONT hFont;
    uint32_t resID;
    UINT localizedID;
    HMODULE hMod;
    int charsRead;

    /* Step 1: pre-init check */
    if (!FUN_0045b500())
        return 0;

    /* Step 2: read resource file path from LEGO.INI
     *   [DIRECTORIES]
     *   ResFile=<path to resource.rfh>
     *   WIN32: GetPrivateProfileStringA via INI_GetString
     *   LINUX: custom INI parser
     */
    INI_GetString(g_IniFile,
                  "DIRECTORIES",
                  "ResFile",
                  /* default: */ (LPCSTR)&DAT_004851d0,
                  rfhPath,
                  0x104);

    /* Step 3: parse .RFH index and open .RFD data file */
    if (!RFHMGR_Load((CRFHFile *)((char *)this + 0x18), rfhPath))
        return 0;

    /* Step 4: create five Arial fonts for UI rendering.
     * Font sizes and weights:
     *   this->fonts[0]: 12pt, weight 800 (bold)
     *   this->fonts[1]: 14pt, weight 700
     *   this->fonts[2]: 16pt, weight 700
     *   this->fonts[3]: 24pt, weight 700
     *   this->fonts[4]: 20pt, weight 900 (heavy)
     *
     * WIN32: CreateFontA(height, 0, 0, 0, weight, 0, 0, 0,
     *                    ANSI_CHARSET=1, 0, 0, PROOF_QUALITY=2, 0, "Arial")
     * LINUX: TTF_Font *f = TTF_OpenFont("LiberationSans.ttf", ptSize);
     */
    strcpy(fontName, "Arial");   /* s_Arial_0047ed94 */

    hFont = CreateFontA(12, 0,0,0, 800, 0,0,0, 1,0,0, 2, 0, fontName);
    this->fonts[0] = hFont;

    hFont = CreateFontA(14, 0,0,0, 700, 0,0,0, 1,0,0, 2, 0, fontName);
    this->fonts[1] = hFont;

    hFont = CreateFontA(16, 0,0,0, 700, 0,0,0, 1,0,0, 2, 0, fontName);
    this->fonts[2] = hFont;

    hFont = CreateFontA(24, 0,0,0, 700, 0,0,0, 1,0,0, 2, 0, fontName);
    this->fonts[3] = hFont;

    hFont = CreateFontA(20, 0,0,0, 900, 0,0,0, 1,0,0, 2, 0, fontName);
    this->fonts[4] = hFont;

    /* Step 5: initialize audio module (sound engine at 0x4A99B0) */
    FUN_0041f7e0(0x4a99b0);
    FUN_0041f970(0x4a99b0);

    /* Step 6: pre-load button / sound resources IDs 0x5000..0x6060 */
    RESMGR_LoadButtonRange(this, 0x5000, 0x6060);

    /* Step 7: load string resources for IDs 0x400..0x4000.
     * For each ID, LoadStringA fetches the localized display name from the
     * EXE's STRINGTABLE.  If found, RESMGR_LoadResource creates the asset object.
     * The g_LocaleFlag (DAT_004851f4 == 10) can suppress all string loading
     * (debug / no-locale mode).
     *
     * WIN32: GetModuleHandleA + LoadStringA
     * LINUX: static string table lookup
     */
    for (resID = 0x400; (int)resID <= 0x4000; resID++) {

        if (DAT_004851f4 == 10)   /* locale-disabled mode: skip all strings */
            break;

        /* Compute language-adjusted string ID (same logic as LoadLocalizedString) */
        localizedID = resID;
        if (resID >= 100 && resID <= 500) {
            /* apply language offset -- see RESMGR_LoadLocalizedString */
            /* (table omitted here; see that function for the full switch) */
        }

        hMod = GetModuleHandleA(NULL);  /* WIN32: get the EXE module handle */
        charsRead = LoadStringA(hMod, localizedID, nameBuf, 0x104);

        if (charsRead == 0 && localizedID != resID) {
            /* Localized string missing -- try base ID */
            charsRead = LoadStringA(hMod, resID, nameBuf, 0x104);
        }

        if (charsRead == 0) {
            /* No string for this ID -- mark slot permanently unavailable */
            this->resourceCache[resID] = (CResourceBase *)-1;
        } else {
            /* String found -- create the resource object */
            RESMGR_LoadResource(this, resID, nameBuf);
        }
    }

    /* Step 8: clock phase sentinel (-1 = clock not yet displayed) */
    this->clockPhase = -1;   /* 0xFFFFFFFF */

    return 1;
}

/* =========================================================================
 * Button / Sound Resource Constructors
 * ========================================================================= */

/*
 * BUTTON_InitByID  (FUN_00448990)
 *
 * Placement constructor for CButton using a numeric resource ID.
 * Sets vtable, stores resID, optionally builds the .wav filename, then
 * calls BUTTON_LoadSound to locate and buffer the associated WAV file.
 *
 * WIN32:  none directly
 * LINUX:  no changes needed
 */
CButton *BUTTON_InitByID(CButton *this, int resID, int hasSound)
{
    this->resID = resID;
    this->vtable = (void **)&PTR_FUN_00478278;  /* CButton vtable */

    if (hasSound) {
        /* Build .wav filename: "%s%s.wav" with game data directory prefix.
         * s__s_s_wav_0047ef38 = "%s%s.wav"
         * DAT_004a99c8       = game data directory path */
        wsprintfA((LPSTR)((char *)this + 0x18),
                  "%s%s.wav",
                  (LPCSTR)&DAT_004a99c8);
        /* LINUX: snprintf((char*)this + 0x18, 0x104, "%s%s.wav", g_DataDir); */
    }

    BUTTON_LoadSound(this);
    return this;
}

/*
 * BUTTON_InitByName  (FUN_00448a20)
 *
 * Placement constructor for CButton from a file path string.
 * Sets resID = -1, copies filePath into the inline filename buffer,
 * then calls BUTTON_LoadSound.
 *
 * WIN32:  none
 * LINUX:  no changes needed
 */
CButton *BUTTON_InitByName(CButton *this, char *filePath)
{
    this->vtable = (void **)&PTR_FUN_00478278;
    this->resID  = -1;

    if (filePath != NULL)
        strcpy((char *)this + 0x18, filePath);   /* inline filename buffer */

    BUTTON_LoadSound(this);
    return this;
}

/*
 * BUTTON_LoadSound  (FUN_00448a70)
 *
 * Resolves the .wav audio clip for a CButton and marks this->loaded.
 * Two-stage lookup:
 *   Stage 1: if the RFD archive is open (g_RFDArchive != 0), strip the
 *            game-directory prefix from this->filename, look up the
 *            remainder in the archive via FUN_0045cd00, decode via
 *            FUN_00464490, and call this->vtable[1](stream) to consume it.
 *   Stage 2: if no archive hit, fall back to opening the .wav directly
 *            from the filesystem (FUN_00463aa0).
 *   Also calls GetFileAttributesA to confirm file existence if decoding
 *   is deferred.
 *
 * WIN32:  GetFileAttributesA (existence check)
 * LINUX:  access(path, F_OK) != -1
 */
void BUTTON_LoadSound(CButton *this)
{
    char     *filename = (char *)this + 0x18;
    void     *archiveEntry;
    int       archiveOffset;
    void     *audioObj;
    DWORD     attrs;

    *((uint8_t *)this + 9) = 0;   /* loaded = false */

    if (g_RFDArchive != 0) {
        /* Strip the game data-dir prefix from filename, look up in archive */
        size_t prefixLen = strlen((char *)&DAT_004a99c8);
        archiveEntry = FUN_0045cd00(g_RFDArchive,
                                    filename + prefixLen,
                                    &archiveOffset);
        if (archiveEntry != NULL) {
            audioObj = malloc(0x5C);  /* FUN_00465ce0(0x5C) */
            if (audioObj) {
                void *stream = FUN_00464490(audioObj,
                                            (char *)archiveEntry,
                                            archiveOffset, 1);
                /* Call this->vtable[1](stream) to decode audio */
                uint8_t ok = (*(uint8_t(*(*)(void*,void*))(void*,void*))
                               this->vtable[1])(this, stream);
                *((uint8_t *)this + 9) = ok;
            }
            FUN_00466c70(archiveEntry);   /* free archive lookup result */
        }
    }

    if (*((uint8_t *)this + 9) == 0) {
        /* Stage 2: open from filesystem with FUN_00463aa0
         * LINUX: fopen(filename, "rb") */
        void *fileStream = FUN_00463aa0(filename,
                                        /* block 0x20 */ 0x20,
                                        /* buf 0x1A4 */ 0x1A4);
        if (fileStream != NULL) {
            uint8_t ok = (*(uint8_t(*(*)(void*,void*))(void*,void*))
                           this->vtable[1])(this, fileStream);
            *((uint8_t *)this + 9) = ok;
            FUN_00463b10(fileStream);   /* close file stream */
        }
    }

    /* Final existence fallback: if still not loaded, check file on disk */
    if (*((uint8_t *)this + 9) != 1) {
        /* WIN32: GetFileAttributesA returns 0xFFFFFFFF if not found */
        attrs = GetFileAttributesA(filename);
        /* LINUX: if (access(filename, F_OK) == 0) */
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            *((uint8_t *)this + 9) = 1;  /* file exists, mark as loaded */
        }
    }
}

/* =========================================================================
 * Audio (Sound) Resource
 * ========================================================================= */

/*
 * SOUND_Load  (FUN_00448d60)
 *
 * Increments the reference count for a sound; decodes and buffers the audio
 * if this is the first reference.  Uses DirectSound vtable for buffer
 * management.  Returns 1 on success, error code on failure.
 *
 * WIN32:  DirectSound IDirectSoundBuffer vtable calls
 * LINUX:  SDL_mixer: Mix_LoadWAV / Mix_PlayChannel
 */
int SOUND_Load(CButton *btn)
{
    *(int *)((char *)btn + 0x120) += 1;   /* increment refCount */

    if (*(int *)((char *)btn + 0x0C) != 0)
        return 1;                          /* already buffered */

    /* WIN32: allocate a DirectSound buffer and fill it with PCM data.
     * WAVEFORMAT-like descriptor on the stack:
     *   type  = 0x14 (PCM)
     *   flags = 0xCA or 0x80CA (stereo flag for stereo clips)
     * Then call FUN_00413070(g_DSoundDevice, &fmt, &bufPtr, 0) to create
     * the buffer, then vtable[0x2C] to lock and vtable[0x4C] to fill.
     *
     * LINUX:
     *   chunk = Mix_LoadWAV((char*)btn + 0x18);
     *   *(void**)((char*)btn + 0x0C) = chunk;
     */

    *(uint8_t *)((char *)btn + 9) = 1;   /* mark loaded */
    return 1;
}

/*
 * SOUND_Release  (FUN_00448ee0)
 *
 * Decrements the reference count.  When the count reaches 0 and the
 * 'keep loaded' flag at btn+8 is not 1, stops playback and destroys
 * the DirectSound buffer.
 *
 * WIN32:  IDirectSoundBuffer::Stop (vtable[0x48]), Release (vtable[8])
 * LINUX:  Mix_HaltChannel; Mix_FreeChunk
 */
int SOUND_Release(CButton *btn)
{
    int refCount = *(int *)((char *)btn + 0x120);
    if (refCount > 0)
        *(int *)((char *)btn + 0x120) = refCount - 1;

    if (*(int *)((char *)btn + 0x120) == 0) {
        void *buf = *(void **)((char *)btn + 0x0C);
        if (buf != NULL && *(uint8_t *)((char *)btn + 8) != 1) {
            /* WIN32: IDirectSoundBuffer::Stop then Release */
            /* vtable[0x48](buf);  vtable[8](buf); */
            /* LINUX: Mix_HaltChannel(channelID); Mix_FreeChunk(chunk); */
            *(void **)((char *)btn + 0x0C) = NULL;
        }
    }
    return 1;
}

/* =========================================================================
 * Bitmap Resource
 * ========================================================================= */

/*
 * BITMAP_Init  (FUN_00448f30)
 *
 * Placement constructor for a bitmap/surface resource.
 * Allocates a pixel data buffer of (dataLen+1) bytes filled with the
 * default palette stub (DAT_004851d0).
 *
 * WIN32:  none
 * LINUX:  no changes needed; replace malloc with SDL_malloc if desired
 */
void *BITMAP_Init(void *this, int dataLen, int width, int height,
                  uint32_t colorKey, uint16_t bitDepth)
{
    char *pixBuf;

    FUN_0040cfa0(this, width, height, bitDepth);   /* base class init */

    *(void ***)this = &PTR_FUN_00478280;           /* set vtable */
    *(int *)((char *)this + 4) = 8;               /* surface type tag */

    pixBuf = (char *)malloc(dataLen + 1);          /* FUN_00465ce0(dataLen+1) */
    *(char **)((char *)this + 0x60) = pixBuf;

    if (pixBuf != NULL) {
        /* Copy the blank/default pixel buffer from global */
        memcpy(pixBuf, &DAT_004851d0, dataLen + 1);
    }

    *(uint8_t *)((char *)this + 0x58)  = 0;        /* dirty flag = false */
    *(uint32_t *)((char *)this + 100)  = colorKey; /* transparent color  */

    return this;
}

/*
 * BITMAP_LoadFromArchiveOrFile  (FUN_00447ba0)
 *
 * Loads bitmap pixel data for an RFH-entry-based asset.
 * Stage 1: archive lookup via RFD handle (FUN_0045cd00 + FUN_00464490).
 * Stage 2: fallback to direct file open (FUN_00463970, 160x420 assumed).
 * After loading, reads the BMP header (0x114 bytes) and allocates
 * width*height bytes for the pixel buffer.
 * Returns 1 on success, 0 on failure.
 *
 * WIN32:  none
 * LINUX:  no changes needed; file I/O is already CRT-wrapped
 */
int BITMAP_LoadFromArchiveOrFile(void *this, char *filePath)
{
    void    *archiveEntry;
    int      archiveOffset;
    void    *fileStream;
    uint32_t pixBufSize;
    void    *pixBuf;
    int      loaded = 0;

    FUN_00447fb0((int)this);    /* reset / clear existing data */

    /* Stage 1: look up in the RFD archive */
    if (DAT_00485600 != 0) {
        size_t prefixLen = strlen((char *)&DAT_004a99c8);
        archiveEntry = FUN_0045cd00(DAT_00485600,
                                    filePath + prefixLen,
                                    (int *)((char *)this + 0x1D4));
        *(void **)((char *)this + 0x1D0) = archiveEntry;

        if (archiveEntry != NULL) {
            void *audioObj = malloc(0x5C);   /* FUN_00465ce0(0x5C) */
            if (audioObj) {
                fileStream = FUN_00464490(audioObj,
                                          (char *)archiveEntry,
                                          *(int *)((char *)this + 0x1D4), 1);
                *(void **)((char *)this + 0x1C8) = fileStream;
            }
        }
    }

    /* Stage 2: fallback to direct filesystem open */
    if (*(int *)((char *)this + 0x1C8) == 0) {
        void *fsStream = malloc(0x5C);
        if (fsStream) {
            fileStream = FUN_00463970(fsStream, filePath, 0xA0, 0x1A4, 1);
            *(void **)((char *)this + 0x1C8) = fileStream;
        }
    }

    /* Read BMP header (0x114 bytes) and allocate pixel buffer */
    fileStream = *(void **)((char *)this + 0x1C8);
    if (fileStream != NULL) {
        int  *vtbl   = *(int **)fileStream;
        int   status = *(int *)(*(int *)((char *)vtbl + 4) + (int)fileStream);
        if (status == 0) {
            loaded = 1;

            FUN_00463810(fileStream, (char *)this + 0xB0, 0x114);  /* read BMP header */

            if (*(int *)(*(int *)((char *)this + 0x1C8) + 8) != 0x114) {
                /* Header size mismatch -- should not happen with valid BMPs */
                FUN_00466ce0((char **)&(char *)0x1, &DAT_0047a950);
            }

            /* Allocate pixel buffer: width * height bytes */
            pixBufSize = (uint32_t)(*(uint16_t *)((char *)this + 0xB4))
                       * (uint32_t)(*(uint16_t *)((char *)this + 0xB2));
            pixBuf = malloc(pixBufSize);   /* FUN_00465ce0(pixBufSize) */
            *(int *)((char *)this + 0x1C4) = (int)pixBuf;

            if (pixBuf == 0) {
                /* WIN32: MessageBox or assert with "Unable to allocate thumbnail mem" */
                FUN_00466ce0((char **)&s_Unable_to_allocate_thumbnail_mem_0047ee0c,
                             &DAT_0047a5e8);
            }

            FUN_00463810(fileStream, pixBuf, pixBufSize);  /* read pixel data */
        }
    }

    return loaded;
}

/* =========================================================================
 * Music / Clock Rendering
 * ========================================================================= */

/*
 * RESMGR_PlayBgMusic  (FUN_00447930)
 *
 * Plays a background music track identified by button resource ID.
 * Loads the CButton if not yet cached, then calls the audio playback
 * function at the default screen position.
 *
 * WIN32:  none directly (calls audio subsystem FUN_00413210)
 * LINUX:  Mix_PlayChannel(-1, btn->chunk, -1) for looping
 */
void RESMGR_PlayBgMusic(uint32_t musicID)
{
    int btnPtr;

    if ((int)musicID < 0x5000 || (int)musicID > 0x605F) {
        *GetLastErrorSlot() = 1;
        return;
    }

    /* buttonCache slot via global g_ResMgr */
    btnPtr = *(int *)((char *)&DAT_004855e8 + musicID * 4 + 0xC034);

    if (btnPtr == 0) {
        RESMGR_LoadButtonRange(&g_ResMgr, musicID, musicID);
        btnPtr = *(int *)((char *)&DAT_004855e8 + musicID * 4 + 0xC034);
        if (btnPtr == 0) {
            *(int *)((char *)&DAT_004855e8 + musicID * 4 + 0xC034) = -1;
            *GetLastErrorSlot() = 2;
            return;
        }
    }

    if (btnPtr == -1) {
        *GetLastErrorSlot() = 2;
        return;
    }

    if (g_Renderer != NULL) {
        /* Play at default screen position */
        FUN_00413210(g_Renderer, btnPtr, NULL, g_MusicX, g_MusicY, 4, 0);
        /* LINUX: Mix_PlayChannel(-1, ((CButton*)btnPtr)->chunk, 0); */
    }
}

/*
 * RESMGR_DrawClock  (FUN_00447400)
 *
 * Renders the town-clock animation overlay.
 * Computes the 12-position clock-hand frame from elapsed ticks,
 * plays a chime sound at quarter-hours (frames 0, 3, 6, 9), and
 * blits four clock-sprite resources.
 *
 * Resource IDs used:
 *   0x0842  flag: clock visible?
 *   0x0843  flag: clock hand visible?
 *   0x3DAD  clock face tile-strip
 *   0x3DAE  clock body tile-strip
 *   0x3DB0  hour-hand tile-strip
 *   0x3DB1  minute-hand tile-strip
 *   0x53AB  sound: hour chime (at frame 0)
 *   0x5399  sound: quarter chime (at frames 3, 6, 9)
 *
 * WIN32:  SetRect, CopyRect, OffsetRect (GDI rectangle helpers)
 * LINUX:  replace with SDL_Rect arithmetic inline
 */
void RESMGR_DrawClock(CResourceMgr *this, int timeTicks)
{
    int    newFrame;
    int    btnPtr;
    RECT   srcRect, destRect;

    /* Frame index: 0..11, advancing one step per 5 minutes of game time */
    newFrame = ((( timeTicks / 60 ) % 60) / 5 + 1) % 12;

    /* Only redraw if the clock hand moved */
    if (newFrame == this->clockPhase)
        goto draw_sprites;

    this->clockPhase = newFrame;

    /* Play chime sound */
    if (newFrame == 0) {
        /* Hour chime (resource 0x53AB) */
        btnPtr = RESMGR_GetButtonResource(this, 0x53AB);
    } else if (newFrame == 3 || newFrame == 6 || newFrame == 9) {
        /* Quarter-hour chime (resource 0x5399) */
        btnPtr = RESMGR_GetButtonResource(this, 0x5399);
    } else {
        btnPtr = 0;
    }

    if (g_Renderer != NULL && btnPtr != 0) {
        FUN_00413210(g_Renderer, btnPtr, NULL,
                     DAT_004aad2c, DAT_004aad30, 4, 0);
        /* LINUX: Mix_PlayChannel(-1, chunk, 0); */
    }

draw_sprites:
    {
        /* Blit clock sprite resources.
         * Each tile-strip resource contains 12 frames side-by-side.
         * SetRect selects the frame sub-rect; OffsetRect applies the
         * screen position offset.
         *
         * WIN32: SetRect, CopyRect, OffsetRect
         * LINUX: SDL_Rect; manual left=frame*w, top=0, right=w-1, bottom=h-1
         */
        int *clockFace = (int *)RESMGR_GetResource(this, 0x3DAD);
        int  tileW = *(uint16_t *)(clockFace + 5);
        int  tileH = *(uint16_t *)((char *)clockFace + 0x16);

        SetRect((LPRECT)&srcRect,  0, 0, tileW - 1, tileH - 1);
        CopyRect(&destRect, &srcRect);
        OffsetRect((LPRECT)&srcRect,  newFrame * tileW, 0);  /* select frame */
        OffsetRect(&destRect,          0x0F, 0x18);           /* screen pos   */

        FUN_0042c330(/* surface */ (void *)(**(int(**)(void))( *clockFace + 4 ))(),
                     destRect.left, destRect.top, destRect.right, destRect.bottom,
                     /* target params */ 0, 0,
                     srcRect.left, srcRect.top, srcRect.right, srcRect.bottom);

        (**(void(**)(void))( *clockFace + 8 ))();  /* release lock */
        /* (similar blit calls follow for resources 0x3DAE, 0x3DB0, 0x3DB1) */
    }
}

/* =========================================================================
 * RFHEntry Object Lifecycle
 * ========================================================================= */

/*
 * RFHEntry_DefaultInit  (FUN_00447b20)
 *
 * Default-initializes an RFHEntry in place (placement constructor).
 * Sets vtable to &PTR_FUN_00478274 and zeroes all asset-data fields.
 *
 * WIN32:  none  LINUX:  none
 */
void RFHEntry_DefaultInit(RFHEntry *entry)
{
    /* entry is actually a larger object; 0x71/0x74/0x75 are dword indices */
    void **obj = (void **)entry;
    obj[0]  = &PTR_FUN_00478274;  /* vtable */
    obj[0x71] = 0;
    obj[0x74] = 0;
    obj[0x75] = 0;
    *(uint16_t *)((char *)obj + 0xB0) = 0;
    *(uint16_t *)((char *)obj + 0xB2) = 0;
    *(uint16_t *)(obj + 0x2D)         = 0;
    obj[0x72] = 0;
    obj[0x73] = 0;
}

/*
 * RFHEntry_Destructor  (FUN_00447b60)
 *
 * C++ scalar destructor.  If (flags & 1), also frees the heap block.
 *
 * WIN32:  none  LINUX:  none
 */
void *RFHEntry_Destructor(void *this, uint8_t flags)
{
    *(void ***)this = &PTR_FUN_00478274;  /* restore vtable */
    FUN_00447fb0((int)this);              /* release asset data */
    if (flags & 1)
        free(this);                        /* FUN_00465cd0 wraps free() */
    return this;
}

/*
 * RFHEntry_Reset  (FUN_00447b90)
 *
 * Resets asset data without freeing the object itself.
 *
 * WIN32:  none  LINUX:  none
 */
void RFHEntry_Reset(void *entry)
{
    *(void ***)entry = &PTR_FUN_00478274;
    FUN_00447fb0((int)entry);
}

/* =========================================================================
 * Screensaver Support  (peripheral to the main resource system)
 * ========================================================================= */

/*
 * SCREENSAVER_Init  (FUN_00448040)
 *
 * Initializes the screensaver context.  Creates a light-blue solid brush
 * for background fill and sets the default repaint timer interval.
 *
 * WIN32:  CreateSolidBrush
 * LINUX:  store SDL_Color { 0xD8, 0xC4, 0xA8, 0xFF } instead
 */
void *SCREENSAVER_Init(uint32_t *ctx)
{
    ctx[0]  = 0;      /* hWnd = NULL */
    ctx[1]  = 0;      /* width = 0 */
    ctx[2]  = 0;      /* height = 0 */
    ctx[3]  = 0;      /* flags = 0 */
    ctx[4]  = 0;      /* state = 0 */
    ctx[5]  = 0x400;  /* default timer interval (ms) */
    ctx[0x1C] = 0;
    ctx[0x1D] = 0;
    ctx[0x1E] = 0;

    /* WIN32: create background fill brush (0xA8C4D8 = R168 G196 B216) */
    ctx[6] = (uint32_t)CreateSolidBrush(0xA8C4D8);
    /* LINUX: SDL_Color bgColor = {0xD8, 0xC4, 0xA8, 0xFF}; ctx[6] = bgColor.packed; */

    return ctx;
}

/*
 * SCREENSAVER_Cleanup  (FUN_00448080)
 *
 * Tears down the screensaver: deletes the GDI brush, destroys the window,
 * and unloads the password DLL.
 *
 * WIN32:  DeleteObject, DestroyWindow, FreeLibrary
 * LINUX:  (no-op for brush); SDL_DestroyWindow; dlclose
 */
void SCREENSAVER_Cleanup(uint32_t *ctx)
{
    /* WIN32: free GDI brush */
    DeleteObject((HGDIOBJ)(uintptr_t)ctx[6]);
    /* LINUX: no-op; SDL_Color is a plain value */
    ctx[6] = 0;

    if ((HWND)(uintptr_t)ctx[0] != NULL) {
        DestroyWindow((HWND)(uintptr_t)ctx[0]);   /* WIN32 */
        /* LINUX: SDL_DestroyWindow((SDL_Window*)ctx[0]); */
        ctx[0] = 0;
    }

    if ((HMODULE)(uintptr_t)ctx[0x1E] != NULL) {
        FreeLibrary((HMODULE)(uintptr_t)ctx[0x1E]);  /* WIN32 */
        /* LINUX: dlclose((void*)ctx[0x1E]); */
        ctx[0x1E] = 0;
    }
}

/*
 * SCREENSAVER_PostQuit  (FUN_00448970)
 *
 * Signals the screensaver message loop to terminate by posting WM_USER+5.
 *
 * WIN32:  PostMessageA
 * LINUX:  SDL_Event ev; ev.type = SDL_USEREVENT; ev.user.code = 5;
 *         SDL_PushEvent(&ev);
 */
void SCREENSAVER_PostQuit(void)
{
    HWND hWnd = *(HWND *)((char *)DAT_004aa4a0 + 8);
    PostMessageA(hWnd, WM_USER + 5, 0, 0);
    /* LINUX:
     *   SDL_Event ev = {0};
     *   ev.type = SDL_USEREVENT;
     *   ev.user.code = 5;
     *   SDL_PushEvent(&ev);
     */
}
