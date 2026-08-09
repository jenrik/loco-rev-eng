/**
 * UI_ListBox.h — historical location of the list-box item-list logic
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * As of 2026-08-09 the free functions that used to live in this file
 * (each taking an explicit `void* self` — the CLAUDE.md free-function-
 * with-explicit-self anti-pattern) were converted into real C++ methods:
 *
 *   UI_DrawListBox   (0x424550) -> ScrollCollection::DrawListBox
 *                                   (shared/collections.h)
 *   UI_ListBox_Clear (0x424A00) -> ScrollCollection::Destructor
 *                                   (shared/collections.h)
 *   UI_ListBox_FindItem (0x424820) -> already had a canonical integrated
 *                                   twin, SortedCollection::FindItem
 *                                   (shared/collections.h/.cpp), which was
 *                                   the actual live implementation; this
 *                                   file's copy was dead leftover code,
 *                                   removed rather than converted. Its
 *                                   BUG comment (the recursive high-bound
 *                                   passing `target` instead of `high`)
 *                                   was reconciled into the surviving
 *                                   SortedCollection::FindItem, which had
 *                                   silently "fixed" that divergence from
 *                                   the original — see collections.cpp.
 *
 * See docs/landmine-sweep-worklist.md for the full evidence trail.
 * This file is kept (rather than deleted) as the historical landmark for
 * anyone grepping the old names; it declares nothing.
 */

#pragma once

/* Nothing declared here — see shared/collections.h for ScrollCollection
 * and SortedCollection. */
