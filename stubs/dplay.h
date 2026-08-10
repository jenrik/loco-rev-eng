/**
 * stubs/dplay.h — Minimal DirectPlay type stubs
 */

#ifndef STUBS_DPLAY_H
#define STUBS_DPLAY_H

/* Types only (GUID, DWORD, LPSTR, HANDLE, STDMETHODCALLTYPE, ...) — this
 * header never needed windows.h's ~100 Win32 API function declarations,
 * only pulled them in as a side effect of including the whole thing. See
 * windows_types.h's header comment for why that matters to callers of
 * this header. */
#include "windows_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* DPID — DirectPlay player ID                                        */
/* ================================================================== */

typedef DWORD DPID;
typedef DWORD* LPDWORD;

/* ================================================================== */
/* DPSESSIONDESC2                                                     */
/* ================================================================== */

/*
 * Real Microsoft layout (dplay.h, verified byte-for-byte against
 * loco.exe's own desc[] index usage in DirectPlay_SetSessionDesc/
 * DirectPlay_EnumPlayers): dwSize,dwFlags,guidInstance,guidApplication,
 * dwMaxPlayers,dwCurrentPlayers,{lpszSessionName|lpszSessionNameA},
 * {lpszPassword|lpszPasswordA},dwReserved1,dwReserved2,dwUser1-4 — 20
 * dwords total (0x50 bytes). loco.exe queries IID_IDirectPlay4A (the
 * ANSI interface), so the name/password fields are LPSTR (char*), not
 * LPWSTR — a plain named field is used here instead of Microsoft's
 * anonymous union since this header is reached from native/*.c and
 * loco.exe never uses the wide variant.
 */
typedef struct _DPSESSIONDESC2 {
    DWORD  dwSize;
    DWORD  dwFlags;
    GUID   guidInstance;
    GUID   guidApplication;
    DWORD  dwMaxPlayers;
    DWORD  dwCurrentPlayers;
    LPSTR  lpszSessionNameA;
    LPSTR  lpszPasswordA;
    DWORD  dwReserved1;
    DWORD  dwReserved2;
    DWORD  dwUser1;
    DWORD  dwUser2;
    DWORD  dwUser3;
    DWORD  dwUser4;
} DPSESSIONDESC2, *LPDPSESSIONDESC2;
typedef const DPSESSIONDESC2* LPCDPSESSIONDESC2;

/* ================================================================== */
/* DPNAME                                                             */
/* ================================================================== */

/*
 * Real Microsoft layout has an anonymous union of wide/ANSI name pointers,
 * same as DPSESSIONDESC2 above. loco.exe's DirectPlay_ConnectToSession
 * (network/DirectPlay.cpp) builds this struct on the stack and assigns
 * plain ANSI `char*` values (a shared empty string, a player-name buffer)
 * into these fields via the IDirectPlay4A::CreatePlayer call — so, as with
 * DPSESSIONDESC2, a plain LPSTR field is used instead of Microsoft's wide
 * default.
 */
typedef struct _DPNAME {
    DWORD  dwSize;
    DWORD  dwFlags;
    LPSTR  lpszShortName;
    LPSTR  lpszLongName;
} DPNAME, *LPDPNAME;

/* ================================================================== */
/* DPCAPS                                                             */
/* ================================================================== */

typedef struct _DPCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwMaxBufferSize;
    DWORD dwMaxQueueSize;
    DWORD dwMaxPlayers;
    DWORD dwHundredBaud;
    DWORD dwLatency;
    DWORD dwMaxLocalPlayers;
    DWORD dwHeaderLength;
    DWORD dwTimeout;
} DPCAPS;

/* ================================================================== */
/* DirectPlay constants                                                */
/* ================================================================== */

/* Session flags */
#define DPSESSION_NEWPLAYERSDISABLED    0x00000001
#define DPSESSION_MIGRATEHOST           0x00000002
#define DPSESSION_NOMESSAGEID           0x00000004
#define DPSESSION_JOINDISABLED          0x00000008
#define DPSESSION_KEEPALIVE             0x00000010
#define DPSESSION_NODATAMESSAGES        0x00000020
#define DPSESSION_SECURESERVER          0x00000040
#define DPSESSION_PRIVATE               0x00000080
#define DPSESSION_PASSWORDREQUIRED      0x00000100
#define DPSESSION_MULTICASTSERVER       0x00000200
#define DPSESSION_CLIENTSERVER          0x00000400
#define DPSESSION_DIRECTPLAYPROTOCOL    0x00000800
#define DPSESSION_NOPRESERVECOUNTING    0x00001000
#define DPSESSION_OPTIMIZELATENCY       0x00002000

/* Session descriptions */
#define DPSEND_GUARANTEED              0x00000001
#define DPSEND_HIGHPRIORITY            0x00000002
#define DPSEND_OPENSTREAM              0x00000004
#define DPSEND_CLOSESTREAM             0x00000008
#define DPSEND_SIGNED                  0x00000010
#define DPSEND_ENCRYPTED               0x00000020
#define DPSEND_LOBLOBBY                0x00000040
#define DPSEND_NEWPLAYERMSG            0x00000080
#define DPSEND_NEWHOST                 0x00000100
#define DPSEND_ASYNC                   0x00000200
#define DPSEND_NOSENDCOMPLETEMSG       0x00000400
#define DPSEND_MAX_PRI                 0x00000800
#define DPSEND_COMPLETEONPROCESS       0x00001000
#define DPSEND_NOSENDCOMPLETEMSG2      0x00002000

/* Open flags */
#define DPOPEN_CREATE                  0x00000001
#define DPOPEN_JOIN                    0x00000002
#define DPOPEN_RETURNSTATUS            0x00000004

/* Connection types */
#define DPCONNECTION_DIRECTPLAY        0x00000001
#define DPCONNECTION_DIRECTPLAYLOBBY   0x00000002
#define DPCONNECTION_DIRECTPLAY4       0x00000004

/* Player flags */
#define DPPLAYER_LOCAL                 0x00000001
#define DPPLAYER_SERVERPLAYER          0x00000002
#define DPPLAYER_SPECTATOR             0x00000004

/* Enum players flags */
#define DPENUMPLAYERS_ALL              0x00000000
#define DPENUMPLAYERS_LOCAL            0x00000001
#define DPENUMPLAYERS_REMOTE           0x00000002
#define DPENUMPLAYERS_GROUP            0x00000004
#define DPENUMPLAYERS_SESSION          0x00000008
#define DPENUMPLAYERS_SERVERPLAYER     0x00000010
#define DPENUMPLAYERS_SPECTATOR        0x00000020

/* End session flags */
#define DPENDSESSION_NORMAL            0x00000001
#define DPENDSESSION_IMMEDIATE         0x00000002
#define DPENDSESSION_ABORT             0x00000004
#define DPENDSESSION_KEEPALIVE         0x00000008

/* Service provider GUIDs — minimal set.
 * IPX/Modem/Serial below are byte-verified against loco.exe's own .rdata
 * (network/DirectPlay.cpp's DPSPGUID_* constants, read directly from the
 * binary at 0x478fa8-0x478fe7). The previous TCP/IP entry here
 * ({EBFE7BA0-628D-11D2-AE0F-006097B01411}) was the later DirectPlay 8
 * DP8SP_TCPIP GUID, not the classic DirectPlay 3-6 DPSPGUID_TCPIP loco.exe
 * actually uses — corrected below. */
/* {36E95EE0-8577-11CF-960C-0080C7534E82} — TCP/IP (DPSPGUID_TCPIP) */
/* {685BC400-9D2C-11CF-A9CD-00AA006886E3} — IPX (DPSPGUID_IPX) */
/* {44EAA760-CB68-11CF-9C4E-00A0C905425E} — Modem (DPSPGUID_MODEM) */
/* {0F1D6860-88D9-11CF-9C4E-00A0C905425E} — Serial (DPSPGUID_SERIAL) */

/* ================================================================== */
/* Error codes                                                         */
/* ================================================================== */

#define DP_OK                       0x00000000
#define DP_OK_USERCANCEL            0x88770100
#define DPERR_ALREADYINITIALIZED    0x88770005
#define DPERR_ACCESSDENIED          0x8877000A
#define DPERR_ACTIVEPLAYERS         0x8877000F
#define DPERR_BUFFERTOOSMALL        0x88770014
#define DPERR_CANTCREATEPLAYER      0x8877001E
#define DPERR_CANTCREATESESSION     0x88770028
#define DPERR_CANTPLAYERSESSION     0x88770032
#define DPERR_CANTSEND              0x8877003C
#define DPERR_CANTLOADSESSION       0x88770046
#define DPERR_CANTADDPLAYER         0x88770050
#define DPERR_CONNECTIONLOST        0x8877005A
#define DPERR_EXCEPTION             0x88770064
#define DPERR_GENERIC               0x8877006E
#define DPERR_INVALIDFLAGS          0x88770078
#define DPERR_INVALIDOBJECT         0x88770082
#define DPERR_INVALIDPARAMS         0x8877008C
#define DPERR_INVALIDPLAYER         0x88770096
#define DPERR_INVALIDGROUP          0x887700A0
#define DPERR_NOCAPS                0x887700AA
#define DPERR_NOCONNECTION          0x887700B4
#define DPERR_NOMEMORY              0x887700BE
#define DPERR_NOMESSAGES            0x887700C8
#define DPERR_NONAMESERVERFOUND     0x887700D2
#define DPERR_NOPLAYERS             0x887700DC
#define DPERR_NOSESSIONS            0x887700E6
#define DPERR_PENDING               0x887700F0
#define DPERR_SENDTOOBIG            0x887700FA
#define DPERR_TIMEOUT               0x88770104
#define DPERR_UNAVAILABLE           0x8877010E
#define DPERR_UNSUPPORTED           0x88770118
#define DPERR_BUSY                  0x88770122
#define DPERR_USERCANCEL            0x8877012C
#define DPERR_NOINTERFACE           0x88770136
#define DPERR_CANNOTCREATESERVER    0x88770140
#define DPERR_PLAYERLOST            0x8877014A
#define DPERR_SESSIONLOST           0x88770154
#define DPERR_UNINITIALIZED         0x8877015E
#define DPERR_NONEWPLAYERS          0x88770168
#define DPERR_INVALIDPASSWORD       0x88770172
#define DPERR_CONNECTING            0x8877017C
#define DPERR_CONNECTIONLOST2       0x88770186
#define DPERR_ALREADYCONNECTED      0x88770190
#define DPERR_NOTHANDLED            0x8877019A
#define DPERR_CANCELFAILED          0x887701A4
#define DPERR_STILLCONNECTED        0x887701AE
#define DPERR_NOTLOBBIED            0x887701B8
#define DPERR_SESSIONNOTFOUND       0x887701C2
#define DPERR_CANTJOIN              0x887701CC

#ifdef __cplusplus
}
#endif

/* ================================================================== */
/* IDirectPlay4A / IDirectPlayLobby3A — COM interfaces                 */
/*                                                                     */
/* Never extern "C": virtual dispatch is a C++ linkage feature (see    */
/* CLAUDE.md's anti-pattern rule on this). Real signatures, method     */
/* names, and calling convention (__stdcall) come from the genuine     */
/* DirectX 6.0 SDK's dplay.h/dplobby.h (see NOTE-directx-sdk.md);      */
/* loco.exe's own decompiled code calls through these, never through   */
/* manual vtable-offset arithmetic — see network/DirectPlay.cpp.       */
/*                                                                     */
/* This is a typed adapter exposing only the methods loco.exe's        */
/* reconstructed code actually calls, as real C++ virtual methods —    */
/* not a vtable-slot-accurate replica, since no concrete instance is   */
/* ever backed by a real dplayx.dll on Linux, and the real mingw-w64   */
/* dplay.h/dplobby.h headers are not usable here either (their         */
/* DEFINE_GUID/COM preamble needs an ole2.h-based include chain this   */
/* project's forced compat.h header pre-empts — see PROGRESS.md for    */
/* the open item to wire in the genuine headers on _WIN32 instead).    */
/* The whole DirectPlay subsystem is dormant on host in any case.      */
/* ================================================================== */
#ifdef __cplusplus

typedef BOOL (STDMETHODCALLTYPE *LPDPENUMSESSIONSCALLBACK2)(
    LPCDPSESSIONDESC2 lpThisSD, LPDWORD lpdwTimeOut, DWORD dwFlags, LPVOID lpContext);
typedef BOOL (STDMETHODCALLTYPE *LPDPENUMADDRESSCALLBACK)(
    const GUID& guidDataType, DWORD dwDataSize, LPCVOID lpData, LPVOID lpContext);

struct IDirectPlay4A {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(const GUID& riid, LPVOID* ppvObj) = 0;
    virtual ULONG   STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG   STDMETHODCALLTYPE Release() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE CreatePlayer(DPID* lpidPlayer, LPDPNAME lpPlayerName,
                                                    HANDLE hEvent, LPVOID lpData,
                                                    DWORD dwDataSize, DWORD dwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCaps(DPCAPS* lpDPCaps, DWORD dwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPlayerAddress(DPID idPlayer, LPVOID lpAddress,
                                                        LPDWORD lpdwAddressSize) = 0;
    virtual HRESULT STDMETHODCALLTYPE Open(LPDPSESSIONDESC2 lpsd, DWORD dwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnumSessions(LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
                                                    LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
                                                    LPVOID lpContext, DWORD dwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE CancelMessage(DWORD dwMsgID, DWORD dwFlags) = 0;
    virtual ~IDirectPlay4A() {}
};

struct IDirectPlayLobby3A {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(const GUID& riid, LPVOID* ppvObj) = 0;
    virtual ULONG   STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG   STDMETHODCALLTYPE Release() = 0;
    virtual HRESULT STDMETHODCALLTYPE EnumAddress(LPDPENUMADDRESSCALLBACK lpEnumAddressCallback,
                                                   LPCVOID lpAddress, DWORD dwAddressSize,
                                                   LPVOID lpContext) = 0;
    virtual ~IDirectPlayLobby3A() {}
};

#endif /* __cplusplus */

#endif /* STUBS_DPLAY_H */
