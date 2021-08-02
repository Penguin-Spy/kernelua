#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rpi-term.h"
#include "font.h"

uint8_t fb_ready = 0;
static volatile uint32_t* fb;
static int fb_width;
static int fb_height;

static int cursor_x;
static int cursor_y;

uint32_t foreground_color, background_color;

// this is probably horrible C code, but my OOP brain can't figure out how else to do this :/
void RPI_TermInit(volatile uint32_t* in_fb, int width, int height) {
    fb = in_fb;
    fb_width = width;
    fb_height = height;

    RPI_TermSetCursorPos(0, 0);
    RPI_TermSetTextColor(COLORS_WHITE);
    RPI_TermSetBackgroundColor(COLORS_BLACK);

    fb_ready = 1;
}

int RPI_TermSetCursorPos(int x, int y) {
    if (x >= 0 && x < fb_width) {
        cursor_x = x;
    } else {
        return -1;
    }
    if (x >= 0 && y < fb_height) {
        cursor_y = y;
    } else {
        return -1;
    }
}

// C can only return 1 variable
int RPI_TermGetCursorX() {
    return cursor_x;
}
int RPI_TermGetCursorY() {
    return cursor_y;
}


void RPI_TermSetTextColor(int color) {
    foreground_color = color;
}
int RPI_TermGetTextColor() {
    return foreground_color;
}

void RPI_TermSetBackgroundColor(int color) {
    background_color = color;
}
int RPI_TermGetBackgroundColor() {
    return background_color;
}

int RPI_TermPutC(char glyph) {
    if (!fb_ready) { // Terminal has not been initalized, printing could(will?) cause a null pointer dereference
        return -1;
    }

    // Handle control characters
    switch (glyph) {
        case '\n':  // Move cursor to start of next line
            cursor_x = 0;
            cursor_y++;
            break;

        case '\t':  // Align cursor to next 4-character boundary
            cursor_x = (cursor_x / 4 + 1) * 4;
            break;

        default: {  // Print Glyph, move cursor to right & wrap if neccesary
            int glyph_offset = cursor_y * FONT_HEIGHT * fb_width + cursor_x * FONT_WIDTH;   // Calc offset for whole character

            for (int y = 0; y < FONT_HEIGHT; y++) { // Loop through every pixel of the char and put on screen
                for (int x = 0; x < FONT_WIDTH; x++) {
                    fb[glyph_offset + y * fb_width + x] = (font[glyph][y] & (1 << x)) ? foreground_color : background_color;
                }
            }

            cursor_x++;
            if (cursor_x * FONT_WIDTH >= fb_width) {
                cursor_x = 0;
                cursor_y++;
            }
            if (cursor_y * FONT_HEIGHT >= fb_height) {
                cursor_y = 0;	// good enough for now, should scroll screen but :how:, would need buffer for storing printed chars probably
                // or literally move entire framebuffer up? only usable in text-only mode
            }
            break;
        }
    }
}
