/**
 * NET_ComputeColor — Compute 24-bit RGB color from postcard data bytes
 * Address: 0x4441C0
 * Size: 176 bytes
 * Calling convention: __stdcall
 *
 * Ghidra's own auto-analysis names this function NET_ComputeColor — this
 * file was originally named/authored under the stale default decompiler
 * label "NET_Dtor", which is NOT a destructor (renamed per evidence; see
 * network/Netman.h, which previously carried the same misnomer).
 *
 * Takes three byte parameters from postcard data (such as color components
 * from DPLAY_RenderPlayer) and computes an RGB-like 24-bit color value.
 * Each byte adjusts a channel:
 *   param_2 (green): base = 0xFF - param2, then +param2/4, +param2/2
 *   param_3 (blue):  subtracts from green-adjusted values
 *   param_1 (red):   subtracts from all channels, +param1/3 to green
 * Results are clamped to [0, 255]. Verified against the 0x4441C0
 * disassembly: all six clamp branches (three low-end, three high-end) use
 * signed JGE/JLE, matching the plain `int32_t` comparisons below exactly —
 * no unsigned/signed asymmetry despite the decompiler's own pseudo-C
 * showing mismatched `int`/`uint` variable annotations for the same
 * comparisons (a Ghidra type-inference artifact, not a real difference in
 * the underlying instructions).
 *
 * Called by:
 *   DPLAY_RenderPlayer (0x4437C0) — to compute fill color from postcard data
 *
 * @param param1  Red channel adjustment
 * @param param2  Green channel adjustment
 * @param param3  Blue channel adjustment
 * @return        24-bit RGB value packed as 0x00RRGGBB
 */
#include "../shared/types.h"

/* Canonical declaration lives in network/Netman.h; this local prototype
 * (matching it exactly) satisfies -Wmissing-declarations without pulling
 * in Netman.h's much larger include graph for this one free function. */
extern uint32_t __stdcall NET_ComputeColor(uint8_t param1, uint8_t param2, uint8_t param3);

/* ================================================================== */
/* NET_ComputeColor                                                    */
/* ================================================================== */
uint32_t __stdcall NET_ComputeColor(uint8_t param1, uint8_t param2, uint8_t param3)
{
    int32_t r = 0xFF;
    int32_t g = 0xFF;
    int32_t b = 0xFF;

    if (param2 != 0) {
        r = 0xFF - static_cast<int32_t>(param2);
        b = (param2 >> 2) + 0xFF;
        g = (param2 >> 1) + 0xFF;
    }

    if (param3 != 0) {
        b = b - param3;
        g = g - static_cast<int32_t>(param3 >> 1);
        r = r + param3 / 3;
    }

    if (param1 != 0) {
        g = g - param1;
        r = r - param1;
        b = b + param1 / 3;
    }

    /* Clamp all channels to [0, 255] */
    if (b < 0)   b = 0;
    if (g < 0)   g = 0;
    if (r < 0)   r = 0;
    if (b > 0xFF) b = 0xFF;
    if (g > 0xFF) g = 0xFF;
    if (r > 0xFF) r = 0xFF;

    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}
