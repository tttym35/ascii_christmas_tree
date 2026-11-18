// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Foil-gold "MERRY CHRISTMAS MOSAICO" drawn straight into the canvas: bold
 * 6 x 10 capitals at 2x in solid gold with a 1 px fir-green outline, and a
 * glint that sweeps across every few seconds. Pure function of position and
 * time; no state. */
enum { GREETING_Y = 440, GREETING_H = 20 };

/** If (x, y) is part of the greeting, write its RGB565 colour and return
 * true. `time` is the scene time in seconds (drives the glint). */
bool tree_greeting_pixel(int x, int y, float time, uint16_t *out);
