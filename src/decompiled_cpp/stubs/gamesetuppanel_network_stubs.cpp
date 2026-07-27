/**
 * gamesetuppanel_network_stubs.cpp — Stubs for GameSetupPanel network methods
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These four methods are extended vtable slots [12]-[15] on the
 * GameSetupPanel class. They handle network game session management
 * and are not yet decompiled. Each stub fails loudly via assert()
 * to prevent silent incorrect behavior.
 */

// Status: TRANSCRIBED

#include "../ui/GameSetupPanel.h"
#include <cassert>

/* ================================================================== */
/* GameSetupPanel::HandleMapClick — Handle click on the scenario grid */
/* Address: 0x40ABA0  Vtable slot: [12]                               */
/* TODO: decompile 0x40ABA0                                            */
/* ================================================================== */
void GameSetupPanel::HandleMapClick(int32_t clickX, int32_t clickY)
{
    assert(!"stub: GameSetupPanel::HandleMapClick not decompiled");
}

/* ================================================================== */
/* GameSetupPanel::SelectLayoutEntry — Select a single-player layout  */
/* Address: 0x40AAF0  Vtable slot: [13]                                */
/* TODO: decompile 0x40AAF0                                             */
/* ================================================================== */
void GameSetupPanel::SelectLayoutEntry(int32_t index)
{
    assert(!"stub: GameSetupPanel::SelectLayoutEntry not decompiled");
}

/* ================================================================== */
/* GameSetupPanel::SendScenarioSelect — Send the selected scenario     */
/* Address: 0x40AC50  Vtable slot: [14]                                */
/* TODO: decompile 0x40AC50                                             */
/* ================================================================== */
void GameSetupPanel::SendScenarioSelect(int32_t scenarioIndex)
{
    assert(!"stub: GameSetupPanel::SendScenarioSelect not decompiled");
}

/* ================================================================== */
/* GameSetupPanel::ConnectToNetworkGame — Join a network session       */
/* Address: 0x40AA20  Vtable slot: [15]                                */
/* TODO: decompile 0x40AA20                                             */
/* ================================================================== */
void GameSetupPanel::ConnectToNetworkGame(int32_t index)
{
    assert(!"stub: GameSetupPanel::ConnectToNetworkGame not decompiled");
}
