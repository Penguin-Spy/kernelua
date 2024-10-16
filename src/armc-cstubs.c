/*
    https://sourceware.org/newlib/libc.html#Stubs

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

// for fstat() & times()
#include <sys/stat.h>
#include <sys/times.h>
// for file open mode flags
#include <stdio.h>
#include <fcntl.h>

#include "rpi-term.h"
#include "rpi-aux.h"
#include "log.h"
#include "rpi-input.h"

#include "fs.h"


// shift file handles up 3 to make space for stdin, stdout, stderr (0, 1, 2 respectively)
#define FILE_HANDLE_START 3

static const char log_from[] = "cstubs";


// --- General syscalls --- //

// A pointer to a list of environment variables and their values. For a minimal
// environment, this empty list is adequate:
// TODO: is this even necessary?
//char* __env[1] = { 0 };
//char** environ = __env;

/* Increase program data space. As malloc and related functions depend on this,
 * it is useful to have a working implementation. The following suffices for a
 * standalone system; it exploits the symbol _end automatically defined by the
 * GNU linker. */
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

/** If exit is called, it is likely due to a Lua panic or other issue.
 * Display an error message and halt. */
void _exit(int status) {
    log_error("exit(%i)", status);

    while(1) { }
}

/** Unknown use, something to do with un-initalizing arrays when a program is exiting. */
/*void _fini() {
    log_error("fini()");
}*/


// --- File handle syscalls --- //

/** Open a file.
 * @param name the full file path
 * @param mode the mode the file was opened with (parsed from `r`|`w`|`a`, `+`)
 * @param permission    unknown purpose, but appears to be related to the owner, group, other permission bits
 * @returns a file handle, or `-1` on error and sets `errno`.
 */
int _open(const char* name, int mode, int permission) {
    log_warn("open(%s, %.5X, %.4o)", name, mode, permission);
    /*
      mode contains the file opening mode:
        O_RDONLY            if the open mode is 'r'         and no '+'  ✔
        O_WRONLY            if the open mode is 'w' or 'a'  and no '+'  ✔
        O_CREAT             if the open mode is 'w' or 'a'
        O_TRUNC             if the open mode is 'w'
        O_APPEND            if the open mode is 'a'                     ✔
        O_RDWR              if the open mode has a '+'                  ✔
        O_BINARY            if the open mode has a 'b'

      permission contains the requested file opening permissions if the file is created
      newlib has previously called open() with the mode of 438, which corresponds to -rw-rw-rw (666 in octal)
    */

    int file = fs_open(name, mode, 1);
    if(file == -1) return -1;
    return file + FILE_HANDLE_START;
}

/** Closes an open file.
 * @param file the file handle
 */
int _close(int file) {
    log_warn("close(%i)", file);
    return fs_close(file - FILE_HANDLE_START);
}

/** Read from a file. Attempts to read up to length bytes from the file into the buffer.
 * @param file the file handle. `0` for stdin, `1` for stdout, `2` for stderr, or a file handle obtained from `_open`.
 * @returns the number of bytes read, `0` for end of file, or `-1` on error and sets `errno`.
 */
int _read(int file, char* buffer, int length) {
    if(file >= FILE_HANDLE_START) {
        log_warn("read(%i, %X, %i)", file, buffer, length);
        int x = RPI_TermGetCursorX(), y = RPI_TermGetCursorY();
        RPI_TermSetCursorPos(228, 2);
        RPI_TermPutS("R           ");
        RPI_TermSetCursorPos(230, 2);
        RPI_TermPutHex(file);
        RPI_TermSetCursorPos(x, y);
        int status = fs_read(file - FILE_HANDLE_START, buffer, length);
        log_warn("read: %i", status);
        return status;
    } else {
        return RPI_InputGetChars(buffer, length);
    }
}

/** Write to a file.
 * @param file the file handle. `0` for stdin, `1` for stdout, `2` for stderr, or a file handle obtained from `_open`.
 * @param buffer the data to write
 * @param length the number of bytes to write
 * @returns the number of bytes written. newlib will repeatedly call this until
 * all bytes are written (returns `0`) (i think).
 */
int _write(int file, char* buffer, int length) {
    if(file >= FILE_HANDLE_START) {
        log_warn("write(%i, %X, %i)", file, buffer, length);
        int x = RPI_TermGetCursorX(), y = RPI_TermGetCursorY();
        RPI_TermSetCursorPos(228, 1);
        RPI_TermPutS("W           ");
        RPI_TermSetCursorPos(230, 1);
        RPI_TermPutHex(file);
        RPI_TermSetCursorPos(x, y);
        int status = fs_write(file - FILE_HANDLE_START, buffer, length);
        log_warn("write: %i", status);
        return status;
    } else {
        for(int todo = 0; todo < length; todo++) {
            char b = *buffer++;
            // UART uses '\r\n', but our Term uses '\n'. all printf calls use just '\n', so add the carriage return for UART
            if(b == '\n') {
                RPI_AuxMiniUartWrite('\r');
            }
            RPI_AuxMiniUartWrite(b);

            RPI_TermPutC(b);
        }
    }
    return length;
}

/** Set position in a file.
 * @returns the resulting offset location in bytes, or `-1` on error and sets `errno`.
 */
int _lseek(int file, int offset, int whence) {
    log_warn("lseek(%i, %i, %i)", file, offset, whence);

    if(file < FILE_HANDLE_START) {
        errno = EBADF;
        return -1;
    } else {
        return fs_seek(file - FILE_HANDLE_START, offset, whence);
    }
}

/** Status of an open file.
 * @returns `0` on success, or `-1` on error and sets `errno`.
 */
int _fstat(int file, struct stat* stat) {
    log_warn("fstat(%i, %X)", file, stat);

    if(file < FILE_HANDLE_START) {
        stat->st_mode = S_IFCHR;
    } else if(!fs_is_valid_file(file - FILE_HANDLE_START)) {
        errno = EBADF;
        return -1;
    } else {
        stat->st_mode = S_IFREG;
        stat->st_size = 3;  // TODO: get the actual size
    }
    return 0;
}

/** Query whether an open file is a terminal (output stream).
 * @returns `1` if the file is a terminal, or `0` and sets errno to `ENOTTY` or `EBADF`.
 */
int _isatty(int file) {
    log_warn("isatty(%i)", file);

    if(file < FILE_HANDLE_START) {
        return 1;
    } else if(fs_is_valid_file(file - FILE_HANDLE_START)) {
        errno = ENOTTY;
    } else {
        errno = EBADF;
    }
    return 0;
}


// --- Filesystem syscalls --- //

/** Establish a new name for an existing file.
 *
 */
/*int _link(char* old, char* new) {
    log_warn("link(%s, %s)", old, new);

    errno = EMLINK;
    return -1;
}*/

/** Remove a file's directory entry.
 *
 */
int _unlink(char* name) {
    log_warn("unlink(%s)", name);

    errno = ENOENT;
    return -1;
}

/** Status of a file (by name).
 *
 */
/*int _stat(const char* name, struct stat* st) {
    log_warn("stat(%s, %X)", name, st);

    st->st_mode = S_IFCHR;
    return 0;
}*/


// --- System syscalls --- //

/** Process ID; this is sometimes used to generate strings unlikely to conflict
 * with other processes.
 */
int _getpid(void) {
    log_warn("getpid()");

    return 1;   // we don't have processes, so always return 1.
}

/** Send a signal.
 * @param pid the target process's ID
 * @param signal the signal to send
 */
int _kill(int pid, int signal) {
    log_warn("kill(%i, %i)", pid, signal);

    errno = EINVAL; // we don't have processes, so signals will always be invalid to send.
    return -1;
}

/** Timing information for current process.
 *
 */
/*clock_t _times(struct tms* buf) {
    log_warn("times(%X)", buf);

    return -1;
}*/

// required by something in Lua. (notably not the OS library)
int _gettimeofday (struct timeval *tp, void* tzvp) {
    struct timezone *tzp = tzvp;
    log_warn("gettimeofday(%X, %X)", tp, tzp);

    return -1;
}

/* Transfer control to a new process. Minimal implementation (for a system
   without processes): */
/*int execve(char* name, char** argv, char** env) {
    log_warn("execve(%s, %s, %s)", name, *argv, *env);

    errno = ENOMEM;
    return -1;
}*/

/* Create a new process. Minimal implementation (for a system without
   processes): */
/*int fork(void) {
    log_warn("fork()");

    errno = EAGAIN;
    return -1;
}*/

// Wait for a child process. Minimal implementation:
/*int wait(int* status) {
    log_warn("wait(%i)", *status);

    errno = ECHILD;
    return -1;
}*/
