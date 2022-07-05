#include <stdio.h>

#include "rpi-log.h"
#include "rpi-term.h"

void RPI_Log(const char* source, unsigned level, const char* message, ...) {
	va_list vl;
	va_start(vl, message);
	RPI_vLog(source, level, message, vl);
}

void RPI_vLog(const char* source, unsigned level, const char* message, va_list vl) {
	int old_fg = RPI_TermGetTextColor();
	int old_bg = RPI_TermGetBackgroundColor();
	RPI_TermSetBackgroundColor(COLORS_BLACK);

	switch(level) {
		case LOG_ERROR:
			RPI_TermSetTextColor(COLORS_RED);
			break;
		case LOG_WARNING:
			RPI_TermSetTextColor(COLORS_ORANGE);
			break;
		case LOG_DEBUG:
			RPI_TermSetTextColor(COLORS_PURPLE);
			break;
		case LOG_KERNEL:
			RPI_TermSetTextColor(COLORS_PINK);
			break;
		case LOG_MMU:
			RPI_TermSetTextColor(COLORS_CYAN);
			break;
		case LOG_NOTICE:
		default:            // default to white if unknown log level
			RPI_TermSetTextColor(COLORS_WHITE);
			break;
	}
	printf("[%s]: ", source);
	vprintf(message, vl);
	printf("\n");

	RPI_TermSetTextColor(old_fg);
	RPI_TermSetBackgroundColor(old_bg);
}

void RPI_LogDump(const char* source, const int* buffer, unsigned length) {
	RPI_LogDumpColumns(source, buffer, length, 0);
}

void RPI_LogDumpColumns(const char* source, const int* buffer, unsigned length, unsigned columns) {
	int old_fg = RPI_TermGetTextColor();
	int old_bg = RPI_TermGetBackgroundColor();
	RPI_TermSetBackgroundColor(COLORS_BLACK);
	RPI_TermSetTextColor(COLORS_LIGHTBLUE);
	printf("[%s]: Dumping %u bytes at 0x%0X:\n", source, length, buffer);

	while(length-- > 0) {
		printf("%02X ", *buffer++);
		if(columns != 0 && length % columns == 0) printf("\n");
	}

	printf("\n");
	RPI_TermSetTextColor(old_fg);
	RPI_TermSetBackgroundColor(old_bg);
}
