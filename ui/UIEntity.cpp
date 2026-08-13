/**
 * UIEntity.cpp — UIEntity implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * UIEntity is a GameObject-derived class that places itself at world
 * coordinates based on a direction code. The constructor uses the
 * resource's frame data to compute placement and creates a tooltip
 * if the resource has children.
 *
 * Placement mode (direction) codes:
 *   'C' (0x43) = Center  — world center minus resource half-dimensions
 *   'D' (0x44) = Down    — scatter downward from reference with variant=1
 *   'P' (0x50) = Push    — push right from world edge, random Y within bounds
 *   'R' (0x52) = Random  — fully random (X,Y) within world bounds
 *   'S' (0x53) = Spawn   — spawn along left edge, random Y within world height
 *   'U' (0x55) = Up      — scatter upward from reference with variant=1
 *   'W' (0x57) = West    — place to the left (west) of reference position
 */

// Status: TRANSCRIBED

#include "UIEntity.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include <stdint.h>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* CRT helpers */
int   __cdecl CRT_rand(void);                            /* @ 0x466150 */
int   __cdecl CRT_toupper(int c);                        /* @ 0x467710 — toupper wrapper */

/* GameObject base methods */
void  __thiscall GameObject_SetWorldPos(void* self,
                                         int x, int y);   /* @ 0x405C00 */

/* Tooltip creation */
int*  __thiscall UI_CreateTooltip(void* tooltipMgr,
                                   int resourceId,
                                   int16_t param,
                                   int x, int y);        /* @ 0x423C50 */

/* Tooltip destruction — exact (void*, int) signature matching the one
 * real definition in shared/stubs_impl.cpp. Several OTHER files declare
 * this name with different parameter types (e.g. TrainStationWindow.cpp
 * uses (void*, void*)); those are distinct mangled symbols with no
 * definition of their own. Match the real one exactly here. */
void  UI_DestroyTooltip(void* tooltipMgr, int tooltip);  /* @ 0x423D20 */

/* Game pause toggle — called by SetVisible below. */
void  __fastcall CGWND_SetPause(void* self, char pause);  /* @ 0x408130 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern int32_t g_world_width;        /* 0x4AAD0C — world width in pixels     */
extern int32_t g_world_height;       /* 0x4AAD10 — world height in pixels    */
/* g_game_time declared as uint32_t in shared/types.h */
class UI_Manager;
extern UI_Manager* g_tooltip_mgr;    /* 0x4FD220 — tooltip manager singleton */

/* World center coordinates (computed from world dimensions) */
extern int32_t DAT_004aad34;         /* 0x4AAD34 — world center X            */
extern int32_t DAT_004aad38;         /* 0x4AAD38 — world center Y            */

/* ================================================================== */
/* Resource field offsets (used in the constructor)                     */
/* RESDATA at +0x40 of GameObject:                                      */
/*   +0x08: type (1 byte)                                              */
/*   +0x0C: childCount / childResourceId (int32)                       */
/*   +0x14: frame_width (uint16)                                       */
/*   +0x16: frame_height (uint16)                                      */
/*   +0x20: frameTable (FrameData*)                                    */
/*   +0x32: offset_x (int16, signed half-width for centering)          */
/*   +0x34: offset_y (int16, signed half-height for centering)         */
/*   +0x38: tooltipOffsetX (int32)                                     */
/*   +0x3C: tooltipDuration (int32)                                    */
/*   +0x168: variantField (byte, type-8 specific)                      */
/* ================================================================== */
#define RES_type(p)         (*reinterpret_cast<const uint8_t*>(reinterpret_cast<const uint8_t*>(p) + 0x08))
#define RES_childCount(p)   (*reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(p) + 0x0C))
#define RES_frameWidth(p)   (static_cast<const RESDATA*>(p)->frame_width)
#define RES_frameHeight(p)  (static_cast<const RESDATA*>(p)->frame_height)
#define RES_frameTable(p)   (static_cast<const RESDATA*>(p)->anim_table)
#define RES_offsetX(p)      (static_cast<const RESDATA*>(p)->offset_x)
#define RES_offsetY(p)      (static_cast<const RESDATA*>(p)->offset_y)
#define RES_tooltipX(p)     (*reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(p) + 0x38))
#define RES_tooltipDur(p)   (*reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(p) + 0x3C))
#define RES_variant(p)      (*reinterpret_cast<const uint8_t*>(reinterpret_cast<const uint8_t*>(p) + 0x168))

/* ================================================================== */
/* UIEntity::UIEntity — Constructor (vtable 0x477A90)                   */
/* Address: 0x422EC0                                                    */
/*                                                                      */
/* Called by: UI_CreateMessageBox (0x423BA5) to position dialog elements */
/*                                                                      */
/* Steps:                                                               */
/*   1. Construct Entity base(resourceId, param2, 0, 0) — 0x405790,     */
/*      Entity::Entity(int,int16_t,int,int). Ghidra's stale label for   */
/*      this address ("GameObject_BaseCtor") is a free-function         */
/*      mislabel; it is Entity's real constructor.                     */
/*   2. Set vtable to VTBL_UIENTITY                                     */
/*   3. Zero tooltip pointer, set animVariant=1                         */
/*   4. Store direction (toupper'd), worldX, worldY, field_8A           */
/*   5. If resource exists:                                             */
/*      a. Check if frame is static (first==last): set timer            */
/*      b. Determine animVariant: type-8 uses resource field, else      */
/*         random % 3 + 1                                               */
/*      c. Compute world position based on direction code               */
/*      d. If resource has children (+0x0C > 0), create tooltip         */
/* ================================================================== */
/* Returns sizeof(UIEntity) on this host (0xC8 bytes here vs. the original
 * x86's 0xA4 — pointer-bearing base fields widen). Exists so callers that
 * only need to size an allocation can get the real size without
 * `#include`-ing this header. */
size_t UIEntity_Size()
{
    return sizeof(UIEntity);
}

UIEntity::UIEntity(int32_t resourceId, int16_t param2, char direction,
                    int32_t x, int32_t y)
    : Entity(resourceId, param2, 0, 0)
{
    /* Step 2: Set vtable to UIEntity */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Step 3: Initialize tooltip pointer to NULL, animVariant = 1 */
    this->pTooltip = nullptr;                                             /* +0x98 */
    this->animVariant = 1;                                                 /* +0x94 */

    /* Step 4: Convert direction to uppercase and store all params */
    this->direction = static_cast<char>(CRT_toupper(static_cast<int>(direction)));
                                                                            /* +0x88 */
    this->worldX = x;                                                      /* +0x8C */
    this->worldY = y;                                                      /* +0x90 */
    this->field_8A = param2;                                               /* +0x8A */

    /* Step 5: If resource exists, process frame data and position */
    void* res = this->resource;  /* resource pointer at Entity+0x40 */
    if (res == nullptr) {
        return;
    }

    /*
     * Step 5a: Check frame timing
     * If first frame index == last frame index in the FrameData entry,
     * the sprite is static — set timer to current game time + duration.
     */
    int frameIdx = this->anim_index;                       /* +0x28 = anim_index */
    FrameData* frameEntry = RES_frameTable(res) + frameIdx;
    if (frameEntry->start_frame == frameEntry->end_frame) {
        this->timer = static_cast<uint32_t>(frameEntry->wait_time) + g_game_time;
                                                                            /* +0x58 timer */
    }

    /*
     * Step 5b: Determine animation variant
     * Type 8 uses resource's fixed variant field; otherwise random
     */
    if (RES_type(res) == 8) {
        this->animVariant = RES_variant(res);
    } else {
        this->animVariant = static_cast<uint8_t>(static_cast<uint32_t>(CRT_rand()) % 3 + 1);
    }

    /*
     * Step 5c: Compute world position based on direction code
     *
     * The direction codes use resource offset values (at +0x32/+0x34)
     * as the "half-size" of the sprite for centering, and frame_height
     * (at +0x16) as the full height in unsigned form.
     */

    switch (this->direction) {

    case 'C': /* Center — place at DAT_004aad34/DAT_004aad38 minus offsets */
    {
        int16_t offX = RES_offsetX(res);
        int16_t offY = RES_offsetY(res);
        this->worldX = DAT_004aad34 - offX;
        this->worldY = DAT_004aad38 - offY;
        break;
    }

    case 'D': /* Down — scatter downward; variant fixed to 1 */
    {
        this->animVariant = 1;
        int16_t offX = RES_offsetX(res);
        int16_t offY = RES_offsetY(res);

        if (x >= 0) {
            /* Positive x: use as reference offset */
            this->worldX = x - offX;
            this->worldY = y - offY;
        } else {
            /* Negative x: compute random position in spawn band */
            int negW = -offX;
            int rightEdge = g_world_width + offX;

            if (-rightEdge != offX && negW < rightEdge) {
                /* offX * -2 + 1: total band width */
                int bandTotal = offX * -2 + 1;
                if (bandTotal != g_world_width) {
                    this->worldX = static_cast<int>(static_cast<uint32_t>(CRT_rand()) % (bandTotal - g_world_width)) +
                                   g_world_width + offX;
                }
            } else {
                if (g_world_width + 1 + offX * 2 != 0) {
                    this->worldX = static_cast<int>(static_cast<uint32_t>(CRT_rand()) % (g_world_width + 1 + offX * 2)) - offX;
                }
                this->worldY = 0;
            }
        }
        break;
    }

    case 'P': /* Push right — from edge; push rightwards into view */
    {
        uint16_t resH = RES_frameHeight(res);
        int16_t offY = RES_offsetY(res);
        this->worldY = g_world_height;

        if (y >= 0) {
            this->worldX = offY + y;
        } else {
            /* Random Y within world minus resource height */
            int avail = g_world_height - static_cast<int>(resH);
            if (avail < offY) {
                int overflow = static_cast<int>(resH - g_world_height) + 1 + offY;
                if (overflow != 0) {
                    this->worldX = (static_cast<int>(CRT_rand()) % static_cast<int>((resH - g_world_height) + 1 + offY) +
                                    g_world_height) - static_cast<int>(resH);
                }
                this->worldX -= offY;
            } else if ((g_world_height - offY) - static_cast<int>(resH) == -1) {
                this->worldX = 0;
            } else {
                this->worldX = static_cast<int>(CRT_rand()) %
                               static_cast<int>((g_world_height - static_cast<uint32_t>(resH)) - offY + 1);
            }
        }
        goto position_and_randomize_x;
    }

    case 'R': /* Random — fully random within world bounds */
    {
        uint16_t resH = RES_frameHeight(res);
        int16_t offX = RES_offsetX(res);
        int16_t offY = RES_offsetY(res);

        /* Random Y */
        if (g_world_height - static_cast<int>(resH) < offY) {
            int overflow = static_cast<int>(resH - g_world_height) + 1 + offY;
            if (overflow != 0) {
                this->worldY = (static_cast<int>(CRT_rand()) % (static_cast<int>(offY) - g_world_height + 1 + static_cast<uint32_t>(resH)) +
                                g_world_height) - static_cast<uint32_t>(resH);
            }
        } else if ((g_world_height - offY) - static_cast<int>(resH) != -1) {
            this->worldY = static_cast<int>(CRT_rand()) %
                           static_cast<int>((g_world_height - static_cast<uint32_t>(resH)) - offY + 1) + offY;
        }

        /* Random X */
        if (g_world_width < offX) {
            if (offX - g_world_width != -1) {
                this->worldX = static_cast<int>(CRT_rand()) % ((offX - g_world_width) + 1) + g_world_width;
            }
            this->worldX -= offX;
        } else if (g_world_width - offX == -1) {
            this->worldX = 0;
        } else {
            this->worldX = static_cast<int>(CRT_rand()) % ((g_world_width - offX) + 1);
        }

        /* Subtract offset_y to get final Y */
        this->worldY -= offY;
        break;
    }

    case 'S': /* Spawn left — left edge, random Y */
    {
        uint16_t resH = RES_frameHeight(res);
        uint16_t resW = RES_frameWidth(res);
        int16_t offY = RES_offsetY(res);

        if (y >= 0) {
            this->worldX = y - offY;
        } else {
            int avail = g_world_height - static_cast<int>(resH);
            if (avail < offY) {
                int overflow = static_cast<int>(resH - g_world_height) + 1 + offY;
                if (overflow != 0) {
                    this->worldX = (static_cast<int>(CRT_rand()) % static_cast<int>((static_cast<uint32_t>(resH) - g_world_height) + 1 + offY) +
                                    g_world_height) - static_cast<uint32_t>(resH);
                }
                this->worldX -= offY;
            } else if ((g_world_height - offY) - static_cast<int>(resH) == -1) {
                this->worldX = 0;
            } else {
                this->worldX = static_cast<int>(CRT_rand()) %
                               static_cast<int>((g_world_height - static_cast<uint32_t>(resH)) - offY + 1);
            }
        }
        this->worldY = -static_cast<int32_t>(resW);
        goto position_and_randomize_x;
    }

    case 'U': /* Up — scatter upward; variant fixed to 1 */
    {
        this->animVariant = 1;
        int16_t offX = RES_offsetX(res);

        if (x >= 0) {
            this->worldX = x - offX;
            this->worldY = y - RES_offsetY(res);
        } else {
            int negW = -offX;
            int rightEdge = g_world_width + offX;

            if (-rightEdge != offX && negW < rightEdge) {
                int bandTotal = offX * -2 + 1;
                if (bandTotal != g_world_width) {
                    this->worldX = static_cast<int>(CRT_rand()) % (bandTotal - g_world_width) +
                                   g_world_width + offX;
                }
            } else {
                if (g_world_width + 1 + offX * 2 != 0) {
                    this->worldX = static_cast<int>(CRT_rand()) % (g_world_width + 1 + offX * 2) - offX;
                }
            }
            this->worldY = g_world_height;
        }
        goto apply_world_pos;
    }

    case 'W': /* West — place to the left of reference */
    {
        int16_t offX = RES_offsetX(res);
        int16_t offY = RES_offsetY(res);
        this->worldX = x - offX;
        this->worldY = y - offY;
        goto apply_world_pos;
    }

    }  /* switch (direction) */

    /*
     * Position-and-randomize-X fall-through for cases P and S
     */
    {
        position_and_randomize_x:
        GameObject_SetWorldPos(this, this->worldX, this->worldY);

        if (x == -1) {
            /* Randomize X if x was default */
            if (g_world_width < 0) {
                if (g_world_width != -1) {
                    this->worldX = static_cast<int>(CRT_rand()) % (1 - g_world_width) + g_world_width;
                }
            } else if (g_world_width == -1) {
                this->worldX = 0;
            } else {
                this->worldX = static_cast<int>(CRT_rand()) % (g_world_width + 1);
            }
        }
        goto apply_world_pos;
    }

    {
        apply_world_pos:
        GameObject_SetWorldPos(this, this->worldX, this->worldY);
    }

    /*
     * Step 6: Create tooltip if resource has children
     * Resource childCount at +0x0C > 0
     */
    if (RES_childCount(res) > 0) {
        int32_t tooltipXOff;
        int32_t tooltipYOff;

        if (RES_tooltipDur(res) <= 0) {
            /* No fixed duration: use random offset 0x28..0x46 */
            tooltipXOff = RES_tooltipX(res);
            tooltipYOff = static_cast<int>(CRT_rand()) % 0x1F + 0x28;
        } else {
            /* Fixed duration from resource */
            tooltipXOff = RES_tooltipX(res);
            tooltipYOff = RES_tooltipDur(res);
        }

        this->tooltipOffsetX = tooltipXOff;                                /* +0x9C */
        this->tooltipOffsetY = tooltipYOff;                                /* +0xA0 */

        /* Create tooltip child object */
        this->pTooltip = UI_CreateTooltip(
            g_tooltip_mgr,
            RES_childCount(res),     /* Note: overloaded — child resource ID */
            this->field_8A,
            this->screen_rect.left + tooltipXOff,
            this->screen_rect.top + tooltipYOff);
    }
}

/* ================================================================== */
/* UIEntity::~UIEntity — Destructor                                    */
/* Address: 0x423500 (body; Ghidra auto-named this "UI_Window_Dtor" —  */
/* a misnomer inherited from the vtable's default label, not a         */
/* drawing function). Scalar deleting destructor wrapper (vtable[0],   */
/* 0x477A90): 0x4234E0 ("UI_DtorWrapper" — also a misnomer, not a      */
/* WndProc wrapper).                                                    */
/*                                                                      */
/* Destroys the tooltip child (if any) via UI_DestroyTooltip. The       */
/* binary's explicit `this->vtbl = VTBL_UIENTITY` reset (defends        */
/* against re-entrant virtual calls during unwind) and its call to      */
/* GameObject_DtorBody are compiler-managed base-destructor chaining    */
/* in real C++ (Entity::~Entity -> GameObject::~GameObject runs         */
/* automatically after this body).                                     */
/*                                                                      */
/* NOTE: a previous, unrelated `class UIEntity { virtual ~UIEntity(); }`*/
/* shadow definition lived in shared/vtable_stubs.cpp and silently      */
/* supplied this exact destructor symbol with an assert-stub body       */
/* (an ODR violation — two same-named classes with the same mangled     */
/* destructor symbol in different translation units). It has been      */
/* removed; this is now the one real definition.                       */
/* ================================================================== */
UIEntity::~UIEntity()
{
    if (this->pTooltip != nullptr) {
        UI_DestroyTooltip(g_tooltip_mgr,
                           static_cast<int>(reinterpret_cast<intptr_t>(this->pTooltip)));
    }
}

/* ================================================================== */
/* UIEntity::StopSound — vtable[7] override                            */
/* Address: 0x423840 (previously the free function UI_ShowWindow)      */
/*                                                                     */
/* pTooltip (+0x98) is created via GameObject_BaseCtor as a plain      */
/* 0x88-byte object (see UI_Manager::createTooltip's doc comment,      */
/* ui/UI_Utils.h) — exactly Entity's size — and dispatched through its */
/* own vtable[7]/[9]/[10] slots by the original manual-vtable code     */
/* this replaces, so it is evidenced as an Entity*.                    */
/* ================================================================== */
void UIEntity::StopSound(int param)
{
    if (this->pTooltip != nullptr) {
        static_cast<Entity*>(this->pTooltip)->StopSound(param);
    }
    Entity::StopSound(param);
}

/* ================================================================== */
/* UIEntity::SetVisible — vtable[9] override                           */
/* Address: 0x423890 (previously the free function UI_EnableWindow)    */
/* ================================================================== */
void UIEntity::SetVisible(bool visible)
{
    if (this->pTooltip != nullptr) {
        static_cast<Entity*>(this->pTooltip)->SetVisible(visible);
    }
    CGWND_SetPause(this, static_cast<char>(visible));
}

/* ================================================================== */
/* UIEntity::Update — vtable[10] override                              */
/* Address: 0x423870 (previously the free function UI_HideWindow)      */
/* ================================================================== */
void UIEntity::Update()
{
    if (this->pTooltip != nullptr) {
        static_cast<Entity*>(this->pTooltip)->Update();
    }
    this->InvalidateRect();
}
