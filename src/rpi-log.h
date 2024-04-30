#ifndef RPI_LOG_H
#define RPI_LOG_H

#include <stdarg.h>

// from uspi
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_NOTICE	3
#define LOG_DEBUG	4

// kernelua-specific
#define LOG_KERNEL 5
#define LOG_MMU 6


void RPI_Log(const char* source, // who's logging this message
	unsigned    level,    // use LOG_x defines
	const char* message,  // message, uses printf formatting
	...); // variargs

void RPI_vLog(const char* source, // who's logging this message
	unsigned    level,    // use LOG_x defines
	const char* message,  // message, uses printf formatting
	va_list vl); // variarg list

void RPI_LogDump(const char* source,  // who's logging this message
	const uint8_t* buffer,  // buffer to dump
	unsigned length);   // number of bytes to dump

void RPI_LogDumpColumns(const char* source,  // who's logging this message
	const uint8_t* buffer,  // buffer to dump
	unsigned length,    // number of bytes to dump
	unsigned columns);  // insert newline every nth byte

#endif
