// Status: TRANSCRIBED
/**
 * sdl3_directplay_train_bridge.cpp — DirectPlay provider boundary for
 * TrainSubsystem::TrainSubsystem (0x438BC0).
 *
 * The original calls DirectPlay to enumerate Windows transport providers.
 * SDL builds have no DirectPlay service provider, so enumeration correctly
 * succeeds with an empty list while preserving constructor control flow.
 */

void* DirectPlay_CreatePeer(void* peer, int /*context_a*/, int /*context_b*/)
{
    // Address: 0x45E490. The host has no DirectPlayAddress COM object.
    return peer;
}

void* DirectPlay_EnumConnections(void* /*peer*/)
{
    // Address: 0x45EAB0. No DirectPlay transports are available on SDL.
    return nullptr;
}

int DirectPlay_QueryConnection(const char* /*index*/)
{
    // Address: 0x45EE60. Provider capability is false when no provider exists.
    return 0;
}
