#ifndef RPI_TERM_H
#define RPI_TERM_H

#define FONT_WIDTH 8
#define FONT_HEIGHT 8

#define COLORS_WHITE        0xF0F0F0
#define COLORS_ORANGE       0xF2B233
#define COLORS_MAGENTA      0xE57FD8
#define COLORS_LIGHTBLUE    0x99B2F2
#define COLORS_YELLOW       0xDEDE6C
#define COLORS_LIME         0x7FCC19
#define COLORS_PINK         0xF2B2CC
#define COLORS_GRAY         0x4C4C4C
#define COLORS_LIGHTGRAY    0x999999
#define COLORS_CYAN         0x4C99B2
#define COLORS_PURPLE       0xB266E5
#define COLORS_BLUE         0x3366CC
#define COLORS_BROWN        0x7F664C
#define COLORS_GREEN        0x57A64E
#define COLORS_RED          0xCC4C4C
#define COLORS_BLACK        0x191919

#define COLORS_PUREBLACK    0x000000
#define COLORS_PUREWHITE    0xFFFFFF

#define ERROR_NOTREADY      1
#define ERROR_OOB_X         2
#define ERROR_OOB_Y         3

void RPI_TermInit(volatile uint32_t* in_fb, int width, int height);

int RPI_TermSetCursorPos(int x, int y);
int RPI_TermGetCursorX();   // C can only return 1 variable
int RPI_TermGetCursorY();

void RPI_TermSetTextColor(int color);
int RPI_TermGetTextColor();

void RPI_TermSetBackgroundColor(int color);
int RPI_TermGetBackgroundColor();

int RPI_TermPutC(char glyph);

void RPI_TermPrintAt(int x, int y, const char* string, ...);
void RPI_TermPrintDyed(int textColor, int backgroundColor, const char* string, ...);
void RPI_TermPrintAtDyed(int x, int y, int textColor, int backgroundColor, const char* string, ...);
void RPI_TermPrintRegister(uint64_t reg);

#endif
