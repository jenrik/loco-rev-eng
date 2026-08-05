/**
 * NET_Dtor — Compute 24-bit RGB color from postcard data bytes
 * Address: 0x4441C0
 * Size: 176 bytes
 * Calling convention: __stdcall
 *
 * NOT a destructor despite the name. Takes three byte parameters from
 * postcard data (such as color components from DPLAY_RenderPlayer) and
 * computes an RGB-like 24-bit color value. Each byte adjusts a channel:
 *   param_2 (green): base = 0xFF - param2, then +param2/4, +param2/2
 *   param_3 (blue):  subtracts from green-adjusted values
 *   param_1 (red):   subtracts from all channels, +param1/3 to green
 * Results are clamped to [0, 255].
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

/* ================================================================== */
/* NET_Dtor                                                            */
/* ================================================================== */
uint32_t __stdcall NET_Dtor(uint8_t param1, uint8_t param2, uint8_t param3)
{
    int32_t r = 0xFF;
    int32_t g = 0xFF;
    int32_t b = 0xFF;

    if (param2 != 0) {
        r = 0xFF - (uint32_t)param2;
        b = (param2 >> 2) + 0xFF;
        g = (param2 >> 1) + 0xFF;
    }

    if (param3 != 0) {
        b = b - param3;
        g = g - (uint32_t)(param3 >> 1);
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

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
