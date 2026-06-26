/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: Win32 Platform Layer (WndProc, window, event loop)
 * PRIMARY PORTING TARGET: every Win32 API documented with Linux SDL2 equivalent
 *
 * Original binary: loco.exe — Windows 95/98, DirectX 5 era
 * Developer: Intelligent Games for LEGO Media
 *
 * This file documents the Win32 platform layer in pseudo-C reconstructed from
 * Ghidra decompilation.  Every Win32 API call is annotated:
 *   WIN32: <original API and args>
 *   LINUX: <SDL2 / POSIX replacement>
 *
 * Build note: define LOCO_LINUX to compile the Linux port; leave undefined to
 * study the Win32 original using the stub types in core/core.h.
 */

#include "win32_platform.h"
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Global state referenced throughout this translation unit
 *
 * On Win32 these are raw DAT_xxxxxxxx addresses in the .bss/.data segment.
 * On Linux they are defined in core/core.c and declared extern in core/core.h.
 * ========================================================================= */

volatile int g_renderDirty = 0;    /* DAT_004aa4a4 — set by WM_TIMER id=0x47,
                                     * cleared after SDL_RenderPresent.
                                     * LINUX: written by SDL_AddTimer callback;
                                     * declare volatile or use atomic_int.     */

/* =========================================================================
 * FUN_00406480 (0x00406480) — Pre-window setup / desktop geometry
 *
 * Queries the desktop HWND, reads the primary monitor dimensions, fills the
 * desktop RECT, and reads per-object FPS-balancing thresholds + window
 * position from lego.ini.
 *
 * No DirectDraw calls here despite the surrounding context; DirectDraw init
 * is further down the call chain in FUN_00406f90.
 * ========================================================================= */
void FUN_00406480(CGWND *self)
{
    /* WIN32: GetDesktopWindow()
     *   Returns the root desktop HWND; stored in self->hwndDesktop so later
     *   calls to GetDC(desktopHwnd) can query display properties.
     * LINUX: not needed — SDL_GetDisplayBounds(0, &rect) gives the same
     *        geometry without requiring a window handle. */
    self->hwndDesktop = GetDesktopWindow();

    /* WIN32: GetSystemMetrics(SM_CXSCREEN=0)
     *   Returns the pixel width of the primary monitor.
     * LINUX: SDL_DisplayMode mode;
     *        SDL_GetCurrentDisplayMode(0, &mode);
     *        g_screenWidth = mode.w; */
    g_screenWidth  = GetSystemMetrics(0);   /* SM_CXSCREEN */

    /* WIN32: GetSystemMetrics(SM_CYSCREEN=1)
     *   Returns the pixel height of the primary monitor.
     * LINUX: g_screenHeight = mode.h;  (same SDL_DisplayMode as above) */
    g_screenHeight = GetSystemMetrics(1);   /* SM_CYSCREEN */

    /* WIN32: SetRect(&DAT_004851e0, 0, 0, g_screenWidth, g_screenHeight)
     *   Convenience function that fills a RECT; equivalent to four assignments.
     * LINUX: SDL_Rect desktopRect = {0, 0, g_screenWidth, g_screenHeight};
     *        or use SDL_GetDisplayBounds(0, &desktopRect). */
    /* SetRect(&DAT_004851e0, 0, 0, g_screenWidth, g_screenHeight); */

    /* INI reads via the internal CIniFile layer (no Win32 API):
     *   lego.ini [WINDOW_ATTRIBUTES] → DAT_004851e8/ec/f0/f4 (window rect)
     *   lego.ini [BALANCING]         → self->minVehicleFPS (def 20)
     *                                  self->minBuildingFPS (def 18)
     *                                  self->minMinifigFPS  (def 16)
     *                                  self->minFlyingFPS   (def 14)
     *   lego.ini [PROCESS]CleanExit  → crash-detection flag
     * LINUX: replace CIniFile calls with an inih/minIni wrapper reading from
     *        $LEGO_LOCO_HOME/lego.ini or SDL_GetPrefPath(). */
}

/* =========================================================================
 * FUN_004068d0 (0x004068d0) — Registry / INI path discovery
 *
 * Reads the install path from the Windows registry; creates the key on first
 * run and writes the path back; then opens lego.ini from the discovered path.
 *
 * Linux port: replace the entire registry block with an fopen of a config
 * file at SDL_GetPrefPath("IntelligentGames", "LegoLoco").
 * ========================================================================= */
BOOL FUN_004068d0(void)
{
    /* WIN32: RegOpenKeyExA(HKEY_LOCAL_MACHINE=0x80000002,
     *            "SOFTWARE\\Intelligent Games\\LEGO LOCO",
     *            0, KEY_READ=0x20019, &hKey)
     *   Opens the install-path key for reading.  Returns ERROR_SUCCESS on hit.
     * LINUX: fopen(SDL_GetPrefPath("IntelligentGames","LegoLoco")
     *             "config.ini", "r")
     *        SDL_GetPrefPath returns e.g.
     *        $HOME/.local/share/IntelligentGames/LegoLoco/ on XDG desktops. */
    /* RegOpenKeyExA((HKEY)0x80000002,
     *     "SOFTWARE\\Intelligent Games\\LEGO LOCO", 0, 0x20019, &hKey); */

    /* WIN32: RegQueryValueExA(hKey, NULL, NULL, &type, (LPBYTE)buf, &len)
     *   Reads the default string value (the install directory path).
     * LINUX: fread / fgets + sscanf / an inih callback. */
    /* RegQueryValueExA(hKey, NULL, NULL, &type, (LPBYTE)buf, &len); */

    /* WIN32: RegCloseKey(hKey)
     *   Releases the key handle.
     * LINUX: fclose(f) */
    /* RegCloseKey(hKey); */

    /* WIN32: RegCreateKeyExA(HKEY_LOCAL_MACHINE,
     *            "SOFTWARE\\Intelligent Games\\LEGO LOCO",
     *            0, NULL, 0, KEY_ALL_ACCESS=0xF003F, NULL, &hKey, NULL)
     *   Creates the key on first run (when RegOpenKeyExA found nothing).
     * LINUX: mkdir(prefPath, 0755) + fopen(path, "w") */
    /* RegCreateKeyExA((HKEY)0x80000002,
     *     "SOFTWARE\\Intelligent Games\\LEGO LOCO",
     *     0, NULL, 0, 0xf003f, NULL, &hKey, NULL); */

    /* WIN32: RegSetValueExA(hKey, NULL, 0, REG_SZ=1, (LPBYTE)buf, len)
     *   Writes the install path as the default (unnamed) REG_SZ value.
     * LINUX: fputs(installPath, f) */
    /* RegSetValueExA(hKey, NULL, 0, 1, (LPBYTE)buf, len); */

    /* WIN32: RegCloseKey(hKey)   [second close after write]
     * LINUX: fclose(f) */
    /* RegCloseKey(hKey); */

    /* After path discovery: opens lego.ini via the internal CIniFile layer.
     * LINUX: same fopen-based INI reader pointed at the discovered path. */

    return TRUE;
}

/* =========================================================================
 * FUN_00406680 (0x00406680) — Display capability pre-flight check
 *
 * Queries the primary monitor's colour depth via GDI.  Returns failure if
 * the display is in palette (8-bit) mode or below 16-bit colour, or when
 * multi-monitor colour-depth mismatches are detected.
 * ========================================================================= */
UINT FUN_00406680(CGWND *self)
{
    /* WIN32: GetDC(self->hwndDesktop)
     *   Obtains a GDI device context for the desktop window so that
     *   GetDeviceCaps can interrogate the display hardware.
     * LINUX: not needed; SDL_GetCurrentDisplayMode gives all the info.
     *        SDL_DisplayMode mode;
     *        SDL_GetCurrentDisplayMode(0, &mode); */
    /* HDC hdc = GetDC(self->hwndDesktop); */

    /* WIN32: GetDeviceCaps(hdc, RASTERCAPS=0x18)
     *   Returns a bitmask of rasteriser capabilities.  The RC_PALETTE bit
     *   (0x0100) indicates a palettised (8-bit) display — unsupported.
     * LINUX: SDL_ISPIXELFORMAT_INDEXED(mode.format)
     *        Returns SDL_TRUE when the current display is palette-based. */
    /* UINT caps = GetDeviceCaps(hdc, 0x18); */

    /* WIN32: GetDeviceCaps(hdc, BITSPIXEL=0x0c)
     *   Returns the colour depth of the primary display in bits per pixel.
     *   The game requires >= 16 bpp.  Stored in DAT_0048521c for later checks.
     * LINUX: SDL_BITSPERPIXEL(mode.format)
     *        e.g. SDL_BITSPERPIXEL(SDL_PIXELFORMAT_RGB565) == 16 */
    /* DAT_0048521c = GetDeviceCaps(hdc, 0x0c); */

    /* WIN32: ReleaseDC(self->hwndDesktop, hdc)
     *   Returns the device context to the system.
     * LINUX: nothing; SDL does not hold explicit DC ownership. */
    /* ReleaseDC(self->hwndDesktop, hdc); */

    /* WIN32: GetSystemMetrics(SM_SAMEDISPLAYFORMAT=0x13)
     *   Returns non-zero when all connected monitors share the same pixel
     *   format (required for multi-monitor DirectDraw).
     * LINUX: iterate SDL_GetNumDisplayModes(i) for each display index and
     *        compare the returned SDL_DisplayMode.format values. */
    /* int sameFormat = GetSystemMetrics(0x13); */

    /* Failure path — palette mode or depth < 16 bpp:
     * WIN32: MessageBoxA(NULL, localised_error_text, "LEGO LOCO",
     *                    MB_ICONEXCLAMATION=0x30)
     *   Shows a modal error dialog above all windows.
     * LINUX: SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
     *            "LEGO LOCO", localised_error_text, NULL)
     *   Use NULL for the window handle during pre-window init. */
    /* if ((caps & 0x100) || DAT_0048521c < 16) {
     *     MessageBoxA(NULL, errorText, "LEGO LOCO", 0x30);
     *     return 0;
     * } */

    (void)self;
    return 1;
}

/* =========================================================================
 * FUN_00406ed0 (0x00406ed0) — Window class registration + window creation
 *
 * Registers the "LEGO LOCO" WNDCLASS and creates the fullscreen WS_POPUP
 * game window that hosts all rendering.  Stores the HWND in self->hwndGame.
 * ========================================================================= */
BOOL FUN_00406ed0(CGWND *self)
{
    /* Build WNDCLASSA descriptor:
     *   style         = CS_OWNDC | CS_DBLCLKS = 0x0028
     *   lpfnWndProc   = WndProc @ 0x004618c0
     *   cbClsExtra    = 0
     *   cbWndExtra    = 0
     *   hInstance     = self->hInstance
     *   hbrBackground = NULL   (DirectDraw covers the entire client area)
     *   hCursor       = NULL   (game renders its own cursor sprite)
     *   lpszMenuName  = NULL
     *   lpszClassName = "LEGO LOCO"
     */

    /* WIN32: LoadIconA(self->hInstance, MAKEINTRESOURCE(0x65))
     *   Loads icon resource ID 0x65 (decimal 101) embedded in loco.exe and
     *   stores the HICON in WNDCLASSA.hIcon.
     * LINUX: SDL_Surface *iconSurf = SDL_LoadBMP("loco_icon.bmp");
     *        SDL_SetWindowIcon(sdlWindow, iconSurf);
     *        SDL_FreeSurface(iconSurf); */
    /* wc.hIcon = LoadIconA(self->hInstance, (LPCSTR)0x65); */

    /* WIN32: RegisterClassA(&wc)
     *   Registers the window class with the OS.  Only required once per
     *   process lifetime.
     * LINUX: not needed; SDL_CreateWindow handles class registration internally
     *        and exposes no equivalent concept. */
    /* RegisterClassA(&wc); */

    /* WIN32: CreateWindowExA(
     *     dwExStyle  = WS_EX_TOPMOST=0x00000008  (local play)
     *                  or 0                       (remote-play / g_debugMode),
     *     lpClassName  = "LEGO LOCO",
     *     lpWindowName = "LEGO LOCO",
     *     dwStyle    = WS_POPUP | WS_VISIBLE = 0x82000000,
     *     X=0, Y=0, nWidth=g_screenWidth, nHeight=g_screenHeight,
     *     hWndParent=NULL, hMenu=NULL, hInstance=self->hInstance, lpParam=NULL)
     *
     *   Creates a full-screen borderless popup window at (0,0) covering the
     *   entire desktop.  WS_EX_TOPMOST keeps it above all other windows
     *   during normal (local) play.
     *
     * LINUX: g_sdlWindow = SDL_CreateWindow(
     *            "LEGO LOCO",
     *            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
     *            g_screenWidth, g_screenHeight,
     *            SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS
     *            | SDL_WINDOW_SHOWN);
     *        SDL_WINDOW_ALWAYS_ON_TOP replaces WS_EX_TOPMOST for non-debug builds. */
    /* HWND hWnd = CreateWindowExA(
     *     g_debugMode ? 0 : WS_EX_TOPMOST,
     *     "LEGO LOCO", "LEGO LOCO",
     *     WS_POPUP | WS_VISIBLE,
     *     0, 0, g_screenWidth, g_screenHeight,
     *     NULL, NULL, self->hInstance, NULL); */

    /* WIN32: GetClientRect(hWnd, &DAT_00485220)
     *   Returns the drawable client area dimensions.  For a WS_POPUP window
     *   with no title bar or borders this is exactly (0, 0, W, H).
     *   The rect is cached at DAT_00485220 for later subsystem use.
     * LINUX: int clientW, clientH;
     *        SDL_GetWindowSize(g_sdlWindow, &clientW, &clientH); */
    /* GetClientRect(hWnd, &DAT_00485220); */

    /* Store HWND at self->hwndGame for later use by the message loop and
     * subsystems that call SendMessage / PostMessage.
     * LINUX: self->hwndGame = (HWND)g_sdlWindow; */
    /* self->hwndGame = hWnd; */

    (void)self;
    return TRUE;
}

/* =========================================================================
 * FUN_00406ba0 (0x00406ba0) — Subsystem orchestration + 35 fps timer start
 *
 * Top-level initialisation hub.  Allocates ~10 engine subsystem objects
 * (via FUN_00406f90), starts the multimedia timer that drives the game loop
 * at ~35.7 fps, and creates the synchronisation event for frame timing.
 * ========================================================================= */
int FUN_00406ba0(CGWND *self)
{
    /* Subsystem allocations (called via FUN_00406f90):
     *   CDirectDrawManager  g_pDirectDraw   — DAT_004fd378
     *   CDirectSoundManager g_pDirectSound  — DAT_004fd37c
     *   CNetworkManager     g_pNetworkMgr   — DAT_004fd388
     *   CAnimManager        g_pAnimMgr      — DAT_00485258
     *   CInputManager       g_pInputMgr     — DAT_004fd384
     *   CMoviePlayer        g_pMovieMgr     — DAT_004fd380
     *   CSceneManager       g_pSceneMgr     — DAT_004fd38c
     *   CWorldManager       g_pWorldMgr     — DAT_004fd390
     * LINUX: replace with SDL_mixer_init, SDL input events, BSD sockets, etc. */

    /* WIN32: CreateEventA(lpSecurity=NULL, bManualReset=TRUE,
     *                     bInitialState=FALSE, lpName="GameLoop")
     *   Creates a named manual-reset kernel event object.
     *   GameLoopCB (the multimedia timer callback) calls SetEvent() on each
     *   28 ms tick.  The main message loop calls:
     *     WaitForSingleObject(g_hGameLoopEvent, INFINITE)
     *     ... process frame ...
     *     ResetEvent(g_hGameLoopEvent)
     *   to synchronise to the 35.7 fps cadence.
     * LINUX: sem_init(&g_gameSem, 0, 0)
     *        In TimerCB: sem_post(&g_gameSem);
     *        In main loop: sem_wait(&g_gameSem);
     *        Alternative: eventfd(0, EFD_SEMAPHORE) integrated with epoll. */
    g_hGameLoopEvent = CreateEventA(NULL, TRUE, FALSE, "GameLoop");

    /* WIN32: timeBeginPeriod(14)
     *   Raises the global Win32 multimedia timer resolution to 14 ms.
     *   Required before timeSetEvent so the periodic callback fires
     *   accurately at 28 ms rather than rounding to the 15.6 ms default.
     * LINUX: no-op.  Linux timer_create(CLOCK_MONOTONIC) and SDL_AddTimer
     *        already provide ~1 ms resolution with no global side-effect. */
    timeBeginPeriod(14);

    /* WIN32: timeSetEvent(uDelay=28, uResolution=14,
     *                     lpTimeProc=GameLoopCB, dwUser=0,
     *                     fuEvent=TIME_PERIODIC=1)
     *   Fires GameLoopCB every 28 ms from a separate multimedia timer thread.
     *   1000 ms / 28 ms ≈ 35.7 fps.
     *   The returned MMRESULT timer ID is saved to g_timerID (DAT_00485438)
     *   for later cancellation in FUN_004077a0 via timeKillEvent.
     * LINUX: SDL_TimerID g_sdlTimerID = SDL_AddTimer(28, TimerCB, NULL);
     *   In TimerCB (called from SDL's timer thread):
     *     SDL_Event ev;
     *     SDL_memset(&ev, 0, sizeof(ev));
     *     ev.type = g_gameTickEventType;  // from SDL_RegisterEvents(1)
     *     SDL_PushEvent(&ev);
     *     return 28;   // reschedule for another 28 ms */
    g_timerID = timeSetEvent(28, 14, /* GameLoopCB */ NULL, 0, TIME_PERIODIC);

    (void)self;
    return (g_timerID != 0) ? 0 : -1;
}

/* =========================================================================
 * FUN_004085e0 (0x004085e0) — Mini message pump / loading filter
 *
 * Drains the Win32 message queue during heavy loading sequences.  Mouse
 * interactions are suppressed so the game does not misinterpret clicks made
 * while subsystems are being initialised.
 *
 * param_1 (blockMouse):
 *   0 = normal  — mouse-move messages are dispatched as usual
 *   non-zero    — mouse-move is swallowed; cursor is forced blank
 *
 * Mouse-button messages (WM_LBUTTONDOWN/UP, WM_RBUTTONDOWN/UP) are ALWAYS
 * discarded regardless of blockMouse.
 * ========================================================================= */
void FUN_004085e0(char blockMouse)
{
    /* msg: local MSG variable populated by PeekMessageA.
     * LINUX: SDL_Event ev; used with SDL_PollEvent / SDL_PeepEvents. */
    /* MSG msg; */

    /* Primary loop: drain the queue until no more messages are pending.
     *
     * WIN32: while (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE=0)) { ... }
     *   PM_NOREMOVE peeks at the next message without dequeuing it.
     *   The loop body decides whether to dispatch, discard, or leave it.
     * LINUX: Use SDL_PeepEvents(&ev, 1, SDL_PEEKEVENT,
     *            SDL_FIRSTEVENT, SDL_LASTEVENT) > 0
     *        to non-destructively check for pending events before deciding
     *        to SDL_PollEvent (which always dequeues). */

    /* ── WM_MOUSEMOVE (0x200) ─────────────────────────────────────────────
     * WIN32: if (msg.message == WM_MOUSEMOVE) {
     *            PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE=1); // dequeue
     *            if (!blockMouse) {
     *                TranslateMessage(&msg);    // VK → WM_CHAR translation
     *                DispatchMessageA(&msg);    // route to WndProc
     *            }
     *            // blockMouse != 0: message silently discarded
     *            continue;
     *        }
     * LINUX: if (ev.type == SDL_MOUSEMOTION) {
     *            SDL_PollEvent(&ev);  // dequeue
     *            if (!blockMouse) { handle_mouse_move(ev); }
     *            continue;
     *        } */

    /* ── WM_SETCURSOR (0x20) ──────────────────────────────────────────────
     * WIN32: if (msg.message == WM_SETCURSOR) {
     *            PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
     *            if (blockMouse) {
     *                SetCursor(NULL);  // force blank cursor immediately
     *            } else {
     *                TranslateMessage(&msg);
     *                DispatchMessageA(&msg);
     *            }
     *            continue;
     *        }
     *   WIN32: SetCursor(NULL) — removes the current mouse cursor from screen.
     * LINUX: if (blockMouse) SDL_ShowCursor(SDL_DISABLE); */

    /* ── Mouse buttons: always swallow (0x201–0x205) ─────────────────────
     * WM_LBUTTONDOWN (0x201), WM_LBUTTONUP (0x202),
     * WM_RBUTTONDOWN (0x204), WM_RBUTTONUP (0x205) are all dequeued and
     * dropped unconditionally during loading, regardless of blockMouse.
     * No click events ever reach the game while assets are loading.
     *
     * WIN32: if (msg.message >= WM_LBUTTONDOWN &&
     *            msg.message <= WM_RBUTTONUP) {
     *            PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
     *            continue;  // discard
     *        }
     * LINUX: if (ev.type == SDL_MOUSEBUTTONDOWN ||
     *            ev.type == SDL_MOUSEBUTTONUP) {
     *            SDL_PollEvent(&ev);  // dequeue and discard
     *            continue;
     *        } */

    /* ── All other messages: leave in queue and return ────────────────────
     * WIN32: break; // do NOT call PeekMessageA(PM_REMOVE) — leave pending
     *   The message stays in the queue for the real message loop to handle.
     * LINUX: Use SDL_SetEventFilter to drop mouse events during loading, or
     *        maintain a boolean g_loadingActive checked in SDL_PollEvent. */

    (void)blockMouse;
}

/* =========================================================================
 * FUN_00408350 (0x00408350) — Deferred startup / repaint trigger
 *
 * Installs a one-shot WM_TIMER to defer heavy DirectDraw flip-chain
 * allocation until the window is fully visible.  Blocks user input during
 * loading and forces an initial repaint.
 * ========================================================================= */
void FUN_00408350(HWND hWnd)
{
    /* WIN32: SetTimer(hWnd, nIDEvent=0x47=71, uElapse=150, lpTimerFunc=NULL)
     *   Queues a WM_TIMER(id=71) message 150 ms after the window is shown.
     *   The WndProc WM_TIMER handler (id check == 0x47) sets g_renderDirty=1
     *   to trigger the first DirectDraw flip via FUN_0045e1e0.
     *   The timer ID is saved to DAT_004a97a4 for KillTimer in FUN_004086f0.
     *   Return value: the timer identifier (0 on failure).
     * LINUX: SDL_TimerID hbTimerID = SDL_AddTimer(150, HeartbeatCB, NULL);
     *        HeartbeatCB sets g_renderDirty = 1 and returns 0 to fire once.
     *        Alternatively call the init directly after the first
     *        SDL_RenderPresent and skip the timer entirely. */
    /* DAT_004a97a4 = SetTimer(hWnd, LOCO_HEARTBEAT_TIMER_ID, 150, NULL); */

    /* WIN32: EnableWindow(hWnd, FALSE)
     *   Disables all keyboard and mouse input to the window until loading
     *   completes and EnableWindow(hWnd, TRUE) is called later.
     * LINUX: no SDL equivalent for blanket input disabling.  Set a boolean
     *        g_loadingActive = 1 and check it at the top of the SDL event
     *        loop to swallow unwanted events. */
    /* EnableWindow(hWnd, FALSE); */

    /* WIN32: InvalidateRect(hWnd, NULL, FALSE)
     *   Marks the entire client area as invalid (dirty), scheduling a
     *   WM_PAINT delivery on the next GetMessage cycle.
     *   Third arg FALSE: do not erase the background before WM_PAINT.
     * LINUX: SDL_RenderPresent(g_sdlRenderer) — push the loading-screen
     *        frame to the display immediately; no lazy deferral needed. */
    /* InvalidateRect(hWnd, NULL, FALSE); */

    /* WIN32: UpdateWindow(hWnd)
     *   Delivers the pending WM_PAINT synchronously to WndProc, bypassing
     *   the message queue (does NOT go through GetMessage/PeekMessage).
     * LINUX: SDL_RenderPresent achieves the same synchronous result.
     *        No separate "update" step is needed; Present is always synchronous. */
    /* UpdateWindow(hWnd); */

    (void)hWnd;
}

/* =========================================================================
 * FUN_004086f0 (0x004086f0) — Game mode exit handler (modes 1–9)
 *
 * Tears down the current game mode and triggers an async state transition.
 * ========================================================================= */
void FUN_004086f0(int mode)
{
    /* WIN32: KillTimer(hWnd, DAT_004a97a4)
     *   Cancels the 150 ms startup timer installed by FUN_00408350 via
     *   SetTimer.  Must be called before the HWND is destroyed, otherwise
     *   WM_TIMER fires into a dead WndProc.
     * LINUX: SDL_RemoveTimer(g_hbTimerID) */
    /* KillTimer(g_pGameWnd->hwndGame, DAT_004a97a4); */

    /* WIN32: PostMessageA(hWnd, WM_APP+6=0x406, DAT_004a99b4, 0)
     *   Posts the locale/language-switch notification asynchronously.
     *   WndProc case 0x406 calls FUN_00447400 to reload string tables in
     *   the new language on the next message-loop iteration.
     *   PostMessageA is non-blocking: returns immediately after queueing.
     * LINUX: SDL_Event ev;
     *        SDL_memset(&ev, 0, sizeof(ev));
     *        ev.type           = g_localeChangeEventType; // SDL_RegisterEvents(1)
     *        ev.user.code      = DAT_004a99b4;
     *        ev.user.data1     = NULL;
     *        ev.user.data2     = NULL;
     *        SDL_PushEvent(&ev);  // equivalent non-blocking queue push */
    /* PostMessageA(g_pGameWnd->hwndGame, WM_LOCO_LOCALE_CHANGE,
     *              DAT_004a99b4, 0); */

    /* WIN32: wsprintfA(buf, "Layouts\\%s", layoutName)
     *   Builds a relative path using Windows path separator.  wsprintfA is
     *   the Win32 non-CRT printf variant — no locale effects, no width limit
     *   argument; the destination buffer must be large enough.
     * LINUX: snprintf(buf, sizeof buf, "Layouts/%s", layoutName)
     *        Use forward slash; POSIX path separator is '/'. */
    /* wsprintfA(buf, "Layouts\\%s", layoutName); */

    /* WIN32: PlaySoundA(NULL, NULL, SND_NODEFAULT=0)
     *   Passing NULL as the sound name immediately stops any .wav currently
     *   playing via a prior PlaySoundA(SND_ASYNC) call.  The flags argument
     *   of 0 means SND_SYNC (blocks until stopped), but with NULL it returns
     *   immediately.
     * LINUX: Mix_HaltChannel(-1)  — halts all active SDL_mixer channels */
    /* PlaySoundA(NULL, NULL, 0); */

    /* Mode-specific teardown also includes (depending on mode):
     *   FUN_0043d350() — save world state
     *   FUN_00420000() — reset UI to attract mode (mode 4)
     *   vtable destructors on scene objects (modes 5/6/7/9)
     *   FUN_00434800() — audio subsystem stop
     *   FUN_00423f80() — DirectDraw flip-chain stop
     *   FUN_0044dbd0() — timer service stop
     *   FUN_00461790() — engine state machine transition
     *   FUN_00411760() — UI cleanup
     * LINUX: replace DirectDraw stop with SDL_RenderClear + SDL_RenderPresent;
     *        use Mix_CloseAudio for audio stop. */

    (void)mode;
}

/* =========================================================================
 * FUN_00407ae0 (0x00407ae0) — Scroll position synchronisation
 *
 * Updates the Win32 horizontal scrollbar to reflect the current world-map
 * scroll position.  Only called when the game is in windowed (non-fullscreen)
 * mode.
 * ========================================================================= */
void FUN_00407ae0(HWND hWnd, int nMax, int pos)
{
    /* WIN32: SetScrollRange(hWnd, SB_HORZ=0, nMin=0, nMax, bRedraw=FALSE)
     *   Configures the valid range for the horizontal scrollbar thumb.
     *   bRedraw=FALSE defers the visual update until the subsequent
     *   SetScrollPos call, avoiding a double-redraw.
     * LINUX: SDL has no native scrollbar widget.  Track nMax as a plain int
     *        and pass it to the custom scrollbar sprite renderer:
     *        g_scrollMax = nMax; */
    /* SetScrollRange(hWnd, SB_HORZ, 0, nMax, FALSE); */

    /* WIN32: SetScrollPos(hWnd, SB_HORZ=0, nPos=pos, bRedraw=TRUE)
     *   Moves the scroll thumb to the given position within [0..nMax] and
     *   redraws the scrollbar.  Returns the previous position.
     * LINUX: g_scrollX = pos;
     *        // Render the custom scrollbar sprite at:
     *        // x = spriteLeft + (pos * spriteWidth / nMax)
     *        // y = spriteTop */
    /* SetScrollPos(hWnd, SB_HORZ, pos, TRUE); */

    (void)hWnd;
    (void)nMax;
    (void)pos;
}

/* =========================================================================
 * FUN_004077a0 (0x004077a0) — Engine shutdown / teardown
 *
 * Persists session state to lego.ini, waits for background threads, and
 * destroys all engine subsystems in reverse-creation order.
 * ========================================================================= */
void FUN_004077a0(CGWND *self)
{
    /* Persist window rect and CleanExit flag via the CIniFile layer:
     *   lego.ini [WINDOW_ATTRIBUTES] ← current window rect
     *   lego.ini [PROCESS]CleanExit  ← 1 (written on clean exit only)
     * LINUX: fprintf / fwrite to the INI file at SDL_GetPrefPath(). */

    /* WIN32: Sleep(100) in a busy-wait loop
     *   Polls g_workerRunning at 100 ms intervals until the background
     *   resource-loading thread finishes its current task.  Crude, but
     *   widely used in Win9x/XP-era game engines.
     * LINUX: pthread_join(g_workerThread, NULL) — blocks until the thread
     *        exits cleanly; no wasted CPU, no arbitrary timeout needed. */
    /* while (g_workerRunning) Sleep(100); */

    /* Destroy ~15 engine subsystem objects in reverse-creation order.
     * Each subsystem is destroyed via vtable slot 0 (destructor):
     *   (*(*subsystem)[0])(1)  — '1' = "destroy and free heap allocation"
     * Reverse order (prevents dangling back-references):
     *   g_pThumbnailMgr, g_pSaveGameMgr, g_pDebugLog, g_pStringTable,
     *   g_pEventQueue, g_pConfigMgr, g_pTimerSvc, g_pAnimMgr,
     *   g_pWorldMgr, g_pSceneMgr, g_pMovieMgr, g_pInputMgr,
     *   g_pNetworkMgr, g_pDirectSound, g_pDirectDraw
     * LINUX: call equivalent SDL_Destroy* / Mix_CloseAudio / etc. in order. */

    /* WIN32: CloseHandle(g_hGameLoopEvent)
     *   Releases the kernel object for the manual-reset "GameLoop" event.
     *   After this call the handle is invalid; do not use it again.
     * LINUX: sem_destroy(&g_gameSem)  or  close(g_eventFd) */
    CloseHandle(g_hGameLoopEvent);
    g_hGameLoopEvent = NULL;

    /* WIN32: timeKillEvent(g_timerID)
     *   Cancels the TIME_PERIODIC multimedia timer started in FUN_00406ba0.
     *   Must be called before the process exits; failure leaks the timer
     *   and it may fire into freed memory.
     * LINUX: SDL_RemoveTimer(g_sdlTimerID)
     *        or timer_delete(g_posixTimerId) for POSIX interval timers */
    timeKillEvent(g_timerID);
    g_timerID = 0;

    /* WIN32: timeEndPeriod(14)
     *   Restores the system-wide multimedia timer resolution to its default
     *   (typically 15.6 ms on Windows).  Must be paired with timeBeginPeriod
     *   called in FUN_00406ba0; unmatched calls corrupt the reference count.
     * LINUX: no-op — Linux does not have a global timer-resolution setting
     *        that requires per-process cleanup. */
    timeEndPeriod(14);

    (void)self;
}

/* =========================================================================
 * FUN_00408130 (0x00408130) — Display-mode state machine
 *
 * Transitions the engine between 10 display/game modes.  Each mode maps to
 * a specific subsystem configuration and renderer setup.
 *
 * Mode table:
 *   0    — (unspecified / unused)
 *   1    — initial load → FUN_00408350
 *   2    — windowed DirectDraw flip
 *   3    — mode transition via FUN_004086f0
 *   4    — reset to attract/idle
 *   5/6  — fullscreen 2-D tile gameplay
 *   7    — 3-D flying-vehicle view
 *   9    — mini-game screen
 *   0xa  — quit → PostMessageA(WM_QUIT)
 * ========================================================================= */
void FUN_00408130(int newMode)
{
    switch (newMode) {

    /* Mode 1 — Initial load
     *   Installs the 150 ms heartbeat timer and forces the first paint.
     * WIN32: FUN_00408350(g_pGameWnd->hwndGame);
     * LINUX: SDL_AddTimer(150, LoadHeartbeatCB, NULL);
     *        SDL_RenderPresent(g_sdlRenderer); */
    case 1:
        /* FUN_00408350(g_pGameWnd->hwndGame); */
        break;

    /* Mode 2 — Windowed DirectDraw flip
     *   Performs a DirectDraw Blt from back to primary surface in windowed mode.
     * WIN32: FUN_0045e1e0(0);   // → SendMessageA(hwnd, 0x407, 0, 0)
     *                            // → WndProc 0x407 → IDirectDrawSurface::Flip
     * LINUX: SDL_RenderPresent(g_sdlRenderer); */
    case 2:
        /* FUN_0045e1e0(0); */
        break;

    /* Mode 3 — State transition
     *   Tears down the current mode and posts an async locale-change event.
     * WIN32: FUN_004086f0(3);
     * LINUX: SDL_PushEvent with custom SDL_UserEvent type. */
    case 3:
        /* FUN_004086f0(3); */
        break;

    /* Mode 4 — Attract / idle reset
     *   Resets game objects to the idle/attract loop state.
     * WIN32: FUN_00420000();
     * LINUX: reset SDL_RenderTarget to main framebuffer; clear + present. */
    case 4:
        /* FUN_00420000(); */
        break;

    /* Modes 5 / 6 — Fullscreen 2-D tile gameplay
     *   Loads the tile-map renderer; switches to the primary 640x480 2-D view.
     * WIN32: DirectDraw SetDisplayMode + exclusive cooperative level.
     * LINUX: SDL_SetRenderTarget(g_sdlRenderer, g_tileMapTexture); */
    case 5:
    case 6:
        break;

    /* Mode 7 — 3-D flying-vehicle view
     *   Activates the software 3-D renderer for the flying-minifig camera.
     * WIN32: DirectDraw surface reconfiguration.
     * LINUX: SDL_SetRenderTarget to the 3-D scene render texture. */
    case 7:
        break;

    /* Mode 9 — Mini-game screen */
    case 9:
        break;

    /* Mode 0xa (10) — Quit
     *   Terminates the GetMessage loop by posting WM_QUIT.  GetMessage
     *   returns FALSE when it dequeues WM_QUIT, causing WinMain to exit.
     *
     * WIN32: PostMessageA(g_pGameWnd->hwndGame, WM_QUIT=0x12, 0, 0)
     *   PostMessageA is asynchronous — the message enters the queue and the
     *   current frame completes before the loop exits.
     * LINUX: SDL_Event ev;
     *        SDL_memset(&ev, 0, sizeof(ev));
     *        ev.type = SDL_QUIT;
     *        SDL_PushEvent(&ev);
     *   The SDL event loop checks for SDL_QUIT and breaks on the next
     *   SDL_PollEvent call, then calls SDL_Quit() and exits. */
    case 10:
        /* PostMessageA(g_pGameWnd->hwndGame, WM_QUIT, 0, 0); */
        break;

    default:
        break;
    }
}

/* =========================================================================
 * WndProc (0x004618c0) — Main window procedure
 *
 * Registered as WNDCLASSA.lpfnWndProc in FUN_00406ed0.
 * Ghidra initially tagged the bytes at 0x4618c0 as data (not a function);
 * the decompilation was forced via a headless script.
 *
 * Main HWND: *(HWND*)(DAT_004aa4a0 + 8) == g_pGameWnd->hwndGame
 *
 * Game state (g_gameState / DAT_004851f4):
 *   0  = startup    1  = main menu   2  = loading
 *   3  = play       4  = dialog      8  = sub-dialog   10 = quit
 *
 * Mouse / keyboard state cache (written here, read by game logic):
 *   DAT_00485556   — mouse-moved pending flag (1 = move pending)
 *   DAT_00485558   — cached lParam from WM_MOUSEMOVE
 *   DAT_004855ae   — left-button-held flag
 *   DAT_0048556c   — left-button-down pending flag
 *   piRam00485570  — cached lParam from WM_LBUTTONDOWN
 *   DAT_0048558c   — left-button-up pending flag
 *   piRam00485590  — cached lParam from WM_LBUTTONUP
 *   DAT_0048557c   — right-button-down pending flag
 *   piRam00485580  — cached lParam from WM_RBUTTONDOWN
 *   uRam0048559c   — right-button-up pending flag
 *   piRam004855a0  — cached lParam from WM_RBUTTONUP
 *   DAT_004851f0   — minimised flag (0=normal, 1=iconified)
 *
 * SDL2 message → event mapping summary:
 *
 *   WM_DESTROY    0x002  return 0; never PostQuitMessage
 *   WM_CLOSE      0x010  state<=2: error dialog+quit; else graceful close
 *   WM_PAINT      0x00F  state<=2: BltFast loading frame; else scroll-offset blit
 *   WM_MOUSEMOVE  0x200  state<=2: SetCursor(NULL); state==8: fwd child; else cache
 *   WM_LBUTTONDOWN 0x201 state<=2: swallow; else SetFocus+flags+cache
 *   WM_LBUTTONDBLCLK    same path as WM_LBUTTONDOWN
 *   WM_LBUTTONUP  0x202  clear held, set up-pending, cache pos
 *   WM_RBUTTONDOWN 0x204 state<=2: swallow; else set flags+cache
 *   WM_RBUTTONDBLCLK    same path as WM_RBUTTONDOWN
 *   WM_RBUTTONUP  0x205  set up-pending, cache pos
 *   WM_KEYDOWN    0x100  state<=2: swallow; 0x20-0x5A: widget+RETURN/ESC
 *   WM_KEYUP      0x101  DefWindowProc only
 *   WM_CHAR       0x102  0x20-0x7E: uppercase; Q→WM_CLOSE; W→special
 *   WM_SIZE       0x005  RESTORED→scrollbars; MINIMIZED→hide; MAXIMIZED→prevent
 *   WM_ACTIVATE   0x006  DefWindowProc (screensaver intercept separate)
 *   WM_DISPLAYCHANGE     resolution mismatch → hide + error + quit
 *   WM_TIMER(0x47) 0x113 g_renderDirty=1 (150 ms heartbeat)
 *   WM_SYSCOMMAND 0x112  SC_MAXIMIZE→toggle; SC_CLOSE→WM_CLOSE; SC_KEYMENU→special
 *   0x405 WM_USER+5      Sleep(20) + DirectDraw surface recovery
 *   0x406 WM_USER+6      FUN_00447400 locale/language switch
 *   0x407 WM_USER+7      func_0x0045e210(wParam) → DirectDraw Flip
 *   0x401 WM_USER+1      game subsystem command dispatcher
 * ========================================================================= */
#ifndef LOCO_LINUX
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {

    /* ── WM_DESTROY (0x002) ───────────────────────────────────────────────
     * The game never uses WM_DESTROY for cleanup; real shutdown flows from
     * WM_CLOSE → func_0x00463430().  Return 0 immediately in all states.
     *
     * WIN32: return 0  (DefWindowProcA is NOT called)
     * LINUX: SDL_QUIT event is posted by the WM_CLOSE handler; no WM_DESTROY
     *        equivalent exists in SDL. */
    case WM_DESTROY:
        return 0;

    /* ── WM_PAINT (0x00F) ────────────────────────────────────────────────
     * Two rendering paths depending on game state.
     *
     * State <= 2 (startup/menu/loading):
     *   WIN32: BeginPaint(hWnd, &ps)
     *          → FUN_00401280(dirtyRect, hWnd, NULL, FALSE)
     *            (FUN_00401280 performs IDirectDrawSurface::BltFast to primary)
     *          → EndPaint(hWnd, &ps)
     *   LINUX: SDL_RenderPresent(g_sdlRenderer);  // dirty rect from state
     *
     * Active state (>= 3):
     *   WIN32: BeginPaint(hWnd, &ps) + EndPaint
     *          Rebuild client rect from DAT_00485220-2C.
     *          Apply scroll offsets DAT_004aad24 (horiz) / DAT_004aad28 (vert).
     *          FUN_00401280(offsetRect, hWnd, &scrollOffset, FALSE)
     *   LINUX: SDL_RenderCopy with src/dst SDL_Rect adjusted for scroll offset. */
    case WM_PAINT: {
        /* PAINTSTRUCT ps; */
        /* WIN32: BeginPaint(hWnd, &ps)
         * LINUX: not needed; SDL_RenderPresent replaces the paint cycle. */
        /* HDC hdc = BeginPaint(hWnd, &ps); */
        if (g_gameState <= 2) {
            /* FUN_00401280(&dirtyRect, hWnd, NULL, 0); */
        } else {
            /* Rebuild rect from stored client area DAT_00485220-2C.
             * Apply DAT_004aad24 / DAT_004aad28 scroll offsets.
             * LINUX: SDL_Rect src = g_clientRect; src.x += g_scrollX;
             *        src.y += g_scrollY;
             *        SDL_RenderCopy(renderer, g_backTex, &src, &g_clientRect); */
            /* FUN_00401280(&offsetRect, hWnd, &scrollOffset, 0); */
        }
        /* WIN32: EndPaint(hWnd, &ps)
         * LINUX: not needed. */
        /* EndPaint(hWnd, &ps); */
        return 0;
    }

    /* ── WM_MOUSEMOVE (0x200) ────────────────────────────────────────────
     * SDL2: SDL_MOUSEMOTION → ev.motion.x / ev.motion.y */
    case WM_MOUSEMOVE:
        if (g_gameState <= 2) {
            /* Loading state: hide the OS cursor.
             * WIN32: SetCursor(NULL)
             *   Immediately removes the cursor from the screen.
             *   The cursor reappears on the next WM_SETCURSOR if not suppressed.
             * LINUX: SDL_ShowCursor(SDL_DISABLE) */
            /* SetCursor(NULL); */
            return 0;  /* LAB_00461bc0 */
        }
        if (g_gameState == 8) {
            /* Sub-dialog state: forward to child window.
             * WIN32: PostMessageA(*(HWND*)((char*)g_pSceneMgr+8),
             *            WM_MOUSEMOVE, wParam, lParam)
             *   Then fall through to DefWindowProcA.
             * LINUX: translate (x,y) into child widget coordinate space;
             *        call child_widget->on_mouse_move(x, y) directly. */
            /* PostMessageA(*(HWND*)((char*)g_pSceneMgr+8),
             *              WM_MOUSEMOVE, wParam, lParam); */
            break;  /* then DefWindowProc */
        }
        /* Normal play: record mouse-moved flag and cache packed position.
         * DAT_00485556 = 1;        // mouse-moved pending
         * DAT_00485558 = lParam;   // packed (x,y) = MAKEPOINTS(lParam)
         *   LOWORD(lParam) = x,  HIWORD(lParam) = y
         * LINUX: g_mouseState.moved = 1;
         *        g_mouseState.x = ev.motion.x;
         *        g_mouseState.y = ev.motion.y; */
        /* if (g_gameState == 4) { FUN_00410840(...); } */
        break;

    /* ── WM_LBUTTONDOWN (0x201) + WM_LBUTTONDBLCLK (0x203) ─────────────
     * SDL2: SDL_MOUSEBUTTONDOWN with button == SDL_BUTTON_LEFT
     *       WM_LBUTTONDBLCLK: same SDL event but ev.button.clicks == 2 */
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        if (g_gameState <= 2) return 0;  /* swallow during loading */
        /* WIN32: SetFocus(hWnd)
         *   Directs keyboard input to this window.
         * LINUX: SDL_RaiseWindow(g_sdlWindow) */
        /* SetFocus(hWnd); */
        /* WIN32: SetForegroundWindow(hWnd)
         *   Brings the window to the top of the z-order and activates it.
         * LINUX: SDL_RaiseWindow handles both focus and foreground; SDL
         *        does not distinguish them as separate operations. */
        /* SetForegroundWindow(hWnd); */
        /* DAT_004855ae = 1;       // left-button-held flag
         * DAT_0048556c = 1;       // left-button-down pending
         * piRam00485570 = lParam; // packed (x,y) cursor position
         * LINUX: g_mouseState.leftHeld = 1;
         *        g_mouseState.leftDown = 1;
         *        g_mouseState.leftPos  = {ev.button.x, ev.button.y}; */
        break;  /* then DefWindowProc */

    /* ── WM_LBUTTONUP (0x202) ────────────────────────────────────────────
     * SDL2: SDL_MOUSEBUTTONUP with button == SDL_BUTTON_LEFT */
    case WM_LBUTTONUP:
        /* DAT_004855ae = 0;        // clear held flag
         * DAT_0048558c = 1;        // left-button-up pending
         * piRam00485590 = lParam;  // release position
         * LINUX: g_mouseState.leftHeld = 0;
         *        g_mouseState.leftUp   = 1;
         *        g_mouseState.leftPos  = {ev.button.x, ev.button.y}; */
        break;  /* then DefWindowProc */

    /* ── WM_RBUTTONDOWN (0x204) + WM_RBUTTONDBLCLK (0x206) ─────────────
     * SDL2: SDL_MOUSEBUTTONDOWN with button == SDL_BUTTON_RIGHT */
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        if (g_gameState <= 2) return 0;
        /* DAT_0048557c = 1;       // right-button-down pending
         * piRam00485580 = lParam; // packed (x,y) cursor position
         * LINUX: g_mouseState.rightDown = 1;
         *        g_mouseState.rightPos  = {ev.button.x, ev.button.y}; */
        break;  /* then DefWindowProc */

    /* ── WM_RBUTTONUP (0x205) ────────────────────────────────────────────
     * SDL2: SDL_MOUSEBUTTONUP with button == SDL_BUTTON_RIGHT */
    case WM_RBUTTONUP:
        /* uRam0048559c   = 1;      // right-button-up pending
         * piRam004855a0  = lParam; // release position
         * LINUX: g_mouseState.rightUp  = 1; */
        break;  /* then DefWindowProc */

    /* ── WM_KEYDOWN (0x100) ──────────────────────────────────────────────
     * SDL2: SDL_KEYDOWN → ev.key.keysym.sym */
    case WM_KEYDOWN:
        if (g_gameState <= 2) return 0;  /* swallow during loading */
        if (wParam >= 0x20 && wParam <= 0x5A) {
            /* Pass to focused widget handler via vtable slot 0x10 (byte offset
             * 0x40 / sizeof(ptr) = slot 16 at 4-byte stride).
             * WIN32: (*(*DAT_004fd3e0)[0x10])(DAT_004fd3e0, wParam, lParam)
             * LINUX: if (g_pFocusWidget)
             *            WidgetManager_OnKeyDown(g_pFocusWidget, sdlSym);
             * If the widget returns non-zero (consumed), skip the global handlers. */

            if (wParam == VK_RETURN && g_gameState == 3) {
                /* RETURN in play state: trigger save + state advance.
                 * FUN_00454680();   // CSaveGame: write world to disk
                 * FUN_00408130(4);  // SetGameState → mode 4 (dialog/idle) */
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                /* WIN32: PostMessageA(hWnd, WM_CLOSE, 0, 0)
                 *   Posts WM_CLOSE asynchronously.
                 * LINUX: SDL_Event ev = {.type=SDL_QUIT}; SDL_PushEvent(&ev); */
                /* PostMessageA(hWnd, WM_CLOSE, 0, 0); */
                return 0;
            }
        } else {
            /* Keys outside 0x20–0x5A: only VK_ESCAPE and a sentinel value
             * 0x564b5f51 (likely a debug key combo) trigger WM_CLOSE. */
            if (wParam == VK_ESCAPE || wParam == 0x564b5f51) {
                /* PostMessageA(hWnd, WM_CLOSE, 0, 0); */
                return 0;
            }
        }
        break;

    /* ── WM_KEYUP (0x101) ────────────────────────────────────────────────
     * No explicit handler — falls directly to DefWindowProcA.
     * SDL2: SDL_KEYUP — no special action required. */
    case WM_KEYUP:
        break;

    /* ── WM_CHAR (0x102) ─────────────────────────────────────────────────
     * Printable ASCII only (0x20–0x7E).
     * SDL2: SDL_TEXTINPUT → ev.text.text[0] for single-byte characters.
     *       Or SDL_KEYDOWN with ev.key.keysym.sym for printable key handling. */
    case WM_CHAR: {
        UINT ch = (UINT)wParam;
        if (ch < 0x20 || ch > 0x7E) break;
        /* Lowercase a–z (0x61–0x7A) → uppercase via FUN_00467710.
         * FUN_00467710 is a locale-aware uppercase helper.
         * LINUX: ch = (unsigned char)toupper((int)ch); */
        if (ch >= 0x61 && ch <= 0x7A) {
            /* ch = FUN_00467710(ch); */
            ch = ch - 0x20;  /* fast ASCII-range uppercase */
        }
        if (ch == 0x51 /* 'Q' */) {
            /* WIN32: PostMessageA(hWnd, WM_CLOSE, 0, 0)
             * LINUX: SDL_Event ev = {.type=SDL_QUIT}; SDL_PushEvent(&ev); */
            /* PostMessageA(hWnd, WM_CLOSE, 0, 0); */
            return 0;
        }
        if (ch == 0x57 /* 'W' */) {
            /* FUN_00407d00() — windowed-mode toggle or debug action.
             * WIN32: DefWindowProcA(hWnd, WM_CHAR, 'W', lParam) called after.
             * LINUX: handle_debug_key_W(); */
            /* FUN_00407d00(); */
            /* DefWindowProcA(hWnd, WM_CHAR, 'W', lParam); */
            return 0;
        }
        break;
    }

    /* ── WM_SIZE (0x005) ─────────────────────────────────────────────────
     * SDL2: SDL_WINDOWEVENT_RESIZED / SDL_WINDOWEVENT_MINIMIZED /
     *       SDL_WINDOWEVENT_RESTORED */
    case WM_SIZE:
        switch ((int)wParam) {
        case SIZE_RESTORED:   /* 0 — window returned to normal size */
            /* DAT_004851f0 = 0; */
            /* FUN_00447930(0x5467);  // show scrollbars
             * LINUX: SDL_ShowWindow(g_sdlWindow);
             *        update scroll widget visibility. */
            break;
        case SIZE_MINIMIZED:  /* 1 — window iconified */
            /* DAT_004851f0 = 1; */
            /* FUN_00447930(0x5465);  // hide scrollbars
             * LINUX: SDL_MinimizeWindow(g_sdlWindow); */
            break;
        case SIZE_MAXIMIZED:  /* 2 — maximise is forbidden; force restore */
            /* WIN32: SendMessageA(hWnd, WM_SYSCOMMAND, SC_RESTORE=0xF120, 0)
             *        PostMessageA(hWnd, WM_SYSCOMMAND, 0xF030, 0)
             * LINUX: SDL_RestoreWindow(g_sdlWindow) */
            /* SendMessageA(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0); */
            /* PostMessageA(hWnd, WM_SYSCOMMAND, 0xF030, 0); */
            break;
        }
        /* All paths: if the window has a scrollable region, update it.
         * WIN32: FUN_00436d60(g_pAnimMgr);
         * LINUX: recalculate scroll bounds from SDL_GetWindowSize result. */
        break;

    /* ── WM_ACTIVATE (0x006) ─────────────────────────────────────────────
     * No explicit handler in the main dispatch — falls through to
     * DefWindowProcA.  In screensaver mode (g_debugMode==1), a separate
     * filter at func_0x004484a0 intercepts this message before WndProc.
     * SDL2: SDL_WINDOWEVENT_FOCUS_GAINED / SDL_WINDOWEVENT_FOCUS_LOST */
    case WM_ACTIVATE:
        break;

    /* ── WM_CLOSE (0x010) ────────────────────────────────────────────────
     * SDL2: SDL_QUIT event → call cleanup then SDL_Quit() */
    case WM_CLOSE:
        if (g_gameState <= 2) {
            if (wParam != 0) {
                /* Load and show a localised crash/error dialog.
                 * Resource 0x14a = error string "An error occurred…"
                 * WIN32: MessageBoxA(hWnd, errorStr, "LEGO LOCO",
                 *            MB_ICONERROR=0x10)
                 * LINUX: SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                 *            "LEGO LOCO", errorStr, g_sdlWindow) */
                /* MessageBoxA(hWnd, DAT_error, "LEGO LOCO", 0x10); */
            }
            /* func_0x00463430(); — hard quit (no further cleanup) */
            return 0;
        }
        if (g_gameState != 10 /* && !g_debugMode */) {
            /* Graceful close through the scene manager.
             * FUN_00436ec0(g_pAnimMgr, 0, 0);
             * LINUX: SDL_PushEvent with SDL_QUIT type so the main event loop
             *        can call subsystem destructors in the correct order. */
            /* FUN_00436ec0(g_pAnimMgr, 0, 0); */
        }
        return 0;

    /* ── WM_DISPLAYCHANGE (0x07E) ────────────────────────────────────────
     * Fired by Windows when the display resolution or colour depth changes
     * while the game is running.
     * SDL2: SDL_WINDOWEVENT_DISPLAY_CHANGED */
    case WM_DISPLAYCHANGE:
        /* Check whether the new resolution matches the game's requirements.
         * g_screenWidth  = DAT_004851d8, g_screenHeight = DAT_00485214,
         * colour depth   = DAT_0048521c.
         * If changed:
         *   WIN32: ShowWindow(hWnd, SW_HIDE)
         *          MessageBoxA(NULL, resChangeErr, "LEGO LOCO", 0)
         *          func_0x00463430()
         *   LINUX: SDL_HideWindow(g_sdlWindow);
         *          SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
         *              "LEGO LOCO", resChangeErr, NULL);
         *          SDL_PushEvent({.type=SDL_QUIT}); */
        break;

    /* ── WM_TIMER (0x113) — id=0x47 (71) ────────────────────────────────
     * 150 ms render-heartbeat installed by FUN_00408350 via SetTimer.
     * Sets g_renderDirty=1 so the main message loop triggers a frame present
     * on the next iteration.
     * SDL2: SDL_AddTimer(150, HeartbeatCB, NULL); in cb: g_renderDirty = 1; */
    case WM_TIMER:
        if (wParam == LOCO_HEARTBEAT_TIMER_ID) {
            g_renderDirty = 1;
        }
        return 0;

    /* ── WM_SYSCOMMAND (0x112) ───────────────────────────────────────────
     * SDL2: SDL handles SC_CLOSE as SDL_WINDOWEVENT_CLOSE internally.
     *       SDL_MaximizeWindow / SDL_RestoreWindow for SC_MAXIMIZE. */
    case WM_SYSCOMMAND:
        switch (wParam & 0xFFF0) {
        case SC_MAXIMIZE:  /* 0xF030 */
            /* Conditionally allow or prevent maximise based on DAT_00485210.
             * if (DAT_00485210): FUN_00407d20(1) — handle maximise
             * else: prevent.
             * LINUX: if (allowMaximize) SDL_MaximizeWindow(g_sdlWindow);
             *        else SDL_RestoreWindow(g_sdlWindow); */
            return 0;
        case SC_CLOSE:     /* 0xF060 — close button / Alt-F4 */
            /* WIN32: PostMessageA(hWnd, WM_CLOSE, 0, 0)
             * LINUX: SDL_Event ev = {.type=SDL_QUIT}; SDL_PushEvent(&ev); */
            /* PostMessageA(hWnd, WM_CLOSE, 0, 0); */
            return 0;
        case SC_KEYMENU:   /* 0xF140 — ALT key activates menu bar */
            /* FUN_00463670() — custom ALT key interceptor */
            break;  /* then DefWindowProc */
        }
        break;

    /* ── 0x405 (WM_USER+5) — DirectDraw surface recovery ─────────────────
     * Sent when the DirectDraw surfaces are lost (e.g. after Alt+Tab).
     * SDL2: SDL_Delay(20); then re-create the SDL_Texture from the asset. */
    case WM_LOCO_SURFACE_LOST:   /* 0x0405 */
        /* WIN32: Sleep(20)
         *   Gives the display driver time to reclaim VRAM before Restore.
         * LINUX: SDL_Delay(20) */
        /* Sleep(20); */
        /* func_0x0045e400() — IDirectDrawSurface::Restore + re-blit from disk
         * LINUX: SDL_DestroyTexture(g_gameTexture);
         *        g_gameTexture = SDL_CreateTexture(g_sdlRenderer,
         *            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
         *            g_screenWidth, g_screenHeight); */
        return 0;

    /* ── 0x406 (WM_USER+6) — Locale / language switch ────────────────────
     * Posted asynchronously from FUN_004086f0 via PostMessageA.
     * SDL2: custom SDL_UserEvent registered with SDL_RegisterEvents(1). */
    case WM_LOCO_LOCALE_CHANGE:  /* 0x0406 */
        /* FUN_00447400() — reload and re-cache all localised string tables */
        return 0;

    /* ── 0x407 (WM_USER+7) — DirectDraw page flip / frame present ─────────
     * Sent SYNCHRONOUSLY from FUN_0045e1e0 via SendMessageA.  WndProc runs
     * inline inside SendMessageA and returns only after the Flip completes.
     * SDL2: call SDL_RenderPresent(g_sdlRenderer) directly (also synchronous).
     *       No event needed; just make it a direct function call. */
    case WM_LOCO_RENDER_FLIP:    /* 0x0407 */
        /* func_0x0045e210((UINT)wParam) — IDirectDrawSurface::Flip(NULL, flags)
         *   wParam = flipFlags byte (0 = normal flip, other values are driver hints)
         * LINUX: SDL_RenderPresent(g_sdlRenderer);
         *        g_renderDirty = 0; */
        return 0;

    /* ── 0x401 (WM_USER+1) — Game subsystem command dispatcher ───────────
     * wParam carries a sub-command identifier.
     *   case 5:  multiplayer session event
     *   case 6:  save-game completion notification
     *   case 7:  load-game completion notification
     *   case 8:  network player joined
     *   case 9:  network player left
     *   case 10: score update
     *   case 11: chat message received
     * SDL2: custom SDL_UserEvent with ev.user.code = wParam. */
    case WM_LOCO_SUBSYS_CMD:     /* 0x0401 */
        /* switch ((int)wParam) { ... } */
        return 0;

    default:
        break;
    }

    /* WIN32: DefWindowProcA(hWnd, uMsg, wParam, lParam)
     *   Default OS processing for all unhandled messages (sets cursor,
     *   handles Alt-Space system menu, WM_NCPAINT, WM_GETTEXT, etc.).
     * LINUX: the SDL_PollEvent loop simply does nothing for unrecognised
     *        event types; there is no DefWindowProc equivalent. */
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}
#endif /* !LOCO_LINUX */

/* =========================================================================
 * FUN_0045e1e0 (0x0045e1e0) — Render / flip request shim
 *
 * Sends the synchronous render-flip message to the main HWND.  Called after
 * each subsystem init in FUN_00408350 to show incremental loading progress,
 * and from the main game loop whenever g_renderDirty is set.
 *
 * The use of SendMessageA (not PostMessageA) is intentional: the call must
 * block until the DirectDraw Flip completes before the caller can proceed.
 * ========================================================================= */
void FUN_0045e1e0(int flipFlags)
{
    /* WIN32: SendMessageA(*(HWND*)((char*)g_pGameWnd+8), 0x407,
     *                     (WPARAM)(flipFlags & 0xFF), 0)
     *   Delivers WM_USER+7 synchronously to WndProc.  WndProc case 0x407
     *   calls func_0x0045e210((UINT)wParam) which invokes:
     *     IDirectDrawSurface::Flip(NULL, DDFLIP_WAIT)
     *   or the Blt-to-primary variant depending on cooperative level.
     *   The & 0xFF masks the flip-flags byte; 0 = normal flip.
     *   SendMessageA returns only after WndProc returns, guaranteeing that
     *   the flip is done before this function returns.
     *
     * LINUX: SDL_RenderPresent(g_sdlRenderer)
     *   SDL_RenderPresent is also synchronous on most platforms (blocks until
     *   the GPU has consumed the command buffer or vsync if SDL_RENDERER_PRESENTVSYNC
     *   is set).  No event push is needed; just call it directly.
     *   After the call: g_renderDirty = 0; */

    /* Complete SDL2 replacement:
     *   void FUN_0045e1e0_linux(int flipFlags) {
     *       (void)flipFlags;
     *       SDL_RenderPresent(g_sdlRenderer);
     *       g_renderDirty = 0;
     *   }
     */
    (void)flipFlags;
}

/* =========================================================================
 * FUN_00408b20 (0x00408b20) — UI widget constructor
 *
 * Initialises a UI scroll-widget object (~0x250 bytes).  Allocates five
 * navigation-arrow sprite button sub-objects and nine scrollable item buttons.
 * Wrapped in SEH (structured exception handling) to catch resource-load failures.
 * ========================================================================= */
void *FUN_00408b20(void *self)
{
    if (!self) return NULL;

    /* Zero the widget's item-array region and double-buffer flag.
     * Key offsets zeroed:
     *   +0xec–0x11c  — item array / counts / cached data
     *   +0x1b4       — scroll state flags
     *   +0x21c       — double-buffer enable flag
     * LINUX: memset(widget->itemArray, 0, sizeof(widget->itemArray)); */
    memset((char *)self + 0xec, 0, 0x30);
    /* *(int*)((char*)self + 0x1b4) = 0; */
    /* *(int*)((char*)self + 0x21c) = 0; */

    /* Set default scroll alignment: 3 = top-align.
     *   0 = right-align, 1 = left-align, 2 = bottom-align, 3 = top-align
     * LINUX: widget->scrollAlign = 3; */
    /* *(int*)((char*)self + 0x1b0) = 3; */

    /* Allocate and load 5 navigation-arrow sprite buttons.
     * Stored at this+0x220, +0x224, +0x228, +0x22c, +0x230
     * (pointers in CGWND-style void* fields at offsets 0x88–0x8c).
     * Resource IDs 0x429–0x42d = up/down/left/right/more arrows.
     *
     *   FUN_00465ce0(0x24) — allocates a 36-byte sprite-button object
     *   FUN_00454b50(btn, resId) — loads the BMP sprite resource
     *
     * LINUX:
     *   for (int i = 0; i < 5; i++) {
     *       const char *path = g_arrowSpritePaths[i];   // pre-extracted BMPs
     *       SDL_Surface *s = SDL_LoadBMP(path);
     *       g_arrowTextures[i] = SDL_CreateTextureFromSurface(g_sdlRenderer, s);
     *       SDL_FreeSurface(s);
     *   } */

    /* Allocate and load 9 scrollable item buttons.
     * Stored at this+0x23c array (offsets 0x8f–0x97 as void* pointers).
     * Resource IDs 0x43a–0x442.
     *
     * LINUX: same SDL_CreateTextureFromSurface pattern for each slot. */

    return self;
}

/* =========================================================================
 * FUN_00408d10 (0x00408d10) — UI widget destructor
 *
 * Frees all child objects and removes the widget from the UI manager list.
 * ========================================================================= */
void FUN_00408d10(void *self)
{
    if (!self) return;

    /* Walk linked list at param_1[0x3b] (first child-item list).
     * Node layout:
     *   [0] = next-node pointer (void*)
     *   [2] = optional child data pointer — freed if non-null
     *
     * WIN32: internal allocator free (operator delete / HeapFree)
     * LINUX:
     *   LocoListNode *node = widget->itemList1;
     *   while (node) {
     *       LocoListNode *next = node->next;
     *       if (node->data) free(node->data);
     *       free(node);
     *       node = next;
     *   }
     *   widget->itemList1 = NULL; */

    /* Walk linked list at param_1[0x3c] (second child-item list).
     * Same structure as the first list.
     * LINUX: walk widget->itemList2 identically. */

    /* If arrow-button flag is set at param_1+0x87:
     * Destroy 5 arrow sprite buttons via vtable slot 2 (destructor+free).
     *   (*(*btn)[2])(btn, 1)  — slot 2 = "destroy and free" destructor
     *
     * WIN32: vtable call on each of the 5 buttons at this+0x88..0x8c
     * LINUX: for (int i = 0; i < 5; i++) {
     *            SDL_DestroyTexture(g_arrowTextures[i]);
     *            free(g_arrowBtns[i]);
     *            g_arrowBtns[i] = NULL;
     *        }
     * Then 9 scrollable items at this+0x8f..0x97:
     * LINUX: same SDL_DestroyTexture + free pattern. */

    /* Remove this widget from the global UI widget list.
     * WIN32: FUN_004472b0(self, 0x5015)
     *   Walks the UI manager linked list, finds this widget by class ID 0x5015,
     *   and unlinks it.
     * LINUX: LinkedList_Remove(&g_uiWidgetList, self); */

    /* Call base-class destructor.
     * WIN32: FUN_00425910(self)  — CWidget base class dtor
     * LINUX: BaseWidget_Dtor(self); */
}

/* =========================================================================
 * FUN_004091a0 (0x004091a0) — UI widget layout calculation
 *
 * Computes bounding rects for two child sprite widgets (the "+ / -" or
 * left/right navigation pair) and caches the combined result in the parent
 * widget's layout fields.  Pure arithmetic; no Win32 API calls.
 * ========================================================================= */
void FUN_004091a0(void *self, int baseX)
{
    /* Read child sprite texture dimensions from the sprite header structs.
     * child = *(void**)((char*)self + 0x228);   // child sprite 1
     * childW1 = *(int16_t*)((char*)*(void**)((char*)child+0x14) + 0x14);
     * childH1 = *(int16_t*)((char*)*(void**)((char*)child+0x14) + 0x16);
     *
     * LINUX: int childW1, childH1;
     *        SDL_QueryTexture(child1Tex, NULL, NULL, &childW1, &childH1); */

    /* Child 1 bounding rect (navigation button, e.g. scroll-left arrow):
     *   left   = baseX + 13
     *   top    = *(int*)((char*)self + 0x1e8) + 13
     *   right  = left + childW1
     *   bottom = top  + childH1
     *
     * LINUX: SDL_Rect child1Rect = {
     *     .x = baseX + 13,
     *     .y = widget->baseY + 13,
     *     .w = childW1,
     *     .h = childH1
     * }; */

    /* Child 2 bounding rect (scroll-right arrow) placed to the right of child 1:
     *   left   = child1.right + 7
     *   top    = child1.top
     *   right  = left + childW2
     *   bottom = top  + childH2
     *
     * LINUX: SDL_Rect child2Rect = {
     *     .x = child1Rect.x + child1Rect.w + 7,
     *     .y = child1Rect.y,
     *     .w = childW2,
     *     .h = childH2
     * }; */

    /* Store individual child rects into a local layout struct (iVar4):
     *   iVar4+4  = child1.left
     *   iVar4+8  = child1.top
     *   iVar4+0xc = child1.right
     *   iVar4+0x10 = child1.bottom
     * Then store combined layout into the parent widget cache:
     *   this+0x20c = combined left   (child1.left)
     *   this+0x210 = combined top    (child1.top)
     *   this+0x214 = combined right  (child2.right)
     *   this+0x218 = combined bottom (max(child1.bottom, child2.bottom))
     *
     * LINUX: widget->layoutRect = SDL_Rect union of child1Rect and child2Rect:
     *   widget->layoutRect.x = child1Rect.x;
     *   widget->layoutRect.y = child1Rect.y;
     *   widget->layoutRect.w = (child2Rect.x + child2Rect.w) - child1Rect.x;
     *   widget->layoutRect.h = SDL_max(child1Rect.h, child2Rect.h); */

    (void)self;
    (void)baseX;
}

/* =========================================================================
 * FUN_004094b0 (0x004094b0) — Listbox / save-game-list paint handler
 *
 * Draws the save-game selection list using GDI text rendering onto a
 * DirectDraw surface.  Called from FUN_00409280 on WM_PAINT for the listbox
 * region.
 * ========================================================================= */
void FUN_004094b0(void *self, void *itemList)
{
    /* WIN32: FUN_00426b00(self) — internal GetDC wrapper.
     *   Calls IDirectDrawSurface::GetDC() to obtain an HDC compatible
     *   with the DirectDraw back surface, then SelectObject(hdc, font).
     * LINUX: not needed; SDL_ttf renders directly to SDL_Surface / Texture.
     *        TTF_Font *font = g_listboxFont;
     *        SDL_Color fgColour; */

    /* Draw header text above the list.
     * WIN32: DrawTextA(hdc, DAT_0047e2c8, -1, &headerRect, DT_CALCRECT)
     *   DT_CALCRECT measures the text bounding box without drawing; the
     *   returned rect is used to position the list items below the header.
     *   Then DrawTextA again without DT_CALCRECT to actually render.
     * LINUX: int headerW, headerH;
     *        TTF_SizeUTF8(font, g_listboxHeader, &headerW, &headerH);
     *        SDL_Surface *hdrSurf = TTF_RenderUTF8_Blended(
     *            font, g_listboxHeader, headerColour);
     *        SDL_Texture *hdrTex = SDL_CreateTextureFromSurface(renderer, hdrSurf);
     *        SDL_Rect hdrDst = {listLeft, listTop, headerW, headerH};
     *        SDL_RenderCopy(renderer, hdrTex, NULL, &hdrDst);
     *        SDL_DestroyTexture(hdrTex); SDL_FreeSurface(hdrSurf); */

    /* Iterate the save-game linked list.
     * Node layout: [0]=next ptr (void*), [2]=name string (const char*).
     *
     * for each node while itemTop < listBottom:
     *   selectedIdx = *(int*)((char*)self + 0xf4);
     *
     *   Colour selection:
     *   WIN32: SetTextColor(hdc,
     *       (index == selectedIdx) ? 0x2525dc : 0xff5c00)
     *     GDI COLORREF is 0x00BBGGRR:
     *       0x2525dc → R=0xdc G=0x25 B=0x25 (blue)
     *       0xff5c00 → R=0x00 G=0x5c B=0xff (orange)
     *   LINUX: SDL_Color itemColour = (index == selectedIdx)
     *              ? (SDL_Color){0xdc, 0x25, 0x25, 0xff}   // blue  [#DC2525]
     *              : (SDL_Color){0x00, 0x5c, 0xff, 0xff};  // orange [#005CFF]
     *
     *   WIN32: DrawTextA(hdc, name, -1, &itemRect, DT_LEFT)
     *   LINUX: SDL_Surface *s = TTF_RenderUTF8_Blended(font, name, itemColour);
     *          SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
     *          SDL_RenderCopy(renderer, t, NULL, &itemRect);
     *          SDL_DestroyTexture(t); SDL_FreeSurface(s); */

    /* If no items were drawn, show the "empty list" placeholder string.
     * WIN32: LoadStringA(hInst, 0x7f, emptyStr, sizeof(emptyStr))
     *        DrawTextA(hdc, emptyStr, -1, &listRect, DT_CENTER|DT_VCENTER)
     *   DT_CENTER | DT_VCENTER centres the text in listRect.
     * LINUX: SDL_Surface *s = TTF_RenderUTF8_Blended(
     *            font, g_emptyListString, greyColour);
     *        int tw, th; SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
     *        SDL_Rect dst = {
     *            listRect.x + (listRect.w - tw) / 2,
     *            listRect.y + (listRect.h - th) / 2,
     *            tw, th
     *        };
     *        SDL_RenderCopy(renderer, tex, NULL, &dst); */

    /* Trigger DirectDraw blit of the rendered HDC content to the primary surface.
     * WIN32: FUN_00426b90(self) — wraps IDirectDrawSurface::Blt(NULL, src, NULL,
     *            DDBLT_WAIT | DDBLT_COLORFILL_or_keyed, NULL)
     * LINUX: SDL_RenderPresent(g_sdlRenderer)
     *        (or SDL_RenderCopy if this is an off-screen render texture) */

    (void)self;
    (void)itemList;
}

/* =========================================================================
 * FUN_00409770 (0x00409770) — Single-line text widget draw handler
 *
 * Renders a single text string within the widget's bounding rect, with
 * four-way scroll-alignment support.  Called from FUN_00409360.
 * ========================================================================= */
void FUN_00409770(void *self)
{
    /* const char *text = *(char**)((char*)self + 0x120); */
    /* int boundL = *(int*)((char*)self + 0x1fc);  // left  */
    /* int boundT = *(int*)((char*)self + 0x200);  // top   */
    /* int boundR = *(int*)((char*)self + 0x204);  // right */
    /* int boundB = *(int*)((char*)self + 0x208);  // bottom */
    /* int scrollAlign = *(int*)((char*)self + 0x1b0); */

    /* Double-buffered path: if this+0x21c != 0, blit back→front before redraw.
     * WIN32: FUN_0042b050(backSurf, frontSurf, &srcRect, dstX, dstY,
     *                     DDBLTFAST_WAIT=0x10)
     *   IDirectDrawSurface::BltFast copies the back buffer over the front
     *   surface region to clear any previous text frame before redrawing.
     * LINUX: SDL_RenderCopy(renderer, g_backTex, &srcRect, &dstRect);
     *        // Where srcRect = widget->backRegion, dstRect = widget->frontRegion */

    /* WIN32: FUN_00426b00(self) — GetDC wrapper (IDirectDrawSurface::GetDC)
     *   Obtain HDC compatible with the DirectDraw surface for GDI text calls.
     * LINUX: not needed; SDL_ttf renders text to SDL_Surface without an HDC. */

    /* WIN32: SelectObject(hdc, DAT_004855fc)
     *   Sets the GDI font (HFONT stored globally at 0x004855fc).
     *   The font is loaded once at startup from a .fon or TrueType resource.
     * LINUX: TTF_Font *font = g_textWidgetFont;
     *        // Keep a global font pointer initialised from TTF_OpenFont(). */

    /* WIN32: DrawTextA(hdc, text, -1, &measRect, DT_CALCRECT)
     *   Measures the text bounding box without drawing.  Fills measRect with
     *   the minimum enclosing RECT for the text string at the current font.
     *   Param -1 means text is NUL-terminated.
     * LINUX: int textW, textH;
     *        TTF_SizeUTF8(font, text, &textW, &textH); */

    /* WIN32: FUN_00426b90(self) — DirectDraw Blt of measured rect area.
     *   Blits the GDI-rendered HDC region to the DirectDraw primary surface.
     * LINUX: SDL_Surface *textSurf = TTF_RenderUTF8_Blended(
     *            font, text, textColour);
     *        SDL_Texture *textTex = SDL_CreateTextureFromSurface(
     *            renderer, textSurf);
     *        SDL_RenderCopy(renderer, textTex, NULL, &dstRect);
     *        SDL_DestroyTexture(textTex);
     *        SDL_FreeSurface(textSurf); */

    /* Scroll-alignment offset calculation (this+0x1b0 field):
     *   0 = right-align  — dstRect.x = boundR - textW
     *   1 = left-align   — dstRect.x = boundL
     *   2 = bottom-align — dstRect.y = boundB - textH
     *   3 = top-align    — dstRect.y = boundT   (default)
     *
     * Applied to this+0x1a0 bounding rect before the blit.
     *
     * LINUX: SDL_Rect dstRect;
     *        switch (scrollAlign) {
     *        case 0:  dstRect.x = boundR - textW;      dstRect.y = boundT; break;
     *        case 1:  dstRect.x = boundL;               dstRect.y = boundT; break;
     *        case 2:  dstRect.x = boundL;               dstRect.y = boundB - textH; break;
     *        default: dstRect.x = boundL;               dstRect.y = boundT; break;
     *        }
     *        dstRect.w = textW; dstRect.h = textH; */

    (void)self;
}
