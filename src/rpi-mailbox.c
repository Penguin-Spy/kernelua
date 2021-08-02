/*
    Part of the Raspberry-Pi Bare Metal Tutorials
    https://www.valvers.com/rpi/bare-metal/
    Copyright (c) 2013-2018, Brian Sidebotham

    This software is licensed under the MIT License.
    Please see the LICENSE file included with this software.

*/

#include <stdint.h>
#include <stdio.h>

#include "rpi-gpio.h"
#include "rpi-mailbox.h"

/* Mailbox 0 mapped to it's base address */
static mailbox_t* rpiMailbox0 = (mailbox_t*)RPI_MAILBOX0_BASE;

void RPI_Mailbox0Write(mailbox0_channel_t channel, int value) {
   /* For information about accessing mailboxes, see:
      https://github.com/raspberrypi/firmware/wiki/Accessing-mailboxes */

      /* Add the channel number into the lower 4 bits */
   value &= ~(0xF);
   value |= channel;

   /* Wait until the mailbox becomes available and then write to the mailbox
      channel */
      //printf("\tRPI_Mailbox0Write waiting\n");
   while ((rpiMailbox0->Status & ARM_MS_FULL) != 0) {}
   //printf("\tRPI_Mailbox0Write ready\n");

   /* Write the modified value + channel number into the write register */
   rpiMailbox0->Write = value;
}


int RPI_Mailbox0Read(mailbox0_channel_t channel) {
   /* For information about accessing mailboxes, see:
      https://github.com/raspberrypi/firmware/wiki/Accessing-mailboxes */
   int value = -1;

   /* Keep reading the register until the desired channel gives us a value */
   //printf("\tRPI_Mailbox0Read reading channel\n");
   while ((value & 0xF) != channel)     {
      //printf("\tRPI_Mailbox0Read waiting for value\n");
      /* Wait while the mailbox is empty because otherwise there's no value
         to read! */
      while (rpiMailbox0->Status & ARM_MS_EMPTY) {}
      //printf("\tRPI_Mailbox0Read value read\n");

      /* Extract the value from the Read register of the mailbox. The value
         is actually in the upper 28 bits */
      value = rpiMailbox0->Read;
   }

   /* Return just the value (the upper 28-bits) */
   return value >> 4;
}
