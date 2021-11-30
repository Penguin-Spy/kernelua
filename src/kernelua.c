#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gic-400.h"

#include "rpi-aux.h"
#include "rpi-armtimer.h"
#include "rpi-gpio.h"
#include "rpi-interrupts.h"
#include "rpi-mailbox-interface.h"
#include "rpi-systimer.h"

#include "rpi-term.h"

#include "uspi.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define SCREEN_DEPTH 32 /* Stick to 32-bit depth for ease-of tutorial code */

#define TIMER_HERTZ 100 /* Default hertz for libuspi (can be changed, but best to leave at default for now) */

const char* rotor = "\xC4\\\xB3/";

extern void _enable_interrupts(void);

void keyPressed(const char* string) {
    printf(string);
}

void shutdown() {
    printf("yeah not yet srry");
    while (1) {}
}


/** Main function - we'll never return from here */
void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags) {
    volatile uint32_t* fb = NULL;
    int width = 0, height = 0;
    int pitch_bytes = 0;
    int pixel_offset;
    unsigned int frame_count = 0;
    rpi_mailbox_property_t* mp;
    uint32_t pixel_value = 0;

    /* Write 1 to the LED init nibble in the Function Select GPIO peripheral register to enable
       LED pin as an output */
    RPI_SetGpioPinFunction(LED_GPIO, FS_OUTPUT);
    LED_ON();

    /* Using some print statements with no newline causes the output to be buffered and therefore
       output stagnates, so disable buffering on the stdout FILE */
    setbuf(stdout, NULL);

    /* Use the GPU Mailbox to dynamically retrieve the CORE Clock Frequency. This is also what the
       datasheet refers to as the APB (Advanced Peripheral Bus) clock which drives the ARM Timer
       peripheral */
    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_GET_CLOCK_RATE, TAG_CLOCK_CORE);
    RPI_PropertyProcess();
    mp = RPI_PropertyGet(TAG_GET_CLOCK_RATE);
    uint32_t core_frequency = mp->data.buffer_32[1];

    /* Calculate the timer reload register value so we achieve an interrupt rate of 2Hz. Every
       second interrupt will therefore be one second. It's approximate, the division doesn't
       really work out to be precisely 1s because of the divisor options and the core
       frequency. */
    uint16_t prescales[] = { 1, 16, 256, 1 };
    uint32_t timer_load = (1.0 / TIMER_HERTZ) / (1.0 / (core_frequency / (RPI_GetArmTimer()->PreDivider + 1) * (prescales[(RPI_GetArmTimer()->Control & 0xC) >> 2])));
    RPI_GetArmTimer()->Load = timer_load;

    /* Setup the ARM Timer */
    RPI_GetArmTimer()->Control = (RPI_ARMTIMER_CTRL_23BIT |
        RPI_ARMTIMER_CTRL_ENABLE | RPI_ARMTIMER_CTRL_INT_ENABLE);

    /* Enable the ARM Interrupt controller in the BCM interrupt controller */
    RPI_EnableARMTimerInterrupt();

    /* Globally enable interrupts */
    _enable_interrupts();

    /* Initialise the UART */
    RPI_AuxMiniUartInit(115200, 8);

    /* Initialise a framebuffer using the property mailbox interface */
    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_ALLOCATE_BUFFER);
    RPI_PropertyAddTag(TAG_SET_PHYSICAL_SIZE, SCREEN_WIDTH, SCREEN_HEIGHT);
    RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE, SCREEN_WIDTH, SCREEN_HEIGHT * 2);
    RPI_PropertyAddTag(TAG_SET_DEPTH, SCREEN_DEPTH);
    RPI_PropertyAddTag(TAG_GET_PITCH);
    RPI_PropertyAddTag(TAG_GET_PHYSICAL_SIZE);
    RPI_PropertyAddTag(TAG_GET_DEPTH);
    RPI_PropertyProcess();

    /* Produce a colour spread across the screen */
    /*for (int y = 0; y < height; y++) {
        int line_offset = y * width;

        for (int x = 0; x < width; x++) {
            fb[line_offset + x] = pixel_value++;
        }
    }*/

    if ((mp = RPI_PropertyGet(TAG_ALLOCATE_BUFFER))) {
        fb = (volatile uint32_t*)(mp->data.buffer_32[0] & ~0xC0000000);
    }

    if ((mp = RPI_PropertyGet(TAG_GET_PHYSICAL_SIZE))) {
        width = mp->data.buffer_32[0];
        height = mp->data.buffer_32[1];
    }

    RPI_TermInit(fb, width, height);

    if (fb[0] != 0x000000) {
        RPI_TermSetTextColor(COLORS_RED);
        printf("!!CAUGHT SOFT RESET!!");
        return; // interrupts still happen, this doesn't properly halt
    }

    printf("Initialised Framebuffer: %dx%d ", width, height);

    if ((mp = RPI_PropertyGet(TAG_GET_DEPTH))) {
        int bpp = mp->data.buffer_32[0];
        printf("%dbpp\r\n", bpp);
        if (bpp != 32) {
            printf("THIS TUTORIAL ONLY SUPPORTS DEPTH OF 32bpp!\r\n");
        }
    }

    if ((mp = RPI_PropertyGet(TAG_GET_PITCH))) {
        pitch_bytes = mp->data.buffer_32[0];
        printf("Pitch: %d bytes\r\n", pitch_bytes);
    }

    if ((mp = RPI_PropertyGet(TAG_ALLOCATE_BUFFER))) {
        fb = (volatile uint32_t*)(mp->data.buffer_32[0] & ~0xC0000000);
        printf("Framebuffer address: %8.8X\r\n", (unsigned int)fb);
    }

    /* Print to the UART using the standard libc functions */
    printf("\r\n");
    printf("------------------------------------------\r\n");
    printf("Valvers.com ARM Bare Metal Tutorials\r\n");
    printf("Initialise UART console with standard libc\r\n");
    /*
        printf("PREDIV: 0x%8.8x\r\n", RPI_GetArmTimer()->PreDivider );
        printf("Timer Reload: 0x%8.8x\r\n", timer_load);
     */
    printf("CORE Frequency: %dMHz\r\n", (core_frequency / 1000000));

    /* Clock Frequency */
    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_GET_MAX_CLOCK_RATE, TAG_CLOCK_ARM);
    RPI_PropertyProcess();

    mp = RPI_PropertyGet(TAG_GET_MAX_CLOCK_RATE);

    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_SET_CLOCK_RATE, TAG_CLOCK_ARM, mp->data.buffer_32[1]);
    RPI_PropertyProcess();

    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_GET_CLOCK_RATE, TAG_CLOCK_ARM);
    RPI_PropertyProcess();

    if (mp = RPI_PropertyGet(TAG_GET_CLOCK_RATE)) {
        printf("ARM  Frequency: %dMHz\r\n", (mp->data.buffer_32[1] / 1000000));
    }

    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_GET_BOARD_REVISION);
    RPI_PropertyAddTag(TAG_GET_FIRMWARE_VERSION);
    RPI_PropertyAddTag(TAG_GET_BOARD_MAC_ADDRESS);
    RPI_PropertyAddTag(TAG_GET_BOARD_SERIAL);
    RPI_PropertyProcess();

    const char* processors[] = { "BCM2835", "BCM2836", "BCM2837", "BCM2711" };

    const char* rpi_types[] = {
        "1A", "1B", "1A+", "1B+", "2B", "ALPHA", "CM1", "{7}", "3B", "Zero", "CM3", "{11}", "Zero W", "3B+",
        "3A+", "-", "CM3+", "4B" };

    const char* rpi_memories[] = {
        "256MB", "512MB", "1GiB", "2GiB", "4GiB", "8GiB" };

    const char* rpi_manufacturers[] = {
        "Sony UK", "Egoman", "Embest", "Sony Japan", "Embest", "Stadium" };

    const char* rpi_models[] = {
        "-",
        "-",
        "RPI1B 1.0 256MB Egoman",
        "RPI1B 1.0 256MB Egoman",
        "RPI1B 2.0 256MB Sony UK",
        "RPI1B 2.0 256MB Qisda",
        "RPI1B 2.0 256MB Egoman",
        "RPI1A 2.0 256MB Egoman",
        "RPI1A 2.0 256MB Sony UK",
        "RPI1A 2.0 256MB Qisda",
        "RPI1B 2.0 512MB Egoman",
        "RPI1B 2.0 512MB Sony UK",
        "RPI1B 2.0 512MB Egoman",
        "RPI1B+ 1.2 512MB Sony UK",
        "CM1 1.0 512MB Sony UK",
        "RPI1A+ 1.1 256MB Sony UK",
        "RPI1B+ 1.2 512MB Embest",
        "CM1 1.0 512MB Embest",
        "RPI1A+ 1.1 256MB/512MB Embest",
    };

    if (mp = RPI_PropertyGet(TAG_GET_BOARD_REVISION)) {
        uint32_t revision = mp->data.value_32;
        printf("Board Revision: 0x%8.8x", mp->data.value_32);
        if (revision & (1 << 23)) {
            /* New style revision code */
            printf(" rpi-%s", rpi_types[(revision & (0xFF << 4)) >> 4]);
            printf(" %s", processors[(revision & (0xF << 12)) >> 12]);
            printf(" %s", rpi_memories[(revision & (0x7 << 20)) >> 20]);
            printf(" %s", rpi_manufacturers[(revision & (0xF << 16)) >> 16]);
        } else {
            /* old style revision code */
            printf(" %s", rpi_models[revision]);
        }

        printf("\r\n");
    }

    if (mp = RPI_PropertyGet(TAG_GET_FIRMWARE_VERSION)) {
        printf("Firmware Version: %d\r\n", mp->data.value_32);
    }

    if (mp = RPI_PropertyGet(TAG_GET_BOARD_MAC_ADDRESS)) {
        printf("MAC Address: %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\r\n",
            mp->data.buffer_8[0], mp->data.buffer_8[1], mp->data.buffer_8[2],
            mp->data.buffer_8[3], mp->data.buffer_8[4], mp->data.buffer_8[5]);
    }

    if (mp = RPI_PropertyGet(TAG_GET_BOARD_SERIAL)) {
        printf("Serial Number: %8.8X%8.8X\r\n", mp->data.buffer_32[0], mp->data.buffer_32[1]);
    }


    /*RPI_TermSetBackgroundColor(COLORS_PINK);
    RPI_TermSetTextColor(COLORS_YELLOW);
    RPI_TermPutC('B');
    RPI_TermSetTextColor(COLORS_CYAN);
    RPI_TermPutC('r');
    RPI_TermSetTextColor(COLORS_RED);
    RPI_TermPutC('u');
    RPI_TermSetBackgroundColor(COLORS_BLACK);
    RPI_TermPutC('h');
    RPI_TermSetTextColor(COLORS_WHITE);
    RPI_TermPutC(' ');
    RPI_TermPutC('H');
    printf("\nHello world!\nthis is a test");

    RPI_TermSetCursorPos(16, 5);
    printf("This is being printed from ");
    RPI_TermSetTextColor(COLORS_GREEN);
    printf("printf");
    RPI_TermSetTextColor(COLORS_WHITE);
    RPI_TermSetCursorPos(12, 6);
    printf("Which is a C standard library function");
    RPI_TermSetCursorPos(6, 7);
    printf("But it's running in a bare-metal environment (No Operating System)!");
    RPI_TermSetTextColor(COLORS_BLUE);
    RPI_TermSetCursorPos(6, 8);
    printf("this was quite hard to get working please clap\n");*/



    //printf("Initialised Framebuffer: %dx%d\n\n", width, height);

    /*RPI_TermSetCursorPos(0, 13);
    RPI_TermSetTextColor(COLORS_WHITE);

    // Print out every single character from 0x01 to 0xFF (not 0x00, that would terminate the string before we print any of it :P
    // and 0x09 - 0x0a are omitted because they're tab & newline
    // 0x25 (%) is duplicated because it must be escaped for printf
    printf("\
 \x01\x02\x03\x04\x05\x06\x07\x08  \x0b\x0c\x0d\x0e\x0f\n\
\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\n\
 !\"#$%%&'()*+,-./\n\
0123456789:,<=>?\n\
@ABCDEFGHIJKLMNO\n\
PQRSTUVWXYZ[\\]^_\n\
`abcdefghijklmno\n\
pqrstuvwxyz{|}~\x7F\n\
\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\n\
\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\n\
\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\n\
\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\n\
\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\n\
\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\n\
\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\n\
\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff\n");

    RPI_TermSetCursorPos(1, 30);
    RPI_TermSetTextColor(COLORS_LIME);
    //printf("libuspi is included i think :thinking emoji but I can't display it because this is ASCII (actually KLSCII \x01)");
    printf("YOOO LETS GO I COMPILED ON WINDOWS 10 YEET");
*/
    int result;

    //RPI_TermSetCursorPos(0, 0);
    RPI_TermSetTextColor(COLORS_PURPLE);

    RPI_TermSetTextColor(COLORS_WHITE);
    result = USPiInitialize();

    RPI_TermSetTextColor(COLORS_PURPLE);
    //printf("What happens if I try to use unicode? 🤔 💾 📧\n\n");
    printf("\nUSPiInitialize() result: %.d\n", result);

    if (result != 0) {
        RPI_TermSetTextColor(COLORS_ORANGE);
    } else {
        RPI_TermSetTextColor(COLORS_LIME);
    }

    if (USPiKeyboardAvailable()) {
        printf("Keyboard detected!\n");
        USPiKeyboardRegisterKeyPressedHandler(keyPressed);
        USPiKeyboardRegisterShutdownHandler(shutdown);
        printf("try typing?\n");
        RPI_TermSetTextColor(COLORS_WHITE);

        while (1) {
            USPiKeyboardUpdateLEDs();
        }
    } else {
        printf("No keyboard detected!\nPlug in a keyboard and reboot the device.  ");

        int x = RPI_TermGetCursorX();
        int y = RPI_TermGetCursorY();
        int i = 0;
        while (1) {
            RPI_TermSetCursorPos(x, y);
            RPI_TermPutC(rotor[i++]);
            if (i > 3) {
                i = 0;
            }

            RPI_WaitMicroSeconds(250000);
        }
    }

    /*while(1) {
        RPI_WaitMicroSeconds(50000);
        LED_OFF();
        RPI_WaitMicroSeconds(5000);
        LED_ON();
    }*/

}