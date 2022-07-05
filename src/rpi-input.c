#include <stdio.h>
#include <string.h>

#include "rpi-input.h"
#include "rpi-term.h"

#define INPUT_BUFFER_SIZE 16

// set to 1 to enable the buffer display (debugging only, can cause timing issues bc it's slow)
#define BUFFER_DISPLAY 0

char inputBuffer[INPUT_BUFFER_SIZE + 1];
unsigned inputBufferIndex = 0;  // 0 = no chars

void RPI_InputAddChar(char c) {
	if(inputBufferIndex < INPUT_BUFFER_SIZE - 1) {
		inputBuffer[inputBufferIndex++] = c;
	}

#if BUFFER_DISPLAY == 1
	RPI_TermPrintAt(100, 1, "I: %i", inputBufferIndex);
	for(int i = 0; i < INPUT_BUFFER_SIZE; i++) {
		RPI_TermPrintAt(100 + i, 0, "%c", inputBuffer[i]);
	}
#endif
}

// DON'T COPY+PASTE, THIS WILL DOUBLE-EVALUATE!!!!
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

// this assumes each character is 1 byte. i fear the day i decide to implement unicode.
// copies characters into buffer, returns # of characters copied or EOF (-1) if no characters available
int RPI_InputGetChars(char* buffer, int maxChars) {
	if(inputBufferIndex > 0) {
		int copyLength = min(maxChars, inputBufferIndex);

		memcpy(buffer, inputBuffer, copyLength);
		inputBufferIndex -= copyLength;

#if BUFFER_DISPLAY == 1
		RPI_TermPrintAt(100, 1, "I: %i", inputBufferIndex);
		RPI_TermPrintAt(100, 0, "                ");
		memcpy(inputBuffer, "                ", 16);
#endif

		return copyLength;
	} else {
		return EOF;
	}
}
