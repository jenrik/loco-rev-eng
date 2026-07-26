/**
 * PlaySound.c — Sound playback convenience wrappers (C free functions)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Top-level sound playback functions. PlaySound plays at the global
 * listener position. PlaySoundAt uses caller-specified coordinates.
 * PlaySoundFile plays a raw .wav file from disk.
 *
 * All three use lazy loading: when a sound resource ID is first
 * requested, they call ResourceManager_LoadStringTable to load it,
 * and cache the result in a global array at 0x49161C.
 *
 * Global sound cache: g_sound_cache[resource_id] at 0x49161C
 *   Each entry is a void* (4 bytes):
 *     0 = not yet loaded
 *     -1 (0xFFFFFFFF) = load failed
 *     other = loaded resource pointer
 */

#include "../shared/types.h"

/* ================================================================== */
/* External declarations                                               */
/* ================================================================== */

struct GameAudio;

extern GameAudio* g_audio;                /* 0x4FD3BC */
extern void*      g_resmgr;               /* 0x4855E8 */
extern int32_t    g_listener_x;           /* 0x4AAD2C */
extern int32_t    g_listener_y;           /* 0x4AAD30 */

extern void* __thiscall ResourceManager_GetById(void* resmgr, uint32_t id);
extern int32_t __thiscall ResourceManager_LoadStringTable(void* resmgr,
                                                           uint32_t startId,
                                                           uint32_t endId);
extern void __fastcall GameAudio_AllocChannel(GameAudio* audio,
                                                int32_t res_ptr,
                                                void** output_ptr,
                                                int32_t pos_x, int32_t pos_y,
                                                uint32_t attenuation,
                                                uint8_t looping);

/* ================================================================== */
/* Sound resource cache array (0x4 bytes per entry x 0x1144 entries)  */
/* Located at 0x49161C. Indexed by resource ID.                        */
/* ================================================================== */
#define SOUND_CACHE_ADDR    0x0049161C
#define SOUND_CACHE_ENTRY(id)  (*(int32_t*)((int8_t*)SOUND_CACHE_ADDR + (id) * 4))

/* ================================================================== */
/* Internal: resolve sound resource ID to a loaded resource pointer.   */
/* Lazily loads the resource table on first access.                    */
/*                                                                     */
/* Returns: loaded resource pointer, or NULL on failure.               */
/* ================================================================== */
static int32_t resolve_sound_resource(uint32_t resourceId)
{
    /* Resource ID must be in range [0x5000, 0x605F] */
    if ((int32_t)resourceId < 0x5000 || (int32_t)resourceId > 0x605F) {
        *(int32_t*)0x4AA4AC = 1;  /* CRT_errno = 1 (EDOM) */
        return 0;
    }

    {
        int32_t* entry = (int32_t*)(SOUND_CACHE_ADDR + resourceId * 4);
        if (*entry == 0) {
            /* Not loaded — load the resource table */
            ResourceManager_LoadStringTable(g_resmgr, resourceId, resourceId);
            if (*entry == 0) {
                /* Load failed — mark as permanently failed */
                *entry = -1;
                *(int32_t*)0x4AA4AC = 2;  /* CRT_errno = 2 (ENOENT) */
            }
        }

        if (*entry == -1) {
            *(int32_t*)0x4AA4AC = 2;  /* CRT_errno = 2 (ENOENT) */
            return 0;
        }

        return *entry;
    }
}

/* ================================================================== */
/* PlaySound — 0x447930                                                 */
/*                                                                      */
/* Play a sound effect by resource ID at the global listener position. */
/* Uses attenuation type 4 (full volume), no looping.                  */
/*                                                                      */
/* Called by: Game_SelectGameObject, UI button click handlers, etc.    */
/* ================================================================== */
void __cdecl PlaySound(uint32_t resourceId)
{
    int32_t res_ptr = resolve_sound_resource(resourceId);

    if (g_audio != NULL && res_ptr != 0) {
        GameAudio_AllocChannel(g_audio, res_ptr, NULL,
                                g_listener_x, g_listener_y, 4, 0);
    }
}

/* ================================================================== */
/* PlaySoundAt — 0x4479D0                                               */
/*                                                                      */
/* Play a sound effect by resource ID at a caller-specified position.  */
/* Uses caller-specified attenuation type (param_4), no looping.       */
/*                                                                      */
/* @param param_1  Resource ID                                         */
/* @param param_2  X position                                          */
/* @param param_3  Y position                                          */
/* @param param_4  Attenuation curve type (1-4, default 4 = full)      */
/* ================================================================== */
void __cdecl PlaySoundAt(uint32_t resourceId, int32_t posX, int32_t posY, uint32_t attenuation)
{
    int32_t res_ptr = resolve_sound_resource(resourceId);

    if (g_audio != NULL) {
        GameAudio_AllocChannel(g_audio, res_ptr, NULL,
                                posX, posY, attenuation, 0);
    }
}

/* ================================================================== */
/* PlaySoundFile — 0x447A70                                             */
/*                                                                      */
/* Play a .wav file from disk at a specified position. Creates a temp  */
/* resource object from the file, loads it into DirectSound, plays it, */
/* then releases. SEH-protected for safety.                            */
/*                                                                      */
/* Called by: NETMAN_ReceivePlayerName (train arrival sound),          */
/*            Cursor_UploadCustomContent                               */
/* ================================================================== */
void __cdecl PlaySoundFile(const char* filePath, int32_t posX, int32_t posY, uint32_t attenuation)
{
    void* resource_obj;

    resource_obj = operator_new(300);  /* 0x12C bytes */
    if (resource_obj != NULL) {
        resource_obj = (*(void*(__thiscall**)(void*, const char*))
            (*(void**)resource_obj))(resource_obj, filePath);
    }

    if (g_audio != NULL && resource_obj != NULL) {
        uint32_t loadResult = *(uint32_t(__fastcall**)(int32_t))
            (0x448D60 /* RESMGR_LoadSoundResource */)((int32_t)resource_obj);
        if ((uint8_t)loadResult != 0) {
            GameAudio_AllocChannel(g_audio, (int32_t)resource_obj, NULL,
                                    posX, posY, attenuation, 0);
            *(void(__fastcall**)(int32_t))(0x448EE0 /* RESMGR_ReleaseSoundResource */)((int32_t)resource_obj);
        }
    }

    if (resource_obj != NULL) {
        (*(void(**)(void*, int))(*(void**)resource_obj))(resource_obj, 1);
    }
}
