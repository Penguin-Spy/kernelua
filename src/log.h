/* log.h © Penguin_Spy 2023-2024
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
#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

// from uspi
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_NOTICE	3
#define LOG_DEBUG	4

// kernelua-specific
#define LOG_KERNEL 5
#define LOG_MMU 6


void log_write(const char* source, // who's logging this message
	unsigned    level,    // use LOG_x defines
	const char* message,  // message, uses printf formatting
	...); // variargs

void log_write_variadic(const char* source, // who's logging this message
	unsigned    level,    // use LOG_x defines
	const char* message,  // message, uses printf formatting
	va_list vl); // variarg list

void log_dump(const char* source,  // who's logging this message
	const uint8_t* buffer,  // buffer to dump
	unsigned length);   // number of bytes to dump

void log_dump_columns(const char* source,  // who's logging this message
	const uint8_t* buffer,  // buffer to dump
	unsigned length,    // number of bytes to dump
	unsigned columns);  // insert newline every nth byte

// convenience macro, requires the using file to define a static const `log_from` string
#define log(level, ...) log_write(log_from, level, __VA_ARGS__)

#define log_error(...) log(LOG_ERROR, __VA_ARGS__)
#define log_warn(...) log(LOG_WARNING, __VA_ARGS__)
#define log_notice(...) log(LOG_NOTICE, __VA_ARGS__)
#define log_debug(...) log(LOG_DEBUG, __VA_ARGS__)

#endif
