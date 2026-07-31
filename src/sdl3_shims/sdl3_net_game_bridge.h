#pragma once

#ifndef _WIN32

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Netman;
class TrainSubsystem;

namespace lego_loco::network {

/** Allocate the host-native Netman size; the original 0x804-byte x86
 * allocation is intentionally not imposed on pointer-wide host objects. */
Netman* CreateHostNetman();

/** Drain transport events on the game thread before Netman::Update. */
void PumpTransportIntoGame(Netman* netman, TrainSubsystem* train,
                           const char* local_player_name);

/** Convert one owned legacy payload into the same TrainMessage/Netman queue
 * path used by TrainSubsystem::ProcessMessages. Unsupported original service
 * packets fail explicitly so accepted bytes are never silently discarded. */
bool QueueLegacyPayloadForGame(Netman* netman, TrainSubsystem* train,
                               std::uint32_t sender,
                               const std::vector<std::uint8_t>& payload,
                               std::string* error);

}  // namespace lego_loco::network

#endif
