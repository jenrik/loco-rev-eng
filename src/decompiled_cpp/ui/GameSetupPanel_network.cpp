/**
 * GameSetupPanel network/session methods recovered from loco.exe.
 *
 * Status: VALIDATED
 */

#include "../ui/GameSetupPanel.h"
#include "../network/Netman.h"
#include "../game/PlayerConfig.h"
#include "../game/Train.h"

#ifndef _WIN32
#include "../../sdl3_shims/sdl3_net_runtime.h"
#endif

#include <cstring>
#include <new>

extern Netman* _g_netman;
extern void* _g_train;
extern void* operator_new(std::size_t size);
extern void GLOBAL_free(void* pointer);
extern void EditorState_StartNewGame(void* panel);  // 0x40A150
extern void UIPANEL_EndPaintEx(void* panel, HWND window, void* dc, byte flags, void* rect);
#ifndef _WIN32
extern char* _g_netman_state;
#endif

namespace {
std::uint32_t CurrentPlayerInfo()
{
    return static_cast<std::uint32_t>(g_player_id) |
           (static_cast<std::uint32_t>(g_player_color) << 16);
}
const char* CurrentPlayerName()
{
    return g_player_config ? g_player_config->name : "Player";
}
}

/** GameSetupPanel::ConnectToNetworkGame
 *  Address: 0x40AA20 */
void GameSetupPanel::ConnectToNetworkGame(int32_t index)
{
#ifndef _WIN32
    const auto transport = lego_loco::network::HostTransportWorker().Snapshot();
    const bool ready =
        (transport.mode == lego_loco::network::TransportRuntimeMode::Host &&
         transport.state == lego_loco::network::TransportRuntimeState::Listening) ||
        (transport.mode == lego_loco::network::TransportRuntimeMode::Client &&
         transport.state == lego_loco::network::TransportRuntimeState::Connected);
    if (ready && _g_netman != nullptr && _g_netman->m_bInit != 0 &&
        _g_netman->m_bFlag1 != 0) {
        this->selectedEntry = index;
        this->field_110 = 0;
        this->hostSessionReady = true;
        return;
    }
    this->hostSessionReady = false;
    return;
#else
    (void)index;  // 0x40AA20 uses selectedEntry (+0xF4), not its stack argument.
    LayoutListNode* selected = this->titleList;
    _g_netman->ResetNetworkState();
    for (int32_t remaining = this->selectedEntry;
         remaining != 0 && selected != nullptr; --remaining) {
        selected = selected->next;
    }
    if (selected == nullptr) return;

    this->field_FC = static_cast<int32_t>(reinterpret_cast<intptr_t>(selected->name));
    _g_netman->LoadScenario(selected->name);
    _g_netman->SendLayoutSelect(-1, 0, CurrentPlayerName(), CurrentPlayerInfo());
    this->field_110 = 0;
    _g_netman->StopSession();
    NETMAN_StartClientSession();
    NETMAN_StartHostSession();
    if (this->renderFlag != 0) {
        this->drawGrid();
        this->updateTitle();
        UIPANEL_EndPaintEx(this, this->hWnd, nullptr, 0, nullptr);
    }
#endif
}

/** GameSetupPanel::SelectLayoutEntry
 *  Address: 0x40AAF0 */
void GameSetupPanel::SelectLayoutEntry(int32_t index)
{
#ifndef _WIN32
    if (_g_netman_state != nullptr && _g_netman_state[8] != 0) return;
#else
    extern char* _g_netman_state;
    if (_g_netman_state[8] != 0) return;
#endif
    LayoutListNode* selected = this->layoutList;
    int32_t walked = 0;
    if (selected != nullptr) {
        for (int32_t remaining = index; remaining != 0; --remaining) {
            selected = selected->next;
            walked = index;
        }
    }
    if (walked == index && selected != nullptr) {
        this->selectedEntry = index;
        this->field_F8 = static_cast<int32_t>(reinterpret_cast<intptr_t>(selected->name));
        _g_netman->ResetNetworkState();
        this->updateTitle();
        UIPANEL_EndPaintEx(this, this->hWnd, nullptr, 0, nullptr);
        _g_netman->StopSession();
        NETMAN_StartClientSession();
        NETMAN_StartHostSession();
        return;
    }
    this->selectedEntry = 0;
    EditorState_StartNewGame(this);
}

/** GameSetupPanel::HandleMapClick
 *  Address: 0x40ABA0 */
void GameSetupPanel::HandleMapClick(int32_t clickX, int32_t clickY)
{
    const int32_t cellHeight = (this->gridRect.bottom - this->gridRect.top) / 3;
    const int32_t cellWidth = (this->gridRect.right - this->gridRect.left) / 3;
    if (cellHeight == 0 || cellWidth == 0 || _g_netman == nullptr) return;
    const int32_t row = (clickY - this->gridRect.top) / cellHeight;
    const int32_t column = (clickX - this->gridRect.left) / cellWidth;
    if (column + 1 <= _g_netman->m_playerRows &&
        row + 1 <= _g_netman->m_playerCols) {
        const int32_t scenario = row * _g_netman->m_playerRows + column;
        if (scenario != this->field_110) {
            this->SendScenarioSelect(scenario);
            this->updateTitle();
            this->drawGrid();
            UIPANEL_EndPaintEx(this, this->hWnd, nullptr, 0, nullptr);
        }
    }
}

/** GameSetupPanel::SendScenarioSelect
 *  Address: 0x40AC50 */
void GameSetupPanel::SendScenarioSelect(int32_t scenarioIndex)
{
#ifndef _WIN32
    const bool networkSelected = _g_netman_state != nullptr && _g_netman_state[8] != 0;
#else
    extern char* _g_netman_state;
    const bool networkSelected = _g_netman_state[8] != 0;
#endif
    if (networkSelected) {
        const int32_t source = _g_netman->m_bFlag1 != 0 ? _g_netman->m_myDpId : -1;
        _g_netman->SendLayoutSelect(source, scenarioIndex,
                                    CurrentPlayerName(), CurrentPlayerInfo());
        this->field_110 = scenarioIndex;
        return;
    }

    auto* packet = static_cast<std::uint8_t*>(operator_new(0x1c));
    if (!packet) return;
    std::memset(packet, 0, 0x1c);
    packet[0] = 0xf0;
    packet[1] = 0x03;
    std::memcpy(packet + 4, &scenarioIndex, sizeof(scenarioIndex));
    std::strncpy(reinterpret_cast<char*>(packet + 8), CurrentPlayerName(), 12);
    std::memcpy(packet + 0x16, &g_player_id, sizeof(g_player_id));
    std::memcpy(packet + 0x18, &g_player_color, sizeof(g_player_color));

    void* messageStorage = operator_new(sizeof(TrainMessage));
    if (!messageStorage) { GLOBAL_free(packet); return; }
    auto* message = ::new (messageStorage) TrainMessage{};
    message->type = 6;
    message->data_len = 0x1c;
    message->data_ptr = packet;
    auto* train = static_cast<TrainSubsystem*>(_g_train);
    message->target_dpId = train ? train->player_peer_id : 0;
    message->flags = 1;
    _g_netman->m_bInit = 0;
    if (train) train->QueueMessage(message);
    else { GLOBAL_free(packet); GLOBAL_free(message); }
}
