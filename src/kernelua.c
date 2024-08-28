#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "gic-400.h"

#include "rpi-aux.h"
#include "rpi-armtimer.h"
#include "rpi-gpio.h"
#include "rpi-interrupts.h"
#include "rpi-mailbox-interface.h"
#include "rpi-systimer.h"

#include "rpi-term.h"
#include "rpi-power.h"
#include "rpi-memory.h"
#include "rpi-input.h"

#include "uspi.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "fs.h"
#include "rpi-log.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define SCREEN_DEPTH 32 /* Stick to 32-bit depth for ease-of tutorial code */

#define TIMER_HERTZ 100 /* Default hertz for libuspi (can be changed, but best to leave at default for now) */

const char* rotor = "\xC4\\\xB3/";


extern void _enable_interrupts(void);

void spinRotor(int i) {
    int x = RPI_TermGetCursorX();
    int y = RPI_TermGetCursorY();
    RPI_TermSetCursorPos(239, 0);
    RPI_TermPutC(rotor[i]);
    RPI_TermSetCursorPos(x, y);
}

void keyPressed(const char* string) {
    RPI_InputAddChar(string[0]);  // TODO: swap to raw key handler
}

void shutdown() {
    RPI_TermSetTextColor(COLORS_ORANGE);
    printf("ctrl+alt+del triggered reboot in ");
    for(int i = 3; i > 0; i--) {
        printf("%d ", i);
        RPI_WaitSeconds(1);
    }
    RPI_PowerReset();
}

static void keyPressedRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]) {
    printf("%X, %X, %X, %X, %X, %X\n", RawKeys[0], RawKeys[1], RawKeys[2], RawKeys[3], RawKeys[4], RawKeys[5]);
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

    /* Calculate the timer reload register value so we achieve an interrupt rate of TIMER_HERTZ (100) */
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

    if((mp = RPI_PropertyGet(TAG_ALLOCATE_BUFFER))) {
        fb = (volatile uint32_t*)(mp->data.buffer_32[0] & ~0xC0000000);
    }

    if((mp = RPI_PropertyGet(TAG_GET_PHYSICAL_SIZE))) {
        width = mp->data.buffer_32[0];
        height = mp->data.buffer_32[1];
    }

    RPI_TermInit(fb, width, height);

    if(fb[0] != 0x000000) {
        RPI_TermSetTextColor(COLORS_BLACK);
        RPI_TermSetBackgroundColor(COLORS_RED);
        printf("!!CAUGHT SOFT RESET!!");
        return; // interrupts still happen, this doesn't properly halt
    }

    printf("Initialised Framebuffer: %dx%d ", width, height);

    if((mp = RPI_PropertyGet(TAG_GET_DEPTH))) {
        int bpp = mp->data.buffer_32[0];
        printf("%dbpp\n", bpp);
        if(bpp != 32) {
            printf("THIS TUTORIAL ONLY SUPPORTS DEPTH OF 32bpp!\n");
        }
    }

    if((mp = RPI_PropertyGet(TAG_GET_PITCH))) {
        pitch_bytes = mp->data.buffer_32[0];
        printf("Pitch: %d bytes\n", pitch_bytes);
    }

    if((mp = RPI_PropertyGet(TAG_ALLOCATE_BUFFER))) {
        fb = (volatile uint32_t*)(mp->data.buffer_32[0] & ~0xC0000000);
        printf("Framebuffer address: %8.8X\n", (unsigned int)fb);
    }

    /* Print to the UART using the standard libc functions */
    printf("\n");
    printf("------------------------------------------\n");
    printf("Valvers.com ARM Bare Metal Tutorials\n");
    printf("Initialise UART console with standard libc\n");
    /*
        printf("PREDIV: 0x%8.8x\n", RPI_GetArmTimer()->PreDivider );
        printf("Timer Reload: 0x%8.8x\n", timer_load);
     */
    printf("CORE Frequency: %dMHz\n", (core_frequency / 1000000));

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

    if(mp = RPI_PropertyGet(TAG_GET_CLOCK_RATE)) {
        printf("ARM  Frequency: %dMHz\n", (mp->data.buffer_32[1] / 1000000));
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

    if(mp = RPI_PropertyGet(TAG_GET_BOARD_REVISION)) {
        uint32_t revision = mp->data.value_32;
        printf("Board Revision: 0x%8.8x", mp->data.value_32);
        if(revision & (1 << 23)) {
          /* New style revision code */
            printf(" rpi-%s", rpi_types[(revision & (0xFF << 4)) >> 4]);
            printf(" %s", processors[(revision & (0xF << 12)) >> 12]);
            printf(" %s", rpi_memories[(revision & (0x7 << 20)) >> 20]);
            printf(" %s", rpi_manufacturers[(revision & (0xF << 16)) >> 16]);
        } else {
          /* old style revision code */
            printf(" %s", rpi_models[revision]);
        }

        printf("\n");
    }

    if(mp = RPI_PropertyGet(TAG_GET_FIRMWARE_VERSION)) {
        printf("Firmware Version: %d\n", mp->data.value_32);
    }

    if(mp = RPI_PropertyGet(TAG_GET_BOARD_MAC_ADDRESS)) {
        printf("MAC Address: %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
            mp->data.buffer_8[0], mp->data.buffer_8[1], mp->data.buffer_8[2],
            mp->data.buffer_8[3], mp->data.buffer_8[4], mp->data.buffer_8[5]);
    }

    if(mp = RPI_PropertyGet(TAG_GET_BOARD_SERIAL)) {
        printf("Serial Number: %8.8X%8.8X\n", mp->data.buffer_32[0], mp->data.buffer_32[1]);
    }

    RPI_MemoryEnableMMU();

    printf("testing term putS and putHex:\n");
    RPI_TermPutS("test string!\n");
    RPI_TermPutHex(0x9abcdef0);
    RPI_TermPutS(" <- epic hex. now a number:\n");
    RPI_TermPutHex(1337);
    RPI_TermPutS(" that concludes our test.");
    printf("\nactually this does\n");

    int result;
    result = USPiInitialize();

    if(result == 0) { // indicates failure
        RPI_TermSetTextColor(COLORS_ORANGE);
        printf("USPiInitialize() result: %.d\n", result);
        goto shutdown;
    } else {
        RPI_TermSetTextColor(COLORS_LIME);
        printf("USPiInitialize() result: %.d\n", result);

        RPI_TermSetTextColor(COLORS_WHITE);
        if(USPiKeyboardAvailable()) {
            printf("Keyboard detected!\n");
            //USPiKeyboardRegisterKeyStatusHandlerRaw(keyPressedRaw);
            USPiKeyboardRegisterKeyPressedHandler(keyPressed);
            USPiKeyboardRegisterShutdownHandler(shutdown);
        } else {
            RPI_TermSetTextColor(COLORS_ORANGE);
            RPI_TermPrintAt(100, 0, "No keyboard or mass storage detected!");
            RPI_TermSetCursorPos(100, 1);
            printf("Plug in a device. RPi rebooting in ");
            goto shutdown;
        }
    }

    int input = 0;
    do {
        input = getchar();
    } while(input != '\n');

    RPI_TermSetTextColor(COLORS_WHITE);
    printf("\ninitializing sd card\n");

    result = fs_init();

    if(result == 0) {
        printf("fs init success!       \n");
    } else {
        RPI_TermSetTextColor(COLORS_ORANGE);
        printf("error init: %i         \n", result);
    }

    lua_State* L = luaL_newstate();
    //luaL_openlibs(L);
    const luaL_Reg* lib;

    static const luaL_Reg loadedlibs[] = {
      {"_G", luaopen_base},
      {LUA_LOADLIBNAME, luaopen_package},
      {LUA_COLIBNAME, luaopen_coroutine},
      {LUA_TABLIBNAME, luaopen_table},
      {LUA_IOLIBNAME, luaopen_io},
    //  {LUA_OSLIBNAME, luaopen_os},    // including this causes the kernel to fail to load (gpu displays rainbow square of death), thankfully we don't need it at all
      {LUA_STRLIBNAME, luaopen_string},
      {LUA_BITLIBNAME, luaopen_bit32},
      {LUA_MATHLIBNAME, luaopen_math},
      {LUA_DBLIBNAME, luaopen_debug},
      {NULL, NULL}
    };

    // call open functions from 'loadedlibs' and set results to global table
    for(lib = loadedlibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);  // remove lib
    }

    result = luaL_loadfile(L, "bios.lua");
    if(result != LUA_OK) {
        printf("loading bios.lua failed: %i\n", result);
        printf("\terror: %s\n", lua_tostring(L, -1));
    } else {
        printf("loading bios.lua returned LUA_OK\n");
        result = lua_pcall(L, 0, LUA_MULTRET, 0);
        if(result != LUA_OK) {
            printf("running bios.lua failed: %i\n", result);
            printf("\terror: %s\n", lua_tostring(L, -1));
        } else {
            printf("running bios.lua returned LUA_OK\n");
        }
    }

    int i = 0;
    while(1) {
        for(i = 0; i <= 3; i++) {
            USPiKeyboardUpdateLEDs();

            spinRotor(i);
            RPI_WaitMiliseconds(250);
        }
    }

shutdown:
    for(int i = 10; i > 0; i--) {
        printf("%d ", i);
        RPI_WaitSeconds(1);
    }
    RPI_PowerReset();
}
