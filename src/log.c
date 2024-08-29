/* log.c © Penguin_Spy 2023-2024
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
 *
 * The Covered Software may not be used as training or other input data
 * for LLMs, generative AI, or other forms of machine learning or neural
 * networks.
 */

#include <stdio.h>

#include "log.h"
#include "rpi-term.h"

void log_write(const char* source, unsigned level, const char* message, ...) {
	va_list vl;
	va_start(vl, message);
	log_write_variadic(source, level, message, vl);
}

void log_write_variadic(const char* source, unsigned level, const char* message, va_list vl) {
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
	printf("    \n");

	RPI_TermSetTextColor(old_fg);
	RPI_TermSetBackgroundColor(old_bg);
}

void log_dump(const char* source, const uint8_t* buffer, unsigned length) {
	log_dump_columns(source, buffer, length, 0);
}

void log_dump_columns(const char* source, const uint8_t* buffer, unsigned length, unsigned columns) {
	int old_fg = RPI_TermGetTextColor();
	int old_bg = RPI_TermGetBackgroundColor();
	RPI_TermSetBackgroundColor(COLORS_BLACK);
	RPI_TermSetTextColor(COLORS_LIGHTBLUE);
	printf("[%s]: Dumping %u bytes at 0x%0X:\n", source, length, buffer);

	while(length-- > 0) {
		printf("%02X ", (uint8_t)*buffer++);
		if(columns != 0 && length % columns == 0) printf("\n");
	}

	printf("\n");
	RPI_TermSetTextColor(old_fg);
	RPI_TermSetBackgroundColor(old_bg);
}
