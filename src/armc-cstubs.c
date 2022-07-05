/* For more information about this file, please visit:
   https://sourceware.org/newlib/libc.html#Stubs

   These are the newlib C-Library stubs for the valvers Raspberry-Pi bare-metal
   tutorial */

/*
	Graceful failure is permitted by returning an error code. A minor
	complication arises here: the C library must be compatible with development
	environments that supply fully functional versions of these subroutines.
	Such environments usually return error codes in a global errno. However,
	the Red Hat newlib C library provides a macro definition for errno in the
	header file errno.h, as part of its support for reentrant routines (see
	Reentrancy).

	The bridge between these two interpretations of errno is straightforward:
	the C library routines with OS interface calls capture the errno values
	returned globally, and record them in the appropriate field of the
	reentrancy structure (so that you can query them using the errno macro from
	errno.h).

	This mechanism becomes visible when you write stub routines for OS
	interfaces. You must include errno.h, then disable the macro, like this:
*/
#include <errno.h>
#undef errno
extern int errno;

/* Required include for fstat() */
#include <sys/stat.h>

/* Required include for times() */
#include <sys/times.h>

/* Prototype for the Term write function */
#include "rpi-term.h"

#include "rpi-aux.h"
#include "rpi-log.h"
#include "rpi-input.h"
#include <stdio.h>

void outbyte(char c);

static const char fromCStubs[] = "cstubs";

/* A pointer to a list of environment variables and their values. For a minimal
   environment, this empty list is adequate: */
char* __env[1] = { 0 };
char** environ = __env;

/* A helper function written in assembler to aid us in allocating memory */
extern caddr_t _get_stack_pointer(void);


/* Never return from _exit as there's no OS to exit to, so instead we trap
   here */
void _exit(int status) {
	RPI_Log(fromCStubs, LOG_ERROR, "_exit(%i)", status);

	while(1) {
		/* TRAP HERE */
	}
}


/* There's currently no implementation of a file system because there's no
   file system! */
int _close(int file) {
	RPI_Log(fromCStubs, LOG_WARNING, "_close(%i)", file);

	return -1;
}


/* Transfer control to a new process. Minimal implementation (for a system
   without processes): */
int execve(char* name, char** argv, char** env) {
	RPI_Log(fromCStubs, LOG_WARNING, "execve(%s, %s, %s)", name, *argv, *env);

	errno = ENOMEM;
	return -1;
}


/* Create a new process. Minimal implementation (for a system without
   processes): */
int fork(void) {
	RPI_Log(fromCStubs, LOG_WARNING, "fork()");

	errno = EAGAIN;
	return -1;
}


/* Status of an open file. For consistency with other minimal implementations
   in these examples, all files are regarded as character special devices. The
   sys/stat.h header file required is distributed in the include subdirectory
   for this C library. */
int _fstat(int file, struct stat* st) {
	RPI_Log(fromCStubs, LOG_WARNING, "_fstat(%i, %X)", file, st);

	st->st_mode = S_IFCHR;
	return 0;
}


/* Process-ID; this is sometimes used to generate strings unlikely to conflict
   with other processes. Minimal implementation, for a system without
   processes: */
int _getpid(void) {
	RPI_Log(fromCStubs, LOG_WARNING, "_getpid()");

	return 1;
}


/* Query whether output stream is a terminal. For consistency with the other
   minimal implementations, which only support output to stdout, this minimal
   implementation is suggested: */
int _isatty(int file) {
	RPI_Log(fromCStubs, LOG_WARNING, "_isatty(%i)", file);

	return 1;
}


/* Send a signal. Minimal implementation: */
int _kill(int pid, int sig) {
	RPI_Log(fromCStubs, LOG_WARNING, "_kill(%i, %i)", pid, sig);

	errno = EINVAL;
	return -1;
}


/* Establish a new name for an existing file. Minimal implementation: */
int link(char* old, char* new) {
	RPI_Log(fromCStubs, LOG_WARNING, "link(%s, %s)", old, new);

	errno = EMLINK;
	return -1;
}


/* Set position in a file. Minimal implementation: */
int _lseek(int file, int ptr, int dir) {
	RPI_Log(fromCStubs, LOG_WARNING, "_lseek(%i, %i, %i)", file, ptr, dir);

	return 0; // implies the file is empty (∴, i assume this should return the current offset into the file?)
}


/* Open a file. Minimal implementation: */
int open(const char* name, int flags, int mode) {
	RPI_Log(fromCStubs, LOG_WARNING, "open(%s, %i, %i)", name, flags, mode);

	return -1;
}


/* Read from a file. Attempts to read up to length bytes from the file into the buffer
 returns: read space left for getc()
 return: -1 for no charaters, >0 for how many characters were read
 returning 0 breaks something (i cant figure out what)
*/

int _read(int file, char* buffer, int length) {
	//RPI_Log(fromCStubs, LOG_WARNING, "_read(%i, %X, %i)", file, buffer, length);

	return RPI_InputGetChars(buffer, length);
}


/* Increase program data space. As malloc and related functions depend on this,
   it is useful to have a working implementation. The following suffices for a
   standalone system; it exploits the symbol _end automatically defined by the
   GNU linker. */
caddr_t _sbrk(int incr) {
	extern char _end;
	static char* heap_end = 0;
	char* prev_heap_end;

	if(heap_end == 0)
		heap_end = &_end;

	prev_heap_end = heap_end;
	heap_end += incr;

	return (caddr_t)prev_heap_end;
}


/* Status of a file (by name). Minimal implementation: */
int stat(const char* name, struct stat* st) {
	RPI_Log(fromCStubs, LOG_WARNING, "stat(%s, %X)", name, st);

	st->st_mode = S_IFCHR;
	return 0;
}


/* Timing information for current process. Minimal implementation: */
clock_t times(struct tms* buf) {
	RPI_Log(fromCStubs, LOG_WARNING, "times(%X)", buf);

	return -1;
}


/* Remove a file's directory entry. Minimal implementation: */
int unlink(char* name) {
	RPI_Log(fromCStubs, LOG_WARNING, "unlink(%s)", name);

	errno = ENOENT;
	return -1;
}


/* Wait for a child process. Minimal implementation: */
int wait(int* status) {
	RPI_Log(fromCStubs, LOG_WARNING, "wait(%i)", *status);

	errno = ECHILD;
	return -1;
}


void outbyte(char b) {
	// UART uses '\r\n', but our Term uses '\n'. all printf calls use just '\n', so add the carriage return for UART
	if(b == '\n') {
		RPI_AuxMiniUartWrite('\r');
	}
	RPI_AuxMiniUartWrite(b);

	RPI_TermPutC(b);
}

/* Write to a file. libc subroutines will use this system routine for output to
   all files, including stdout—so if you need to generate any output, for
   example to a serial port for debugging, you should make your minimal write
   capable of doing this. The following minimal implementation is an
   incomplete example; it relies on a outbyte subroutine (not shown; typically,
   you must write this in assembler from examples provided by your hardware
   manufacturer) to actually perform the output. */
int _write(int file, char* ptr, int len) {
	int todo;

	for(todo = 0; todo < len; todo++)
		outbyte(*ptr++);

	return len;
}
