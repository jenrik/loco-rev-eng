/** UIPANEL_Surface.h — canonical offscreen-surface API.
 *
 * UIPANEL_InitSurface — Address: 0x42A850
 * The original routine initializes/reinitializes an offscreen surface.
 * Keep this declaration C++-linked: mismatched local declarations create
 * distinct C++ symbols and can otherwise resolve to a null PLT entry.
 */
#pragma once

#include "../shared/types.h"

uint32_t UIPANEL_InitSurface(void* surface, int width, int height,
                             int mode, uint32_t palette_param,
                             uint8_t fill_byte);
