/**
 * SaveSprite.h — Savegame/backdrop file-list entry (UIPANEL file browser)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * SaveSprite is a small, standalone, ROOT class (no base class) — one
 * entry in the sorted doubly-linked list of savegame (*.sav) or backdrop
 * (*.bmp) files that UIPANEL::DrawEditField (0x429490) builds for the
 * panel's file-browser UI, and that UIPANEL::CreateSprite/BlitSprite/
 * BlitSpriteEx (0x429850/0x429B20/0x429DD0) walk to populate/save/delete
 * entries.
 *
 * Confirmed root class, single-slot vtable: the vtable at 0x477D24
 * (formerly "VTBL_UIPANEL_SAVESPRITE") has exactly ONE entry — the
 * destructor (0x429830). The four further dwords a raw 32-byte read at
 * 0x477D24 initially appears to show (0x42A140, 0, 0x42CD60, 0x454890...)
 * are NOT further SaveSprite slots: they are the START of the very next
 * global vtable in memory, 0x477D28 (UIPANEL_Surface's own single-entry
 * vtable — see graphics/LOCOBITMAP.h's doc comment, which independently
 * documents 0x477D28 the same way). Decompiling those addresses confirms
 * they belong to unrelated classes (UIPANEL_Surface::~UIPANEL_Surface,
 * GameView::~GameView, GameObject::PtInRect) — pure adjacency in the
 * data segment, not inheritance.
 *
 * Original x86 size: 0x230 bytes.
 * Vtable: 0x477D24 (single slot: destructor).
 *
 * Original x86 field layout (confirmed via disassembly, not decompiler
 * text alone, of UIPANEL_DrawEditField (0x429490), UIPANEL_FreeSprite
 * (0x429820), UIPANEL_DtorSprite (0x429830), UIPANEL_CreateSprite
 * (0x429850), UIPANEL_BlitSprite (0x429B20)):
 *
 *   +0x00: vtable
 *   +0x04: name[11]     -- strncpy'd file basename: 10-byte copy
 *                          (`_strncpy(dst, src, 0xA)`) + one explicit
 *                          extra NUL at +0x0E. The caller accepts
 *                          basenames of length <= 10 (`CMP ECX,0xA` /
 *                          `JA skip`, i.e. reject only when > 10 — a
 *                          10-char basename fits exactly, using all 10
 *                          strncpy bytes with no room for its own NUL;
 *                          the guaranteed byte at +0x0E supplies it).
 *   +0x0F: prefix[0x41] -- "savegame\\" or "backdrop\\" (strcpy) +
 *                          strncat'd original filename (with extension,
 *                          max 0x40 bytes) + one explicit extra NUL,
 *                          ending exactly where RESDATA begins
 *                          (+0x0F + 0x41 == +0x50, zero gap).
 *   +0x50: RESDATA data -- embedded resource header (original x86 size
 *                          0x1D8; widened on this 64-bit host — see
 *                          shared/types.h. Composing it as a REAL member
 *                          here, rather than a raw `sprite_mem+0x50`
 *                          pointer cast into a fixed operator_new(0x230)
 *                          allocation as the previous pseudo-layout did,
 *                          is what fixes that layout's unconditional
 *                          0xD0-byte heap overflow on every construction
 *                          — the compiler now sizes the whole object
 *                          correctly instead of writing a full-sized
 *                          host RESDATA past a fixed 0x230-byte buffer.)
 *   +0x228: SaveSprite* prev  -- backward link.
 *   +0x22C: SaveSprite* next  -- forward link.
 * Total: 0x230 bytes (0x50 header + 0x1D8 RESDATA + 4 + 4).
 *
 * next/prev DIRECTION, confirmed independently three ways:
 *   1. The final fixup in DrawEditField, after all insertions, writes
 *      `head->[+0x228] = NULL` (`MOV dword ptr [EAX+0x228],0`) — only
 *      sensible for "prev" (a list head has no earlier node).
 *   2. DrawEditField's own sorted-insertion loop walks the list via
 *      `cur = cur->[+0x22C]` while `cmp(cur->name, new->name) < 0`
 *      (ascending order, so advancing must move toward larger names —
 *      i.e. FORWARD), and on breaking, writes
 *      `newNode->[+0x22C] = cur; newNode->[+0x228] = prevNode;
 *       prevNode->[+0x22C] = newNode; if (cur) cur->[+0x228] = newNode;`
 *      — the classic sentinel-anchored doubly-linked sorted insert,
 *      with the sentinel's own "next" slot deliberately aliased (by the
 *      compiler's stack layout) onto the local `sprite_list` head
 *      variable, so writing through it also updates the real head when
 *      inserting at the front. (This aliasing is an MSVC stack-layout
 *      artifact of the ORIGINAL binary — reproduced here only as prose,
 *      not as pointer arithmetic; the reimplementation below uses an
 *      ordinary `SaveSprite** insertAt` instead.)
 *   3. UIPANEL::BlitSprite's two scroll loops are consistent with (1)/(2)
 *      once the comparison's argument order is read correctly: it
 *      computes `cmp = strcmp(first_displayed->name-ish state,
 *      saved_state)` (first MINUS saved, not saved MINUS first). The
 *      "scroll toward earlier files" loop (`cmp > 0`, i.e. first >
 *      saved) advances via `anchor->[+0x228]` (prev); the "scroll
 *      toward later files" loop (`cmp < 0`) advances via
 *      `anchor->[+0x22C]` (next). Both directions agree with (1)/(2).
 *
 * NOTE: this is the OPPOSITE of a stale doc comment previously in
 * ui/UIPANEL_Draw.cpp (which claimed +0x228=next/+0x22C=prev), and the
 * previous pseudo-layout's own DrawEditField insertion code genuinely
 * walked/linked via the wrong field throughout — a real, separate
 * correctness bug (list corruption / wrong sort order) fixed alongside
 * the heap overflow. UIPANEL::CreateSprite/BlitSprite/BlitSpriteEx's
 * *existing* use of +0x22C as "next" was already correct and is
 * preserved unchanged in intent (now expressed via `->next`).
 *
 * Sort comparison during insertion is CASE-INSENSITIVE: the function at
 * 0x471480 (auto-named "CRT_wcsstr" by Ghidra — not accurate; it is not
 * a wide-string search) implements the classic MSVC byte-by-byte
 * case-fold compare (`SUB AL,0x41; CMP AL,0x1A; SBB CL,CL; AND CL,0x20;
 * ADD AL,CL; ADD AL,0x41` applied to both operands before comparing —
 * i.e. `_stricmp`), called as `cmp(cur_name, new_name)`. This is a
 * shared CRT helper with ~69 call sites across the tree (Building,
 * TrainStation, Cursor, AssetMgr, InputMgr, BuildingDescriptorEditor,
 * UI_ChildWindow, ...) — renaming/retyping it everywhere is out of
 * scope for this class; only Ghidra's own function name was corrected
 * (see get_xrefs_to 0x471480 for the full call-site list before doing
 * that wider rename). NOTE ALSO: the current native host implementation
 * (shared/stubs_impl.cpp: `CRT_wcsstr(a,b) { return strstr(a,b) ? 1:0;
 * }`) does NOT reproduce this semantic at all — it does substring
 * containment, returning only 0 or 1, never a negative value. An
 * insertion loop that advances only while the comparator result is
 * negative would therefore never advance at all on host, silently
 * breaking alphabetical sorting (every new entry lands at the head).
 * This is a real, tree-wide bug affecting that shared function's ~69
 * callers (tracked separately in PROGRESS.md — fixing the shared stub
 * itself is out of scope here, since several of those callers are
 * unrelated subsystems and expect a substring-search return convention,
 * not a 3-way compare). SaveSprite's own insertion loop
 * (ui/UIPANEL_Draw.cpp: UIPANEL_InsertSpriteSorted) avoids the bug
 * entirely by calling the real CRT_wcsstr only under `#ifdef _WIN32`
 * and using `strcasecmp` on host instead — it never routes through the
 * broken shared stub.
 *
 * Scroll-to-match comparisons elsewhere (UIPANEL::BlitSprite) use a
 * DIFFERENT, case-SENSITIVE inlined strcmp (no case-fold instructions),
 * confirmed by direct byte-for-byte disassembly — this is intentional,
 * not a bug to reconcile with the sort comparator above.
 */

// Status: INTEGRATED

#pragma once

#include "../shared/types.h"

class SaveSprite {
public:
    char        name[11];     // +0x04  file basename, NUL-terminated (<= 10 chars)
    char        prefix[0x41]; // +0x0F  "savegame\\"/"backdrop\\" + original filename
    RESDATA     data;         // +0x50  embedded resource header
    SaveSprite* prev;         // +0x228 backward link (NULL for the list head)
    SaveSprite* next;         // +0x22C forward link (NULL for the list tail)

    /**
     * Constructor. Synthesized from UIPANEL::DrawEditField's inlined
     * per-entry construction (0x429490, ~0x42964A-0x4296E7): initializes
     * `data` via RESMGR_ResourceData_Init, copies `basename` into `name`
     * (bounded to 10 chars + guaranteed NUL), and builds `prefix` from
     * `path_prefix` + `original_filename` (bounded, guaranteed NUL).
     * `prev`/`next` start NULL; the caller links the node into the
     * sorted list immediately after construction.
     *
     * NOTE on `data`'s residual fields: RESMGR_ResourceData_Init (0x447B20,
     * decompiled directly, not inferred) zeros only 9 specific dwords/words
     * -- vtable, save_pixels, primary_stream, secondary_stream, asset_data,
     * asset_size, and save.type/player_id/player_color -- NOT the whole
     * struct. This is confirmed to match the ORIGINAL per-node construction
     * exactly: the disassembly of the heap-allocated case (0x42962E-0x429667)
     * calls RESMGR_ResourceData_Init directly on freshly-`operator_new`'d
     * memory with no memset first (unlike DrawEditField's OWN local scratch
     * RESDATA, which does memset before Init -- that's a different object,
     * see UIPANEL::DrawEditField). Every field this class's real consumers
     * read before a RESMGR_LoadResource populates them for real (`save.type`,
     * checked by RESMGR_IsSaveHeader/UIPANEL::CreateSprite) IS one of the 9
     * zeroed fields; the rest (resource_id, frame dimensions, anim_table,
     * entity_buffer, save.name/entity_count/vehicle_count, ...) are genuinely
     * indeterminate until loaded, same as the original binary -- not
     * additionally zeroed here, to preserve rather than paper over that
     * evidenced behavior.
     *
     * @param basename          File basename without extension (<= 10
     *                          chars; caller enforces the length gate
     *                          before allocating).
     * @param path_prefix       "savegame\\" or "backdrop\\".
     * @param original_filename Original filename (with extension),
     *                          appended to path_prefix.
     */
    SaveSprite(const char* basename, const char* path_prefix,
               const char* original_filename);

    /* The binary never copies a SaveSprite (only ever heap-allocated via
     * operator_new(0x230) inside DrawEditField and linked into the
     * sorted list); no copy semantics to preserve. */
    SaveSprite(const SaveSprite&) = delete;
    SaveSprite& operator=(const SaveSprite&) = delete;

    /**
     * Destructor (vtable[0], the class's only virtual slot).
     * Address: 0x429830 (UIPANEL_DtorSprite, scalar deleting dtor) /
     * body at 0x429820 (UIPANEL_FreeSprite). Releases the embedded
     * RESDATA via RESMGR_ReleaseResource. The original FreeSprite body
     * also re-wrote the vtable pointer before releasing — a compiler
     * vptr-reset artifact of the destructor sequence, not reproduced
     * here; the compiler manages the vptr.
     */
    virtual ~SaveSprite();
};

#if UINTPTR_MAX == 0xffffffffu
static_assert(offsetof(SaveSprite, name) == 0x04,
              "SaveSprite::name offset mismatch");
static_assert(offsetof(SaveSprite, prefix) == 0x0F,
              "SaveSprite::prefix offset mismatch");
static_assert(offsetof(SaveSprite, data) == 0x50,
              "SaveSprite::data offset mismatch");
static_assert(offsetof(SaveSprite, prev) == 0x228,
              "SaveSprite::prev offset mismatch");
static_assert(offsetof(SaveSprite, next) == 0x22C,
              "SaveSprite::next offset mismatch");
static_assert(sizeof(SaveSprite) == 0x230,
              "SaveSprite size mismatch (expected 0x230)");
#endif
