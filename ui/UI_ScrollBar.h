/**
 * UI_ScrollBar.h — historical location of the scroll-bar item-list logic
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * As of 2026-08-09 the 7 free functions that used to live in this file
 * (each taking an explicit `void* self` — the CLAUDE.md free-function-
 * with-explicit-self anti-pattern) were converted into real C++ methods
 * on `ScrollCollection` in shared/collections.h — the class turned out to
 * be a genuinely shared base used well beyond ui/ (also by
 * game/BuildingComplex.cpp's two TimerCollections; not modified here).
 * See docs/landmine-sweep-worklist.md for the full evidence trail.
 *
 * Mapping from the old free functions to their new home:
 *
 *   UI_DrawScrollBar      (0x424040) -> ScrollCollection::DrawScrollBar
 *   UI_HandleScrollMessage(0x4241E0) -> ScrollCollection::RemoveAt (override)
 *   UI_GetScrollPos       (0x424250) -> ScrollCollection::RemoveAll (override)
 *   UI_SetScrollPos       (0x424270) -> ScrollCollection::DestroyAll (override)
 *   UI_InitScrollBar      (0x424460) -> ScrollCollection::Destructor
 *   UI_FreeScrollBar      (0x424490) -> ScrollCollection::SetKey
 *   UI_EnableScrollBar    (0x424510) -> Collection::DestroyAll (base body;
 *                                        ScrollCollection overrides it —
 *                                        see UI_SetScrollPos above)
 *
 * UI_GetScrollPos/UI_SetScrollPos/UI_EnableScrollBar/UI_FreeScrollBar were
 * all dispositively mis-named in the original transcription — none of them
 * reads or writes anything resembling a scroll position or enable flag, or
 * frees memory. Renamed based on confirmed behavior (see collections.h);
 * where original intent beyond the confirmed mechanism was not
 * recoverable, that is stated explicitly there rather than guessed.
 *
 * This file is kept (rather than deleted) as the historical landmark for
 * anyone grepping the old names; it declares nothing.
 */

#pragma once

/* Nothing declared here — see shared/collections.h for ScrollCollection. */
