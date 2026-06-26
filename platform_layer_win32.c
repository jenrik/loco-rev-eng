/*
 * Lego Loco (1998) — Win32 Platform Layer: annotated for Linux port
 * Every Win32 API call tagged WIN32:/LINUX: with SDL2 equivalent.
 */

/* =========================================================
 * FUN_00406480 (0x00406480) — Pre-window setup / INI read
 * Queries desktop geometry, reads window rect + FPS-balancing
 * thresholds from lego.ini.
 * ========================================================= */
void FUN_00406480(int param_1) {
    /* WIN32: GetDesktopWindow() — returns root desktop HWND.
     * LINUX: not needed; use SDL_GetDisplayBounds(0,&rect). */
    HWND desktop = GetDesktopWindow();

    /* WIN32: GetSystemMetrics(SM_CXSCREEN=0) / (SM_CYSCREEN=1)
     * Returns primary monitor pixel dimensions.
     * LINUX: SDL_GetCurrentDisplayMode(0,&mode) → mode.w / mode.h */
    DAT_004851d8 = GetSystemMetrics(0);
    DAT_00485214 = GetSystemMetrics(1);

    /* WIN32: SetRect(&r,l,t,w,h) — fills RECT; pure helper.
     * LINUX: plain struct assignment. */
    SetRect(&DAT_004851e0, 0, 0, DAT_004851d8, DAT_00485214);
}

/* =========================================================
 * FUN_004068d0 (0x004068d0) — Registry / INI path discovery
 * Reads/creates HKLM\SOFTWARE\Intelligent Games\LEGO LOCO to
 * locate the install path, then opens lego.ini.
 * ========================================================= */
void FUN_004068d0(void) {
    /* WIN32: RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0,
     *                      KEY_READ, &hKey)
     * LINUX: fopen(SDL_GetPrefPath("IntelligentGames","LegoLoco"), "r") */
    RegOpenKeyExA((HKEY)0x80000002,
        "SOFTWARE\\Intelligent Games\\LEGO LOCO", 0, 0x20019, &hKey);

    /* WIN32: RegQueryValueExA(hKey, NULL, …, buf, &len)
     * Reads the default (install path) string value.
     * LINUX: fread() from config file. */
    RegQueryValueExA(hKey, NULL, NULL, &type, buf, &len);

    /* WIN32: RegCloseKey(hKey)  LINUX: fclose(f) */
    RegCloseKey(hKey);

    /* WIN32: RegCreateKeyExA(HKLM, subkey, …, KEY_ALL_ACCESS, …, &hKey, …)
     * Creates key on first run.
     * LINUX: mkdir(prefPath) + fopen(…,"w") */
    RegCreateKeyExA((HKEY)0x80000002,
        "SOFTWARE\\Intelligent Games\\LEGO LOCO",
        0, NULL, 0, 0xf003f, NULL, &hKey, NULL);

    /* WIN32: RegSetValueExA(hKey, NULL, 0, REG_SZ, data, len)
     * Writes install path.  LINUX: fputs(path, f) */
    RegSetValueExA(hKey, NULL, 0, 1, buf, len);
    RegCloseKey(hKey);
}

/* =========================================================
 * FUN_00406680 (0x00406680) — Display capability check
 * Verifies colour depth / multimon before window creation.
 * ========================================================= */
uint FUN_00406680(int param_1) {
    /* WIN32: GetDC(hwnd) — obtains a GDI device context.
     * LINUX: SDL_GetCurrentDisplayMode(0,&mode) gives the same info. */
    HDC hdc = GetDC(desktopHwnd);

    /* WIN32: GetDeviceCaps(hdc, RASTERCAPS=0x18)
     * Checks for palette / RC_PALETTE flag (8-bit mode).
     * LINUX: SDL_ISPIXELFORMAT_INDEXED(mode.format) */
    UINT caps = GetDeviceCaps(hdc, 0x18);

    /* WIN32: GetDeviceCaps(hdc, BITSPIXEL=0x0c)
     * Returns colour depth of the primary display.
     * LINUX: SDL_BITSPERPIXEL(mode.format) */
    DAT_0048521c = GetDeviceCaps(hdc, 0x0c);

    /* WIN32: ReleaseDC(hwnd, hdc)  LINUX: nothing (SDL is non-owning). */
    ReleaseDC(desktopHwnd, hdc);

    /* WIN32: GetSystemMetrics(SM_SAMEDISPLAYFORMAT=0x13)
     * Non-zero when all monitors share the same pixel format.
     * LINUX: iterate SDL_GetNumDisplayModes() and compare formats. */
    int same = GetSystemMetrics(0x13);

    /* WIN32: MessageBoxA(NULL, text, "LEGO LOCO", 0)
     * Error dialog when depth < 16-bit or palette mode detected.
     * LINUX: SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, …) */
    MessageBoxA(NULL, errorText, "LEGO LOCO", 0);
}

/* =========================================================
 * FUN_00406ed0 (0x00406ed0) — Window class + window creation
 * TRUE main-window creation function (called from FUN_00406f90
 * which is invoked by FUN_00406ba0).
 * ========================================================= */
undefined4 FUN_00406ed0(int param_1) {
    /* WIN32: LoadIconA(hInstance, MAKEINTRESOURCE(0x65))
     * Loads embedded icon resource.
     * LINUX: SDL_LoadBMP("icon.bmp") + SDL_SetWindowIcon(win, surf) */
    wc.hIcon = LoadIconA(hInstance, (LPCSTR)0x65);

    /* WIN32: RegisterClassA(&wc)
     * Registers window class "LEGO LOCO", style CS_OWNDC|CS_DBLCLKS.
     * LINUX: not needed; SDL_CreateWindow handles internally. */
    RegisterClassA(&wc);

    /* WIN32: CreateWindowExA(
     *   WS_EX_TOPMOST(8) when local, 0 when remote-play,
     *   "LEGO LOCO", "LEGO LOCO", WS_POPUP|WS_VISIBLE=0x82000000,
     *   0, 0, screenW, screenH, NULL, NULL, hInst, NULL)
     * Full-screen borderless popup covering the entire desktop.
     * LINUX: SDL_CreateWindow("LEGO LOCO",
     *            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
     *            screenW, screenH,
     *            SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN) */
    HWND hWnd = CreateWindowExA(
        isRemotePlay ? 0 : WS_EX_TOPMOST,
        "LEGO LOCO", "LEGO LOCO",
        WS_POPUP | WS_VISIBLE,
        0, 0, screenW, screenH,
        NULL, NULL, hInstance, NULL);

    /* WIN32: GetClientRect(hWnd, &rect)
     * Returns drawable client area (no borders on WS_POPUP).
     * LINUX: SDL_GetWindowSize(win, &w, &h) */
    GetClientRect(hWnd, &clientRect);
}

/* =========================================================
 * FUN_00406ba0 (0x00406ba0) — Subsystem init + timer start
 * Orchestrates engine module init, then starts the 35 fps
 * multimedia timer.
 * ========================================================= */
int FUN_00406ba0(int param_1) {
    /* WIN32: CreateEventA(NULL, TRUE /*manual*/, FALSE, "GameLoop")
     * Manual-reset event; GameLoopCallback calls SetEvent(),
     * main thread calls WaitForSingleObject() each frame.
     * LINUX: sem_init(&gameSem, 0, 0)  [post in timer cb, wait in loop]
     *        or: eventfd(0, EFD_SEMAPHORE) + read/write. */
    DAT_004a990c = CreateEventA(NULL, TRUE, FALSE, "GameLoop");

    /* WIN32: timeBeginPeriod(14)
     * Raises Windows multimedia-timer resolution to 14 ms globally.
     * LINUX: not required; Linux timers already resolve to ~1 ms. */
    timeBeginPeriod(0x0e);

    /* WIN32: timeSetEvent(uDelay=28, uRes=14, cb, 0, TIME_PERIODIC=1)
     * Fires GameLoopCallback every 28 ms → 1000/28 ≈ 35.7 fps.
     * Returned timer ID saved to DAT_00485438 for later kill.
     * LINUX: SDL_AddTimer(28, TimerCB, NULL)
     *        In cb: SDL_Event ev; ev.type = GAME_TICK_EVENT;
     *               SDL_PushEvent(&ev); return 28; */
    DAT_00485438 = timeSetEvent(0x1c, 0x0e, GameLoopCB, 0, TIME_PERIODIC);
}

/* =========================================================
 * FUN_004085e0 (0x004085e0) — Mini message pump
 * Drains the Win32 queue during heavy loading operations.
 * ========================================================= */
void FUN_004085e0(char blockMouse) {
    /* WIN32: PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE=0)
     * Non-blocking queue peek.  LINUX: SDL_PollEvent(&ev) (removes). */
    while (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        /* Filters: swallow WM_MOUSEMOVE(0x200), WM_SETCURSOR(0x20),
         * WM_LBUTTONDOWN/UP(0x201/202), WM_RBUTTONDOWN/UP(0x204/205)
         * during load.  blockMouse param controls mouse-move pass-through. */

        /* WIN32: PeekMessageA(…, PM_REMOVE=1) — dequeue the message.
         * LINUX: SDL_PollEvent() already dequeues. */

        /* WIN32: TranslateMessage(&msg) — VK → WM_CHAR.
         * LINUX: implicit in SDL's keyboard event layer. */

        /* WIN32: DispatchMessageA(&msg) — routes to WndProc.
         * LINUX: your SDL event switch statement. */

        /* WIN32: SetCursor(NULL) — hides cursor during loading.
         * LINUX: SDL_ShowCursor(SDL_DISABLE) */
    }
}

/* =========================================================
 * FUN_00408350 (0x00408350) — Deferred startup / repaint
 * Sets a one-shot WM_TIMER to defer DirectDraw init, disables
 * input, forces a repaint.
 * ========================================================= */
void FUN_00408350(void) {
    /* WIN32: SetTimer(hWnd, nID=0x47, uElapse=150, NULL)
     * Posts WM_TIMER after 150 ms; defers heavy DD surface alloc.
     * LINUX: SDL_AddTimer(150, OneShot, NULL) or just call init
     *        directly after the first SDL_RenderPresent(). */
    DAT_004a97a4 = SetTimer(hWnd, 0x47, 150, NULL);

    /* WIN32: EnableWindow(hWnd, FALSE) — disables all input.
     * LINUX: set a boolean "loading" flag; ignore SDL events. */
    EnableWindow(hWnd, FALSE);

    /* WIN32: InvalidateRect(hWnd, NULL, FALSE)
     * Marks entire client area dirty, queues WM_PAINT.
     * LINUX: SDL_RenderPresent() to push the loading frame. */
    InvalidateRect(hWnd, NULL, FALSE);

    /* WIN32: UpdateWindow(hWnd)
     * Delivers WM_PAINT immediately (bypasses the queue).
     * LINUX: SDL_RenderPresent() — same result. */
    UpdateWindow(hWnd);
}

/* =========================================================
 * FUN_004086f0 (0x004086f0) — Game mode dispatcher (cases 1-9)
 * ========================================================= */
void FUN_004086f0(int mode) {
    /* WIN32: KillTimer(hWnd, timerID)
     * Cancels the 150 ms startup timer from FUN_00408350.
     * LINUX: SDL_RemoveTimer(timerID) */
    KillTimer(hWnd, DAT_004a97a4);

    /* WIN32: PostMessageA(hWnd, WM_APP+6=0x406, wparam, 0)
     * Async game-state transition via private message.
     * LINUX: SDL_PushEvent() with SDL_RegisterEvents() custom type. */
    PostMessageA(hWnd, 0x406, DAT_004a99b4, 0);

    /* WIN32: wsprintfA(buf, "Layouts\\%s", name)
     * LINUX: snprintf(buf, sizeof buf, "Layouts/%s", name) */
    wsprintfA(buf, "Layouts\\%s", name);

    /* WIN32: PlaySoundA(NULL, NULL, 0) — stops any active .wav.
     * LINUX: Mix_HaltChannel(-1)  (SDL_mixer) */
    PlaySoundA(NULL, NULL, 0);
}

/* =========================================================
 * FUN_00407ae0 (0x00407ae0) — Scroll position sync
 * ========================================================= */
void FUN_00407ae0(void) {
    /* WIN32: SetScrollRange(hWnd, SB_HORZ=0, 0, nMax, FALSE)
     * WIN32: SetScrollPos(hWnd, SB_HORZ, pos, TRUE)
     * Drives the horizontal scrollbar widget in windowed mode.
     * LINUX: SDL has no native scrollbar; render a custom sprite
     *        and track scroll offset as a plain integer. */
    SetScrollRange(hWnd, SB_HORZ, 0, nMaxPos, FALSE);
    SetScrollPos(hWnd, SB_HORZ, scrollPos, TRUE);
}

/* =========================================================
 * FUN_004077a0 (0x004077a0) — Shutdown / teardown
 * Persists window rect to lego.ini, then destroys every
 * subsystem in reverse-init order.
 * ========================================================= */
void FUN_004077a0(void) {
    /* WIN32: Sleep(100) in a busy-wait loop
     * Waits for async worker thread to drain.
     * LINUX: sem_wait(&workerDone) or pthread_join(). */
    while (workerRunning) Sleep(100);

    /* WIN32: CloseHandle(hEvent) — frees the "GameLoop" event.
     * LINUX: close(timerfd) / sem_destroy(&gameSem) */
    CloseHandle(DAT_004a990c);

    /* WIN32: timeKillEvent(timerID)
     * Cancels the 35 fps TIME_PERIODIC timer.
     * LINUX: SDL_RemoveTimer(tid) or timer_delete(timerId) */
    timeKillEvent(DAT_00485438);

    /* WIN32: timeEndPeriod(14)
     * Restores global system timer resolution.
     * LINUX: no-op — Linux resolution is not altered per-process. */
    timeEndPeriod(0x0e);
}

/* =========================================================
 * FUN_00408130 (0x00408130) — Display-mode switch
 * Dispatches game state changes 0–10; mode 0xa quits.
 * ========================================================= */
void FUN_00408130(int mode) {
    /* Modes:
     *  1 → FUN_00408350 (initial load)
     *  2 → windowed DirectDraw flip
     *  3 → FUN_004086f0 transition handler
     *  4 → reset to attract/idle
     *  5/6 → fullscreen 2-D tile gameplay
     *  7 → 3-D flying-vehicle view
     *  9 → mini-game screen
     * 0xa → quit */

    /* WIN32: PostMessageA(hWnd, WM_QUIT=0x10, 0, 0)  [mode 0xa]
     * Terminates the message loop.
     * LINUX: SDL_Event ev = {.type = SDL_QUIT};
     *        SDL_PushEvent(&ev); */
    PostMessageA(hWnd, WM_QUIT, 0, 0);
}

/*
 * WIN32 → SDL2/POSIX QUICK-REFERENCE  (all APIs found in this file)
 * -----------------------------------------------------------------
 * DISPLAY
 *   GetDesktopWindow()            SDL_GetDisplayBounds(0,&r)
 *   GetSystemMetrics(SM_CX/CYSCREEN) SDL_GetCurrentDisplayMode().w/h
 *   GetDeviceCaps(BITSPIXEL)      SDL_BITSPERPIXEL(mode.format)
 *   GetDeviceCaps(RASTERCAPS)     SDL_ISPIXELFORMAT_INDEXED()
 *   GetSystemMetrics(SM_SAMEDISPLAYFORMAT) compare SDL display modes
 *   GetDC / ReleaseDC             SDL_GetWindowSurface (read-only)
 *
 * WINDOW
 *   RegisterClassA                (implicit in SDL_CreateWindow)
 *   CreateWindowExA (WS_POPUP)    SDL_CreateWindow(FULLSCREEN_DESKTOP)
 *   GetClientRect                 SDL_GetWindowSize
 *   EnableWindow(FALSE)           state flag to ignore SDL events
 *   InvalidateRect / UpdateWindow SDL_RenderPresent
 *   SetCursor(NULL)               SDL_ShowCursor(SDL_DISABLE)
 *   SetScrollRange / SetScrollPos custom sprite widget
 *   LoadIconA                     SDL_LoadBMP + SDL_SetWindowIcon
 *   MessageBoxA                   SDL_ShowSimpleMessageBox
 *
 * MESSAGE LOOP
 *   PeekMessageA(PM_NOREMOVE)     SDL_PeepEvents(SDL_PEEKEVENT)
 *   PeekMessageA(PM_REMOVE)       SDL_PollEvent
 *   TranslateMessage              (implicit in SDL)
 *   DispatchMessageA              SDL event switch
 *   PostMessageA(WM_QUIT)         SDL_PushEvent(SDL_QUIT)
 *   PostMessageA(WM_APP+6)        SDL_PushEvent(custom registered type)
 *   SetTimer / KillTimer          SDL_AddTimer / SDL_RemoveTimer
 *
 * TIMER / AUDIO
 *   timeBeginPeriod(14)           (no-op on Linux)
 *   timeSetEvent(28,14,cb,0,1)    SDL_AddTimer(28,cb,NULL)
 *   timeKillEvent(id)             SDL_RemoveTimer(id)
 *   timeEndPeriod(14)             (no-op on Linux)
 *   PlaySoundA(NULL,NULL,0)       Mix_HaltChannel(-1)
 *
 * SYNC
 *   CreateEventA(manual-reset)    sem_init / eventfd
 *   CloseHandle(event)            sem_destroy / close(fd)
 *   Sleep(ms)                     usleep(ms*1000)
 *
 * REGISTRY
 *   RegOpenKeyExA(HKLM,…)        fopen(SDL_GetPrefPath(…),"r")
 *   RegQueryValueExA              fread / ini_parse
 *   RegCreateKeyExA               mkdir + fopen(…,"w")
 *   RegSetValueExA                fputs / fprintf
 *   RegCloseKey                   fclose
 *
 * STRING
 *   wsprintfA(buf,fmt,…)          snprintf(buf,sizeof buf,fmt,…)
 *   SetRect(…)                    plain struct assignment
 */
