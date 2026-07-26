# Lego Loco — Main Menu / Startup Screen: Reverse-Engineering Report

**Target:** `lego-loco-unpacked/Exe/loco.exe` (Win32, x86, MSVC-compiled, 1997/98)
**Scope:** How the main menu (the fullscreen *name-entry / start* screen) is architected,
drawn, laid out, and how its buttons work.
**Method:** Ghidra + radare2 static analysis of the shipped binary. Every claim below is
cited to a function address and, where useful, to specific instructions. All named functions,
the `EditWnd` struct, and the data globals referenced here have been written back into the
Ghidra project (`loco.exe.rep`) — see *Appendix A: Symbol Map*.

> **Naming note.** The controller class has no RTTI in the binary. The window's title string
> is the literal `"EDIT WINDOW"` (`s_EditWindowTitle`, `0x004851d0`), and its central child is a
> Win32 `EDIT` control, so this report calls the class **`EditWnd`**. Its base class is the
> game's in-house window toolkit, called **`CGWnd`** here (the "C Game Window" — every game
> window, panel and dialog derives from it). These names are used consistently in the Ghidra DB
> and in `src/ui/ui.c`.

---

## 1. Executive summary

The "main menu" is not a Windows menu or a Windows dialog resource. It is a single
**fullscreen, borderless, owner-drawn window** (`EditWnd`) that the game paints itself using
**DirectDraw**, plus one real Win32 child `EDIT` control for typing the player's name.

The screen is built from **sprite art** loaded out of the game's resource archives. A large
**1280×1024 off-screen "composite" surface** is assembled once from five background art pieces,
then blitted to the visible primary surface every time the screen needs to refresh. On top of
that background the controller draws **button sprites**. Each button exists as a *pair* of
sprites — a normal image and a "pressed" image — and owns a rectangle (`RECT`) computed from
the sprite's own pixel dimensions.

Input is routed through the `CGWnd` toolkit: a single registered `WNDPROC`
(`CGWnd_StaticWndProc`) recovers the C++ object from the window and calls a **virtual message
dispatcher** (`CGWnd_dispatchMessage`) that translates each Win32 message into a call to a
specific **virtual method slot**. Mouse and keyboard handlers do **`PtInRect`** hit-testing
against the button rectangles, blit the pressed sprite for feedback, and then invoke the
button's action (validate & save the name, or drive the screen's **state machine**).

```
                        ┌─────────────────────────────────────────────┐
   Win32 message  ─────▶│ CGWnd_StaticWndProc  (0x4272f0)              │
   (mouse/key/...)      │  GetWindowLongA(hwnd, GWL_USERDATA) -> this  │
                        │  this->vtable[10](this, hwnd, msg, wp, lp)   │
                        └───────────────────────┬─────────────────────┘
                                                ▼
                        ┌─────────────────────────────────────────────┐
                        │ CGWnd_dispatchMessage (0x426140) vtable idx10│
                        │  switch(msg): map message -> virtual slot    │
                        │   WM_LBUTTONDOWN -> vtbl[0x38] (idx14)        │
                        │   WM_KEYDOWN     -> vtbl[0x54] (idx21)        │
                        │   WM_MOUSEMOVE   -> vtbl[0x50] (idx20)  ...   │
                        └───────────────────────┬─────────────────────┘
                                                ▼
            ┌───────────────────────────────────────────────────────────────┐
            │ EditWnd virtual handlers                                        │
            │  OnLButtonDown (0x422930)  PtInRect -> press sprite -> action   │
            │  OnKeyDown     (0x420bb0)  Enter/Esc -> animate -> action       │
            │  DrawNameEntry (0x421be0)  blit composite + button sprites      │
            │  SetState      (0x4208f0)  show/hide PanelA/PanelB, start game  │
            └───────────────────────────────────────────────────────────────┘
```

---

## 2. The `CGWnd` window toolkit (the foundation)

Every visible game object — the menu controller, the side panels, in-game dialogs — is a
`CGWnd`. Understanding this base class is the key to everything else, because it explains how
messages reach the menu's handlers.

### 2.1 Class creation and the registered window procedure

`CGWnd_createWindow` (`0x00425b70`) builds the underlying HWND:

```c
/* CGWnd_createWindow @ 0x00425b70 (Ghidra) */
local_128.lpfnWndProc   = (WNDPROC)&LAB_004272f0;     /* the shared static WndProc   */
local_128.lpszClassName = (LPCSTR)((int)this + 0x78); /* per-instance class name buf */
RegisterClassA(&local_128);
pHVar3 = CreateWindowExA(0, classname, title, 0x87000000 /* WS_POPUP|clip */,
                         x, y, w, h, parentHWND, menu, hInst,
                         this);                        /* lpCreateParams = the C++ obj */
...
(**(code **)(*(int *)this + 0x1c))();                 /* vtable[7]  -> recalc layout   */
ShowWindow(...); UpdateWindow(...);
```

Two facts matter:

1. **One `WNDPROC` for all windows** — `&LAB_004272f0` (`CGWnd_StaticWndProc`).
2. **The C++ `this` pointer is smuggled in** as `CreateWindowExA`'s last argument
   (`lpCreateParams`).

### 2.2 Recovering `this` and dispatching — the MFC-style thunk

`CGWnd_StaticWndProc` (`0x004272f0`) is tiny but pivotal. Disassembly:

```asm
; CGWnd_StaticWndProc @ 0x004272f0
004272f6  mov  ebx, [USER32.dll_GetWindowLongA]
00427300  push 0xffffffeb                 ; -21 = GWL_USERDATA
00427303  push edi                        ; hwnd
00427304  call ebx                        ; this = GetWindowLongA(hwnd, GWL_USERDATA)
0042730a  test eax, eax
0042730c  je   <store-this-from-CREATESTRUCT>
...
00427334  call dword [edx + 0x28]         ; this->vtable[0x28/4 = 10](this,hwnd,msg,wp,lp)
```

If the object pointer is already stored it calls **virtual slot 10** (`+0x28`); on the very
first message it instead pulls `this` out of the `CREATESTRUCT` and stores it with
`SetWindowLongA(hwnd, GWL_USERDATA, this)`. This is the classic "C++ object behind an HWND"
thunk. From here on, **every message becomes a call to `this->vtable[10]`**.

### 2.3 The message → virtual-slot demultiplexer

`CGWnd_dispatchMessage` (`0x00426140`, vtable slot 10) is a large `switch(msg)` that converts
each Win32 message into a call to a more specific virtual slot. Recovered mapping (from the
jump tables at `0x426166` and `0x4266e6` and the surrounding compares):

| Win32 message | value | vtable offset | slot (idx) | EditWnd override |
|---|---|---|---|---|
| `WM_KEYDOWN` | `0x100` | `+0x54` | 21 | `EditWnd_onKeyDown` (0x420bb0) |
| `WM_KEYUP` | `0x101` | `+0x58` | 22 | *(default stub)* |
| `WM_SYSCOMMAND`/cmds | `0x111`/… | `+0x88` | — | routed to app-cmd handler |
| `WM_LBUTTONDOWN` | `0x201` | `+0x38` | 14 | `EditWnd_onLButtonDown` (0x422930) |
| `WM_LBUTTONUP` | `0x202` | `+0x3c` | 15 | *(default stub)* |
| `WM_RBUTTONDOWN` | `0x204` | `+0x40` | 16 | `0x4323c0` |
| `WM_MOUSEMOVE`* | `0x200` | `+0x50` | 20 | `EditWnd_onMouseMove_hitTest` (0x422d80) |
| `WM_TIMER` | `0x113` | `+0x30` | 12 | *(default stub)* |
| `WM_CREATE`/`WM_DESTROY` … | low | `+0x78`,`+0x34` … | — | base handling |

\* The `0x200` path is special: the dispatcher itself manages mouse **capture** and cursor
visibility — on button-down it `SetCapture`s and hides the cursor; while captured it tracks the
pointer with `WindowFromPoint` and toggles `ReleaseCapture`/`ShowCursor` when the pointer leaves
the window (`0x426599`–`0x426660`). This is how the fullscreen menu keeps the mouse "inside"
the borderless window.

> The two mouse handlers used by the menu — `WM_LBUTTONDOWN → idx14` and the hover/move
> `idx20` — are therefore *not* guesses; they are the literal targets the dispatcher computes
> from the message id.

### 2.4 The owner-draw render primitive

The base draw method `CGWnd_onDraw` (`0x00425fd0`, vtable slot 3) is a thin wrapper that reads a
`LOCOBITMAP`'s width/height (`+0x32`/`+0x34`) and forwards to **vtable slot 4** (`0x426020`),
which performs the actual DirectDraw blit of a region to the primary surface. The menu's
imperative redraws (after a click, on refresh) funnel through this slot. A `WM_PAINT` handler
*does* exist (`0x0F → vtable+0x6c` in the dispatcher), but the menu artwork is **not** painted
through it — the game presents frames itself via these DirectDraw blits, so the menu does not
depend on the GDI paint cycle.

---

## 3. The `EditWnd` controller object

`EditWnd` is constructed by `EditWnd_ctor` (`0x004202f0`). It installs the vtable
`PTR_EditWnd_vtbl` (`0x004779f8`), publishes itself as the **singleton** `g_pEditWnd`
(`0x00485240`), and creates two GDI brushes used to paint the `EDIT` control's background:

```c
/* EditWnd_ctor @ 0x004202f0 (Ghidra) */
*(undefined ***)this = &PTR_FUN_004779f8;       /* vtable                       */
g_pEditWnd        = this;                        /* DAT_00485240 singleton       */
this->state       = 0;                           /* +0xe8                        */
this->pChildDialog= 0;                           /* +0x210                       */
this->spritesLoaded = 0;                         /* +0x18c                       */
this->hbrSolid = CreateSolidBrush(0x5252e7);     /* +0x204                       */
this->hbrHatch = CreateHatchBrush(5,0xa5c0a);    /* +0x208  HS_DIAGCROSS         */
```

The full object layout was recovered field-by-field and is encoded as the **`EditWnd` struct
(548 = 0x224 bytes)** in the Ghidra type library. The salient members:

| Offset | Field | Meaning |
|---|---|---|
| `+0x000` | `vtable` | → `PTR_EditWnd_vtbl` |
| `+0x008` | `hwnd` | the fullscreen window |
| `+0x0e8` | `state` | UI state machine value (§7) |
| `+0x0f4` | `buttonsVisible` | name-entry buttons currently drawn |
| `+0x0f8` | `hAppIcon` | `LoadIconA(0x65)` |
| `+0x0fc … +0x16b` | `rcBtn407/409/40b/40e`, `rcBtnGo`, `rcBtnBack`, `rcFixedB` | 7 button/region rects |
| `+0x16c/0x170` | `centerDX/centerDY` | design-space → screen centering offsets |
| `+0x17c` | `rcFixedA` | 8th region rect |
| `+0x18c` | `spritesLoaded` | sprites + composite built |
| `+0x190 … +0x1ef` | `spr*`, `sprGo/sprGoPressed`, `sprBack/sprBackPressed`, … | 12 `MenuSprite{res,surface}` pairs |
| `+0x1f0` | `pCompositeSurface` | the 1280×1024 background |
| `+0x20c` | `hwndEdit` | the child `EDIT` control |
| `+0x210` | `pChildDialog` | intro-video child dialog |
| `+0x214/0x218` | `savedEditWndProc/savedChildWndProc` | subclass back-pointers |
| `+0x21c/0x220` | `pPanelA/pPanelB` | the two sub-panels (§8) |

---

## 4. Construction & startup sequence

`EditWnd_onInitDialog` (`0x004204d0`) wires the whole screen together:

```c
/* EditWnd_onInitDialog @ 0x004204d0 (Ghidra) */
GetClientRect(GetDesktopWindow(), &desktopRect);          /* fullscreen extent       */
this->hAppIcon = LoadIconA(this->hInstance, 0x65);
MainMenu_loadSprites(this);                               /* 0x421500 (§5)           */
CGWnd_createWindow(this, ... fullscreen ...);             /* 0x425b70 -> recalc (§6) */

this->pPanelA = operator_new(0x1e4);  PanelA_ctor(pPanelA, hInst, 0x1f6);  /* tmpl 0x1f6 */
PanelA_createWindow(pPanelA, this->hwnd);
this->pPanelB = operator_new(0x260);  PanelB_ctor(pPanelB, hInst, 0x1f9);  /* tmpl 0x1f9 */
PanelB_createWindow(pPanelB, this->hwnd);

/* The name field: a real Win32 EDIT control */
this->hwndEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT" /*s_EditClassName*/, "",
                                 WS_CHILD|ES_AUTOHSCROLL|WS_VISIBLE, x,y,w,h,
                                 this->hwnd, (HMENU)0x411, hInst, NULL);
PostMessageA(hwndEdit, WM_SETFONT, hFont, 1);
PostMessageA(hwndEdit, EM_SETLIMITTEXT, 11, 0);           /* max 11 chars            */
this->savedEditWndProc = SetWindowLongA(hwndEdit, GWL_WNDPROC, 0x420b20); /* subclass */
SetFocus(hwndEdit);
```

So at init time the screen has: a fullscreen owner-drawn host window, two child panels, and
one subclassed `EDIT` control (limited to 11 characters) for the player name.

`EditWnd_activate` (`0x004206b0`, vtable slot 2) is what makes the screen *live*: it
re-loads sprites, hides the OS cursor (`while (ShowCursor(FALSE) >= 0);`), focuses the edit
field, pre-fills it with the stored player name (`g_pPlayerRecord+6`), selects all text
(`EM_SETSEL 0,-1`), and either enters the multiplayer lobby or drops to the name-entry state.

---

## 5. How the screen is drawn

### 5.1 Loading the button sprites

`MainMenu_loadSprites` (`0x00421500`) loads **12 sprites** through the resource manager
`ResourceManager_lookup` (`0x00446ea0`) using the global `g_pResourceMgr` (`0x004855e8`):

```c
/* MainMenu_loadSprites @ 0x00421500 (Ghidra, abridged) */
if (!this->spritesLoaded) {
    this->sprGo.pResource  = ResourceManager_lookup(g_pResourceMgr, 0x403);
    this->sprGo.pSurface   = (*sprGo.pResource->vtable[1])(0,0);   /* create surface */
    /* … resource ids 0x404,0x405,0x406,0x407,0x408,0x409,0x40a,0x40b,0x40c,0x40e,0x40f … */
    MainMenu_buildBackgroundSurface(this);                        /* 0x4216f0        */
    this->spritesLoaded = 1;
}
```

The twelve IDs `0x403,0x404,…,0x40c,0x40e,0x40f` (note `0x40d` is **skipped**) are **six
buttons × two states**. The even/odd pairing is confirmed by the hit-test handlers, which use
`+0` for normal and `+8` (the next ID) for the pressed image:

| Button | Normal id / field | Pressed id / field | Rect |
|---|---|---|---|
| **Go** | `0x403` `sprGo` (+0x1b0) | `0x404` `sprGoPressed` (+0x1b8) | `rcBtnGo` +0x13c |
| **Back** | `0x405` `sprBack` (+0x1c0) | `0x406` `sprBackPressed` (+0x1c8) | `rcBtnBack` +0x14c |
| (3) | `0x407` (+0x190) | `0x408` (+0x198) | `rcBtn407` +0xfc |
| (4) | `0x409` (+0x1a0) | `0x40a` (+0x1a8) | `rcBtn409` +0x10c |
| (5) | `0x40b` (+0x1d0) | `0x40c` (+0x1d8) | `rcBtn40b` +0x11c |
| (6) | `0x40e` (+0x1e0) | `0x40f` (+0x1e8) | `rcBtn40e` +0x12c |

Each `MenuSprite` is `{void *pResource; void *pSurface;}` — the resource object plus its
created DirectDraw surface handle.

### 5.2 Building the background composite

`MainMenu_buildBackgroundSurface` (`0x004216f0`) allocates a **1280×1024** off-screen surface
and composes five background art pieces onto it at fixed coordinates:

```c
/* MainMenu_buildBackgroundSurface @ 0x004216f0 (reconstructed) */
this->pCompositeSurface = new LOCOBITMAP();              /* +0x1f0 */
LOCOBITMAP_create(this->pCompositeSurface, 0x500, 0x400, 1, 0, 0);   /* 1280 x 1024 */

blit(0x413, 0,      0    );   /* full-screen base backdrop      */
blit(0x444, 0xf4,   0x1d6);   /* sub-panel art piece            */
blit(0x445, 0x204,  0xf9 );
blit(0x446, 0x11a,  0xf0 );
blit(0x443, 0x20b,  0x2a8);
/* each blit: lookup resource -> get its surface (vtable[1]) ->
   SetRect/CopyRect/OffsetRect to position -> FUN_0042b960 blit -> release (vtable[2]) */
```

The constant **`0x500 × 0x400` (1280×1024)** is the menu's **design resolution**. All art and
all button coordinates are authored in this space; §6 explains how it is mapped to the player's
actual screen.

### 5.3 Presenting the screen and the buttons

`EditWnd_drawNameEntryButtons` (`0x00421be0`, vtable slot 8) presents the name-entry screen.
It only runs in `state == 0` (initial) or `state == 7` (returned from a child screen):

1. Blit the composite background (`pCompositeSurface`, `+0x1f0`) to the **primary DirectDraw
   surface** `g_pPrimarySurface` (`0x004fd3c4`), offset by the centering rectangle (`+0x16c`).
2. Blit the two name-entry buttons in their **normal** state: `sprGo` at `rcBtnGo`, `sprBack`
   at `rcBtnBack`. Sprite size comes from the sprite header (`+0x14`/`+0x16`).
3. Set `buttonsVisible = 1` (`+0xf4`).

Disassembly confirms the destination is the global primary surface and the source is the
composite:

```asm
; EditWnd_drawNameEntryButtons @ 0x00421be0 (excerpt)
00421c?? mov  edx, [g_primarySurface]      ; 0x4fd3c4
...      <copy composite rect, offset by centering delta, blit via 0x42b050>
...      <blit sprGo (this+0x1b0) into rcBtnGo (this+0x13c)>
00421e8? mov  byte [esi+0xf4], 1           ; buttonsVisible = 1
```

So "drawing" the menu = **one background blit + N sprite blits straight onto the DirectDraw
primary surface**. The artwork is never drawn through GDI / `WM_PAINT`; the only GDI involvement
is colouring the `EDIT` control (§9).

---

## 6. How the engine places elements (layout)

Layout is `MainMenu_recalcButtonRects` (`0x00421200`, vtable slot 7), invoked by
`CGWnd_createWindow` right after the HWND exists, and again whenever the screen is rebuilt.
It implements **"author in 1280×1024, then centre on the real screen."**

```c
/* MainMenu_recalcButtonRects @ 0x00421200 (Ghidra, after struct typing) */
if (this->spritesLoaded) {
    /* centering deltas: half of (design size - actual screen size) */
    this->centerDX = (this->pCompositeSurface->width  - g_screenWidth ) >> 1;  /* +0x16c */
    this->centerDY = (this->pCompositeSurface->height - g_screenHeight) >> 1;  /* +0x170 */

    /* For each button: place at its DESIGN coordinate, size it from the sprite's own
       pixel dimensions, then shift by (-centerDX, -centerDY) into screen space.        */
    SetRect(&rcBtnGo, 0x387, 0x2a5, 0, 0);
    rcBtnGo.right  = rcBtnGo.left + sprGo.pResource->width;   /* sprite hdr +0x14 */
    rcBtnGo.bottom = rcBtnGo.top  + sprGo.pResource->height;  /* sprite hdr +0x16 */
    OffsetRect(&rcBtnGo, -this->centerDX, -this->centerDY);
    /* … identical pattern for the other five buttons … */

    /* two fixed (non-sprite) regions, e.g. the name field hot-zone */
    SetRect(&rcFixedA, 300, 0xac, 0x3d4, 0x354); OffsetRect(&rcFixedA, -centerDX, -centerDY);
    SetRect(&rcFixedB, 0x232,0x2cc,0x34d,0x2ed ); OffsetRect(&rcFixedB, -centerDX, -centerDY);
}
```

**Design-space coordinates** (the authored layout, before centering):

| Button | Design (left, top) | Size source |
|---|---|---|
| Go | `(0x387, 0x2a5)` = (903, 677) | `sprGo` dimensions |
| Back | `(0x18b, 0x2a5)` = (395, 677) | `sprBack` dimensions |
| (3) `rcBtn407` | `(0x212, 0x1ea)` = (530, 490) | `spr407` |
| (4) `rcBtn409` | `(0x2c9, 0x1ea)` = (713, 490) | `spr409` |
| (5) `rcBtn40b` | `(0x387, 0x1bd)` = (903, 445) | `spr40b` |
| (6) `rcBtn40e` | `(0x387, 0x231)` = (903, 561) | `spr40e` |
| `rcFixedA` | `(300, 0xac)`–`(0x3d4,0x354)` | fixed |
| `rcFixedB` | `(0x232,0x2cc)`–`(0x34d,0x2ed)` | fixed |

The crucial design idea: **a button's hit-rectangle is derived from the same sprite that draws
it** (`right = left + sprite.width`, `bottom = top + sprite.height`, read from the sprite
header at `+0x14`/`+0x16`). Art and hit-testing can never drift apart — change the art and the
clickable region follows automatically. Because every rect is then offset by the *same*
`(centerDX, centerDY)`, the entire layout slides as a rigid body to centre the 1280×1024 canvas
on whatever resolution (`g_screenWidth` `0x4851d8` / `g_screenHeight` `0x485214`) the game is
running at.

---

## 7. How buttons work

A "button" is the triple **(normal sprite, pressed sprite, hit-rect)**. There is no button
*object* — the controller owns flat arrays of sprites and rects and drives them directly.

### 7.1 Mouse press — `EditWnd_onLButtonDown` (`0x00422930`, vtable idx 14)

Reached from `WM_LBUTTONDOWN` via the dispatcher (`vtable+0x38`). Behaviour:

```asm
; EditWnd_onLButtonDown @ 0x00422930 (excerpt)
0042293f mov  eax,[esi+0xe8]            ; state
00422950 jne  .have_state
00422952 push 7 ; EditWnd_setState(this, 7)            ; (idle click -> close child)
...
00422945 and  edi, 0xffff              ; x = lParam & 0xffff
0042294b shr  ebx, 0x10                ; y = lParam >> 16
00422973 lea  ebp,[esi+0x13c]          ; &rcBtnGo
0042297c call [PtInRect]               ; PtInRect(&rcBtnGo, {x,y})
00422984 je   .test_back
0042298a ...                            ; HIT: blit pressed sprite sprGoPressed (+0x1b8)
...                                     ;      onto composite then primary  (feedback)
00422aa2 call [edx+0xc]               ; redraw via vtable[3] (CGWnd_onDraw)
00422aaa call [Sleep]  ; Sleep(0x96)   ; 150 ms so the press is visible
00422ab2 call EditWnd_submitPlayerName ; 0x422660  -> the Go action
...
00422ac3 lea  ebp,[esi+0x14c]          ; &rcBtnBack  (sprite pair 0x405/0x406)
00422acc call [PtInRect]               ; ... -> Back action
```

So the press cycle is: **unpack `(x,y)` from `lParam` → `PtInRect` against each button rect →
on hit, blit the *pressed* sprite for visual feedback → `Sleep(150)` so the depression is
visible → run the button's action.** The full six-button action map recovered from
`0x00422930` is:

| Rect / sprite pair | Action (verbatim from `0x422930`) |
|---|---|
| `rcBtnGo` +0x13c (0x403/0x404) | screensaver-pw check; redraw; `Sleep`; **`EditWnd_submitPlayerName`** |
| `rcBtnBack` +0x14c (0x405/0x406) | `PlaySoundA`; screensaver-pw probe (res `0x5015`); `vtable[0x10]`; **`FUN_00408130(0xa)`** |
| `rcBtn407` +0xfc (0x407/0x408) | enable multiplayer: `g_pGameConfig+7=1`, `FUN_0043d2b0(npc,3)` |
| `rcBtn409` +0x10c (0x409/0x40a) | disable multiplayer: `g_pGameConfig+7=0`, `FUN_0043d2b0(npc,0)` |
| `rcBtn40b` +0x11c (0x40b/0x40c) | set sub-option `g_pGameConfig+8=1` |
| `rcBtn40e` +0x12c (0x40e/0x40f) | clear sub-option `g_pGameConfig+8=0` |

The four "mode" buttons are radio/checkbox-style toggles (single vs multiplayer and a
sub-option) that flip a config flag and repaint via `FUN_00422010` + `vtable[3]`; `Go` and the
`+0x14c` button are the two that actually leave the screen.

### 7.2 Hover / move — `EditWnd_onMouseMove_hitTest` (`0x00422d80`, vtable idx 20)

While the pointer is active (`+0x14`), this handler `PtInRect`-tests **all eight** rects in
order — `rcBtnGo, rcBtnBack, rcBtn407, rcBtn409, rcBtn40b, rcBtn40e, rcFixedA, rcFixedB` — and
on the first rect that contains the cursor jumps to a common tail (`0x422e78`) that redraws via
`vtable[3]`. This is the engine's hover/refresh feedback path:

```asm
; EditWnd_onMouseMove_hitTest @ 0x00422d80 (excerpt)
00422dc2 lea  ecx,[esi+0x13c]  ; rcBtnGo
00422dca call ebp             ; PtInRect; hit -> 0x422e78 (redraw)
00422dd5 lea  edx,[esi+0x14c]  ; rcBtnBack
00422de8 lea  eax,[esi+0xfc]   ; rcBtn407 ...
```

### 7.3 Keyboard — `EditWnd_onKeyDown` (`0x00420bb0`, vtable idx 21) and the EDIT subclass

The `EDIT` control is subclassed at init with the thunk at `0x00420b20`. Its disassembly shows
the accelerator routing:

```asm
; EDIT subclass thunk @ 0x00420b20  (eax = msg = [esp+8] & 0xffff; ecx = wParam = [esp+0xc])
00420b20 mov eax, [esp+8]
00420b24 and eax, 0xffff
00420b29 cmp eax, 0x20      ; WM_SETCURSOR -> LoadCursorA(IDC_ARROW)/SetCursor (0x420b85)
00420b32 cmp eax, 0x100     ; WM_KEYDOWN
00420b39 cmp ecx, 0x0d      ; wParam == VK_RETURN -> PostMessage(... app cmd)
00420b3e cmp ecx, 0x1b      ; wParam == VK_ESCAPE -> PostMessage(... app cmd)
00420b5c call [CallWindowProcA]   ; otherwise: original EDIT proc (normal typing)
```

(The `cmp eax, 0x20` compares the *message id*, not a virtual key — `0x20` is
`WM_SETCURSOR`; the thunk simply forces the arrow cursor over the borderless window.)

The handler body (`0x00420bb0`) animates the default button exactly like a mouse press: on
**Enter** it blits the pressed Go sprite, `Sleep`s, restores it, then calls
`EditWnd_submitPlayerName`; on **Escape** it activates the `+0x14c` button's path
(`FUN_00408130(0xa)`). So keyboard and mouse converge on the *same* button actions — pressing
Enter is identical to clicking **Go**.

### 7.4 The button action — `EditWnd_submitPlayerName` (`0x00422660`)

The **Go** action validates and persists the typed name and advances the screen:

```c
/* EditWnd_submitPlayerName @ 0x00422660 (reconstructed) */
this->vtable[4]();                                        /* refresh                       */
GetWindowTextA(this->hwndEdit, buf, 13);                  /* read the EDIT text (<=11)     */
if (string_uses_only(buf, "abcdefghij…XYZ")) {            /* charset check (FUN_004676d0)  */
    strcpy(g_pPlayerRecord + 6, buf);                     /* store into player record      */
    WritePrivateProfileStringA("USER", "Name", buf, ee.ini);/* persist to config           */
}
/* then branch on config flags: single-player -> EditWnd_setState(2/4/5);
   multiplayer  -> kick off the game/lobby (FUN_0043d2b0, EditWnd_startGameWorld). */
```

This is the concrete answer to *"what does a button do"*: **Go** = validate name → save to
`ee.ini` `[USER] Name=` → transition state (into the panel screens or, in multiplayer, the
lobby/game). The other on-screen buttons drive `EditWnd_setState` transitions, and the
`PanelA`/`PanelB` options (town picker etc.) are handled inside those panels (§8).

---

## 8. The screen state machine and the sub-panels

`EditWnd_setState` (`0x004208f0`, value stored at `+0xe8`) is the menu's brain. It shows/hides
the two child panels (via their own vtables: slot `+4` = show, slot `+8` = hide) and ultimately
launches the game:

| State | Name | Effect (from `0x4208f0`) |
|---|---|---|
| 1 | `HIDDEN` | `PlaySoundA(0,0,0)` stop music; hide `EDIT` |
| 2 | `DEACTIVATE` | hide `EDIT`; `PanelA.hide`; `PanelB.hide` (if prev was 4/5) |
| 3 | `SHOW_PANELS` | `PanelA.show`; choose 4 (single) or 5 (multi) from `g_pGameConfig+0x18`; `PanelB.show` |
| 4 / 5 | `PANEL_B` single/multi | hide `EDIT`; `PanelB.show` |
| 6 | `START_GAME` | hide both panels; `EditWnd_startGameWorld`; enter gameplay (`FUN_004616c0`, `FUN_00408130(1)`) |
| 7 | `CLOSE_CHILD` | restore the subclassed proc; destroy the intro-video child dialog; play `svideo\music.wav`; hide panels |

**`PanelA`** (`+0x21c`, allocated 0x1e4 bytes, dialog template **0x1f6**, ctor `0x00440f20`) and
**`PanelB`** (`+0x220`, 0x260 bytes, template **0x1f9**, ctor `0x00408aa0`) are themselves
`CGWnd`-derived panels created in `EditWnd_onInitDialog`. They are the *secondary* menu screens
the player reaches after entering a name (the action strip and the town/city selection panel).
Their internal widget handling mirrors the controller's — `PtInRect` over sprite-derived rects —
but a full decomposition of each panel is **out of scope for this report**, which targets the
top-level menu controller; they are documented at the interface level (construction, templates,
show/hide vtable contract) so the state machine above is complete.

`EditWnd_startGameWorld` (`0x00422820`) is the hand-off out of the menu: it sets the world tick
rate (`g_pGameConfig+0xc = 30`), allocates and constructs the town/world manager objects, spawns
the game-world worker thread (`FUN_00461790` → `CreateThread`), and calls `vtable[5]`.

---

## 9. Supporting details

- **The `EDIT` control's colours** are owner-supplied. `EditWnd_onSysCommandAndAppMsg`
  (`0x00420ec0`, vtable idx 11) answers `WM_CTLCOLOREDIT` (`0x133`) by calling `SetTextColor` /
  `SetBkMode` and returning the solid brush `hbrSolid` (`+0x204`, `CreateSolidBrush(0x5252e7)`).
- **Screensaver integration.** The same handler treats `WM_SYSCOMMAND` with `SC_SCREENSAVE`
  (`wParam & 0xfff0 == 0xf140`) as a request to tear the menu down (`setState(7)`); the activate
  path also probes for a password DLL (resource `0x5015`). Lego Loco shipped a screensaver mode.
- **Intro videos.** App-command messages (`0x3b9`, `0x51`) start AVI playback
  (`svideo\IgSpin.avi`, `locointr.avi`, `legoSpin.avi`) into a child dialog (`pChildDialog`,
  `+0x210`) via `EditWnd_playLegoSpinIntro` (`0x00421eb0`) and the handler at `0x00420ec0`.
- **`WM_PAINT` is not the art path.** The dispatcher *does* route `WM_PAINT` (`0x0F`) to
  `vtable+0x6c`, but the menu artwork is presented imperatively through the DirectDraw blits in
  §5, independent of the GDI paint cycle.

---

## Appendix A: Symbol map (written into the Ghidra project)

### Functions

| Address | Name | Role |
|---|---|---|
| `0x004202f0` | `EditWnd_ctor` | construct controller, install vtable, publish singleton |
| `0x004203a0` | `EditWnd_scalarDeletingDtor` | vtable[0] destructor |
| `0x00420860` | `EditWnd_destroy` | vtable[1] teardown |
| `0x004204d0` | `EditWnd_onInitDialog` | build window, panels, EDIT control |
| `0x004206b0` | `EditWnd_activate` | vtable[2] make screen live |
| `0x004208f0` | `EditWnd_setState` | UI state machine |
| `0x00420bb0` | `EditWnd_onKeyDown` | vtable[21] keyboard / EDIT subclass body |
| `0x00420b20` | *(EDIT subclass thunk)* | Enter/Esc accelerators + WM_SETCURSOR |
| `0x00420ec0` | `EditWnd_onSysCommandAndAppMsg` | vtable[11] CTLCOLOR / SYSCOMMAND / app cmds |
| `0x00421200` | `MainMenu_recalcButtonRects` | vtable[7] layout (design-space + centering) |
| `0x00421500` | `MainMenu_loadSprites` | load 12 button sprites |
| `0x004216f0` | `MainMenu_buildBackgroundSurface` | assemble 1280×1024 composite |
| `0x00421ae0` | `MainMenu_freeSprites` | release sprites + composite |
| `0x00421be0` | `EditWnd_drawNameEntryButtons` | vtable[8] present background + buttons |
| `0x00421eb0` | `EditWnd_playLegoSpinIntro` | vtable[9] intro AVI |
| `0x00422660` | `EditWnd_submitPlayerName` | Go/Enter action: validate + persist name |
| `0x00422820` | `EditWnd_startGameWorld` | hand-off into gameplay |
| `0x00422930` | `EditWnd_onLButtonDown` | vtable[14] mouse press / hit-test / action |
| `0x00422d80` | `EditWnd_onMouseMove_hitTest` | vtable[20] hover hit-test / redraw |
| `0x00425b70` | `CGWnd_createWindow` | RegisterClass + CreateWindowEx + recalc |
| `0x00425fd0` | `CGWnd_onDraw` | vtable[3] base draw primitive |
| `0x00426140` | `CGWnd_dispatchMessage` | vtable[10] message → virtual-slot demux |
| `0x004272f0` | `CGWnd_StaticWndProc` | shared WNDPROC, recovers `this`, dispatches |
| `0x00446ea0` | `ResourceManager_lookup` | resource/sprite factory |

### Data globals

| Address | Name | Meaning |
|---|---|---|
| `0x00485240` | `g_pEditWnd` | the menu controller singleton (`EditWnd *`) |
| `0x004855e8` | `g_pResourceMgr` | global resource manager |
| `0x004fd3c4` | `g_pPrimarySurface` | DirectDraw primary (visible) surface |
| `0x004851d8` | `g_screenWidth` | actual screen width |
| `0x00485214` | `g_screenHeight` | actual screen height |
| `0x004779f8` | `PTR_EditWnd_vtbl` | `EditWnd` vtable |
| `0x004851d0` | `s_EditWindowTitle` | `"EDIT WINDOW"` |
| `0x0047e464` | `s_EditClassName` | `"EDIT"` (child control class) |
| `0x004fd3a8` | `g_pGameConfig` | config (multiplayer flags, tick rate) |
| `0x004aa4a8` | `g_pPlayerRecord` | player record (name at +6) |

### Types

`EditWnd` (548 bytes), `MenuSprite` ({resource, surface}), `LOCO_RECT` — all defined in the
Ghidra type library; `g_pEditWnd` is typed `EditWnd *`.

---
*Generated from static analysis of `loco.exe`. Addresses are file/VA offsets in the shipped
PE image. The companion refinements live in `src/ui/ui.c` (decompiled C with inline citations).*
