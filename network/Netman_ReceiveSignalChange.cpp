
// Status: TRANSCRIBED
class PlayerConfig;
extern PlayerConfig* g_player_config;
/**
 * Netman_ReceiveSignalChange.cpp — NETMAN_ReceiveSignalChange implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NETMAN_ReceiveSignalChange — Resolve remote player address from PostBag
 * files. Called by SendSignalChange to resolve a DPlayData player name
 * to a full DPlayData struct with routing information.
 *
 * Address: 0x43E900
 * Calling convention: __stdcall (1 DWORD parameter, RET 4)
 *
 * The function:
 * 1. Enumerates DPLAY players, matches player name against param_1+0x10
 * 2. Reads route file from PostBag (via NET_SendFile + CreateFileA)
 * 3. Picks a random route entry from the file
 * 4. Reads address file, picks a random address
 * 5. Resolves the address via NET_ResolveAddress
 * 6. Copies player data (session block + name) into resolved DPlayData
 * 7. Calls DPLAY_SetPlayerName to finalize
 * 8. Returns the resolved DPlayData pointer
 *
 * Only 1 actual parameter is used despite the large stack frame
 * (0x8f98 bytes for string buffers and file I/O).
 */

#include "Netman.h"
#include "DPlayManager.h"
#include "../game/PlayerConfig.h"
#include <cstring>
#include <new>

/* ================================================================== */
/* NETMAN_ReceiveSignalChange — 0x43E900                               */
/* ================================================================== */

void* __stdcall NETMAN_ReceiveSignalChange(void* playerDPlayData)
{
#ifndef _WIN32
    // The original 0x43E900 selects PostBag route/address files and resolves a
    // .crd player slot. SDL_net peers already supply the typed slot in 0x3EC;
    // clone its logical state instead of re-entering the local filesystem ABI.
    const auto* source = static_cast<const DPlayManager*>(playerDPlayData);
    if (source == nullptr) return nullptr;
    void* storage = operator_new(sizeof(DPlayManager));
    if (storage == nullptr) return nullptr;
    auto* resolved = ::new (storage) DPlayManager;
    resolved->CreatePlayer();
    resolved->CopyLogicalStateFrom(*source);
    std::memcpy(resolved->m_sessionBlk2, source->m_sessionBlk1,
                sizeof(resolved->m_sessionBlk2));
    resolved->m_wordValue = 0;
    resolved->m_dwordValue = 1;
    if (resolved->m_playerName[0] == '\0' && g_player_config != nullptr) {
        std::strncpy(resolved->m_playerName, g_player_config->name,
                     sizeof(resolved->m_playerName) - 1);
        resolved->m_playerName[sizeof(resolved->m_playerName) - 1] = '\0';
    }
    resolved->SetPlayerName(1, -1);
    return resolved;
#else
    /*
     * Stack layout (approximate, frame size = 0x8f98):
     *   local_390[128]   — player index string buffer (CRT_itoa output)
     *   local_590[0x504] — path buffer 1 (route file path, ~1284 bytes)
     *   local_a94[0x504] — path buffer 2 (address file path, ~1284 bytes)
     *   local_f98[0x8000]— file read buffer (32KB for route/address files)
     *   local_14[0x10]   — route string copy
     *   local_20[0x10]   — address string copy
     *   local_70[0x50]   — player name with escape sequences resolved
     *   local_80         — escape-processed output buffer stack area
     *   bytesRead        — ReadFile result
     *   found            — name match flag
     *   playerCount      — iteration counter
     */

    const int32_t PLAYER_COUNT_MAX = 20;
    const int32_t HEADER_OFFSET    = 4;     /* file data starts after 4-byte header */
    const uint32_t BUF_SIZE        = 0x8000;

    char  playerIdStr[0x98];       /* CRT_itoa output buffer */
    char  routeFilePath[0x504];    /* PostBag route file path */
    char  addrFilePath[0x504];     /* PostBag address file path */
    char  fileBuf[0x8000];         /* file read buffer */
    char  routeStr[0x10];          /* extracted route string */
    char  addrStr[0x10];           /* extracted address string */
    char  resolvedName[0x50];      /* processed player name */
    int32_t bytesRead;
    void* resolved = nullptr;
    int32_t playerEnumIdx;
    bool   playerFound = false;
    char   emptyByte = g_empty_string;

    /* Initialize playerIdStr to empty */
    {
        int32_t i;
        for (i = 0; i < 0x7F; i++) {
            reinterpret_cast<int32_t*>(playerIdStr)[i] = 0;
        }
    }
    playerIdStr[0] = emptyByte;

    /* Initialize found flags */
    resolvedName[0] = 0;
    playerEnumIdx = 0;

    /* Enumerate DPLAY players */
    DPLAY_EnumeratePlayers((int32_t)_g_dplay);

    /* Iterate through enumerated players */
    while (playerEnumIdx < PLAYER_COUNT_MAX) {
        bool nameMatched = false;
        int32_t playerSlot;

        /* Search the 16 player name slots at _g_dplay + 0xB13 */
        for (playerSlot = 0; playerSlot < 16; playerSlot++) {
            const char* slotName = (const char*)_g_dplay + 0xB13 +
                playerSlot * 0x0D;
            const char* targetName = (const char*)playerDPlayData + 0x10;

            /* Wide-char comparison of player name */
            const uint16_t* p1 = (const uint16_t*)targetName;
            const uint16_t* p2 = (const uint16_t*)slotName;

            int32_t cmp = 0;
            while (*p1 != 0 && *p1 == *p2) {
                p1++;
                p2++;
            }
            if (*p1 == *p2) {
                cmp = 0;
            } else if (*p1 < *p2) {
                cmp = -1;
            } else {
                cmp = 1;
            }

            if (cmp == 0) {
                nameMatched = true;
                CRT_itoa(playerSlot + 1, playerIdStr, 10);
                break;
            }
        }

        /* Send file creation requests for this player */
        NET_SendFile(playerIdStr, (uint8_t)1, routeFilePath);
        NET_SendFile(playerIdStr, (uint8_t)0, addrFilePath);

        /* ---- Read route file ---- */
        HANDLE hFile = CreateFileA(
            routeFilePath,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            return nullptr;
        }

        if (!ReadFile(hFile, fileBuf, BUF_SIZE,
                      reinterpret_cast<uint32_t*>(&bytesRead), nullptr)) {
            CloseHandle(hFile);
            return nullptr;
        }
        CloseHandle(hFile);

        {
            /* Parse first line as line count */
            int32_t lineCount = CRT_atoi(fileBuf);
            int32_t randVal = (int32_t)CRT_rand();
            int32_t lineIdx = randVal % (0x7FFF / lineCount);

            /* Skip past the header to find the Nth newline-delimited route */
            int32_t readPos = HEADER_OFFSET;
            while (readPos < bytesRead) {
                if (lineIdx == 0) break;
                if (fileBuf[readPos] == '\n') {
                    lineIdx--;
                }
                readPos++;
            }

            /* Strip carriage returns from remaining data */
            {
                int32_t stripPos = readPos;
                while (stripPos < bytesRead) {
                    if (fileBuf[stripPos] == '\r') {
                        fileBuf[stripPos] = '\0';
                    }
                    stripPos++;
                }
            }

            /* Copy the route string */
            {
                const char* src = &fileBuf[readPos];
                char* dst = routeStr;
                int32_t slen;
                for (slen = 0; src[slen] != '\0' && slen < 0x0F; slen++) {
                    dst[slen] = src[slen];
                }
                dst[slen] = '\0';
            }
        }

        /* ---- Read address file ---- */
        hFile = CreateFileA(
            addrFilePath,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            return nullptr;
        }

        if (!ReadFile(hFile, fileBuf, BUF_SIZE,
                      reinterpret_cast<uint32_t*>(&bytesRead), nullptr)) {
            CloseHandle(hFile);
            return nullptr;
        }
        CloseHandle(hFile);

        {
            /* Same parsing: random line selection */
            int32_t lineCount = CRT_atoi(fileBuf);
            int32_t randVal = (int32_t)CRT_rand();
            int32_t lineIdx = randVal % (0x7FFF / lineCount);

            int32_t readPos = HEADER_OFFSET;
            while (readPos < bytesRead) {
                if (lineIdx == 0) break;
                if (fileBuf[readPos] == '\n') {
                    lineIdx--;
                }
                readPos++;
            }

            /* Strip CR */
            {
                int32_t stripPos = readPos;
                while (stripPos < bytesRead) {
                    if (fileBuf[stripPos] == '\r') {
                        fileBuf[stripPos] = '\0';
                    }
                    stripPos++;
                }
            }

            /* Copy address string */
            {
                const char* src = &fileBuf[readPos];
                char* dst = addrStr;
                int32_t slen;
                for (slen = 0; src[slen] != '\0' && slen < 0x0F; slen++) {
                    dst[slen] = src[slen];
                }
                dst[slen] = '\0';
            }
        }

        /* ---- Build full path for NET_ResolveAddress ---- */
        {
            /* Find end of routeFilePath */
            int32_t pathLen;
            for (pathLen = 0; routeFilePath[pathLen] != '\0'; pathLen++) {}

            /* Calculate insertion point: strlen(routeFilePath) - strlen(playerIdStr) */
            int32_t idLen;
            for (idLen = 0; playerIdStr[idLen] != '\0'; idLen++) {}

            routeFilePath[pathLen - idLen] = '\0';

            /* Concatenate: routeFilePath + "\\" + routeStr */
            const char* concatSrc = routeStr;
            char* concatDst = routeFilePath + (pathLen - idLen);
            int32_t clen;
            for (clen = 0; concatSrc[clen] != '\0' && clen < 0x100; clen++) {
                *concatDst = concatSrc[clen];
                concatDst++;
            }
            *concatDst = '\0';
        }

        /* ---- Resolve address ---- */
        resolved = NET_ResolveAddress(_g_dplay, routeFilePath);
        if (resolved == NULL) {
            return NULL;
        }

        /* ---- Copy player name to session data block 2 (+0x25) ---- */
        {
            const char* src = reinterpret_cast<const char*>(
                reinterpret_cast<const uint8_t*>(playerDPlayData) + 0x10);
            char* dst = reinterpret_cast<char*>(
                reinterpret_cast<uint8_t*>(resolved) + 0x25);

            int32_t slen;
            for (slen = 0; src[slen] != '\0'; slen++) {
                dst[slen] = src[slen];
            }
            dst[slen] = '\0';
        }

        *reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(resolved) + 0x3A) = 0; /* m_wordValue */
        *reinterpret_cast<int32_t*>(
            reinterpret_cast<uint8_t*>(resolved) + 0x3C) = 1; /* m_dwordValue */

        /* ---- Process escape sequences in address string ---- */
        {
            const char* src = addrStr;
            char* dst = resolvedName;
            int32_t outPos = 0;

            while (*src != '\0' && outPos < 0x4F) {
                if (*src == '/') {
                    src++;
                    if (*src == '/') {
                        /* "//" -> "/" */
                        resolvedName[outPos] = '/';
                        outPos++;
                        src++;
                    } else if (*src == 'n') {
                        /* "/n" -> CR+LF (\r\n) */
                        resolvedName[outPos] = '\r';
                        resolvedName[outPos + 1] = '\n';
                        outPos += 2;
                        src++;
                    } else if (*src == '?') {
                        /* "/?" -> substitute player config name */
                        resolvedName[outPos] = '\0';
                        {
                            const char* playerName = (const char*)g_player_config + 6;
                            int32_t nameLen;
                            for (nameLen = 0; playerName[nameLen] != '\0'; nameLen++) {}
                            const char* pnSrc = playerName;
                            char* pnDst = resolvedName;
                            int32_t pnIdx;
                            for (pnIdx = 0; pnIdx < nameLen; pnIdx++) {
                                *pnDst = *pnSrc;
                                pnDst++;
                                pnSrc++;
                            }
                            *pnDst = '\0';
                            outPos = nameLen;
                        }
                        resolvedName[outPos] = ' ';
                        outPos++;
                        src++;
                    } else {
                        /* Unrecognized escape: keep '/' and current char */
                        resolvedName[outPos] = '/';
                        outPos++;
                    }
                } else {
                    resolvedName[outPos] = *src;
                    outPos++;
                    src++;
                }
            }
            resolvedName[outPos] = '\0';
        }

        /* ---- Copy processed name to DPlayData player name field (+0x43) ---- */
        {
            const char* src = resolvedName;
            char* dst = reinterpret_cast<char*>(
                reinterpret_cast<uint8_t*>(resolved) + 0x43);
            int outPos = 0;

            if (outPos < 0x50) {
                int32_t slen;
                for (slen = 0; src[slen] != '\0'; slen++) {
                    dst[slen] = src[slen];
                }
                dst[slen] = '\0';
            } else {
                resolvedName[0x4F] = '\0';
                src = resolvedName;
                int32_t slen;
                for (slen = 0; src[slen] != '\0'; slen++) {
                    dst[slen] = src[slen];
                }
                dst[slen] = '\0';
            }
        }

        playerEnumIdx++;
        playerFound = true;
    }

    /* Finalize: set player track/type via DPLAY_SetPlayerName */
    if (resolved != nullptr) {
        DPLAY_SetPlayerName(resolved, 1, -1);
    }

    return resolved;
#endif
}
