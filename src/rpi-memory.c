// enables paging and the MMU


/* flow of mmu setup:

get memory size from mailbox

page table must be at a 16Kib aligned address
  probably after the kernel, because the stack is before the kernel
  0x200000  - 2 MiB into memory, assumes kernel is never larger than 2 megabytes (which it probably won't reach, even with statically linking lualib and libvlc)
  4096 entries long because with 4096 values, we can map all of the PI's memory

create the "page table" - a big array of values describing that page's attributes
  attributes are:
    normal (read write execute)
    normal_XN (read write, no execute)
    device (shared device? maybe means memory mapped devices?)
  also on the RPI3:
    coherent: "strongly ordered"

  memory that the kerenel occupies must be "normal"
  memory that the kerenel doesn't occupy should be "normal_XN" to prevent ACE
  memory outside the "memory size" from the mailbox should be "device" - memory mapped devices

tell the MMU / system registers where this table is
do other stuff with the system registers to enable features and then the whole MMU

when malloc()ing, allocate using this page table/???
now we can free() pieces of memory

*/

#include <assert.h>

#include "rpi-memory.h"

#include "rpi-mailbox-interface.h"
#include "rpi-log.h"

extern int _etext; // end of executable section in the kernel

static const char fromMMU[] = "mmu";

//  Auxiliary Control register
#if RASPPI == 1
#define ARM_AUX_CONTROL_CACHE_SIZE	(1 << 6)	// restrict cache size to 16K (no page coloring)
#else
#define ARM_AUX_CONTROL_SMP		(1 << 6)
#endif


// TLB type register
#define ARM_TLB_TYPE_SEPARATE_TLBS	(1 << 0)

#define DOMAIN_NO_ACCESS	0
#define DOMAIN_CLIENT		1
#define DOMAIN_MANAGER		3

#define ARM_CONTROL_MMU			(1 << 0)
#define ARM_CONTROL_STRICT_ALIGNMENT	(1 << 1)
#define ARM_CONTROL_L1_CACHE		(1 << 2)
#define ARM_CONTROL_BRANCH_PREDICTION	(1 << 11)
#define ARM_CONTROL_L1_INSTRUCTION_CACHE (1 << 12)
#if RASPPI == 1
#define MMU_MODE	(  ARM_CONTROL_MMU			\
			 | ARM_CONTROL_L1_CACHE			\
			 | ARM_CONTROL_L1_INSTRUCTION_CACHE	\
			 | ARM_CONTROL_BRANCH_PREDICTION	\
			 | ARM_CONTROL_EXTENDED_PAGE_TABLE)
#else
#define MMU_MODE	(  ARM_CONTROL_MMU			\
			 | ARM_CONTROL_L1_CACHE			\
			 | ARM_CONTROL_L1_INSTRUCTION_CACHE	\
			 | ARM_CONTROL_BRANCH_PREDICTION)
#endif

#define TTBCR_SPLIT	0

int RPI_MemoryEnableMMU() {

  uint32_t* pageTable = (uint32_t*)0x200000;

  // get memory
  RPI_PropertyInit();
  RPI_PropertyAddTag(TAG_GET_ARM_MEMORY);
  RPI_PropertyProcess();
  rpi_mailbox_property_t* mp = RPI_PropertyGet(TAG_GET_ARM_MEMORY);

  RPI_Log(fromMMU, LOG_MMU, "ARM Memory: %8.8X%8.8X", mp->data.buffer_32[0], mp->data.buffer_32[1]);

  //base address in bytes = mp->data.buffer_32[0]
  //size in bytes         = mp->data.buffer_32[1]
  uint32_t memSize = mp->data.buffer_32[1];

  for(uint32_t entry = 0; entry < 4096; entry++) {
    uint32_t baseAddress = entry * 0x100000; // 1 MiB sections

    // coherent memory for mailbox???

    // set attributes & map 1:1 to physical memory
    if(baseAddress < memSize) {
      if(baseAddress < (uint32_t)&_etext) {
        pageTable[entry] = MEMORY_SECTION_NORMAL | baseAddress;
      } else {
        pageTable[entry] = MEMORY_SECTION_NORMAL_XN | baseAddress;
      }
    } else {
      pageTable[entry] = baseAddress | 0xC02; //MEMORY_SECTION_DEVICE | baseAddress;
    }
  }

  RPI_Log(fromMMU, LOG_MMU, "pageTable: 0x%0X, pageTable[0]: 0x%0X, &pageTable[0]: 0x%0X", pageTable, pageTable[0], &pageTable[0]);
  RPI_Log(fromMMU, LOG_MMU, "Page table initalized");

  //TODO: make this an extern asm routine in armc-start.S

  uint32_t nAuxControl;
  asm volatile ("mrc p15, 0, %0, c1, c0,  1" : "=r" (nAuxControl));
#if RASPPI == 1
  nAuxControl |= ARM_AUX_CONTROL_CACHE_SIZE;	// restrict cache size (no page coloring)
#else
  nAuxControl |= ARM_AUX_CONTROL_SMP;
#endif
  asm volatile ("mcr p15, 0, %0, c1, c0,  1" : : "r" (nAuxControl));

  RPI_Log(fromMMU, LOG_MMU, "Enabled aux control");

  uint32_t nTLBType;
  asm volatile ("mrc p15, 0, %0, c0, c0,  3" : "=r" (nTLBType));
  assert(!(nTLBType & ARM_TLB_TYPE_SEPARATE_TLBS));

  // set TTB control
  asm volatile ("mcr p15, 0, %0, c2, c0,  2" : : "r" (TTBCR_SPLIT));

  // set TTBR0
  asm volatile ("mcr p15, 0, %0, c2, c0,  0" : : "r" (pageTable));

  // set Domain Access Control register (Domain 0 and 1 to client)
  asm volatile ("mcr p15, 0, %0, c3, c0,  0" : : "r" (DOMAIN_CLIENT << 0));

  RPI_Log(fromMMU, LOG_MMU, "Enabled TLB");

  // enable MMU
  uint32_t nControl;
  asm volatile ("mrc p15, 0, %0, c1, c0,  0" : "=r" (nControl));
#if RASPPI == 1
#ifdef ARM_STRICT_ALIGNMENT
  nControl &= ~ARM_CONTROL_UNALIGNED_PERMITTED;
  nControl |= ARM_CONTROL_STRICT_ALIGNMENT;
#else
  nControl &= ~ARM_CONTROL_STRICT_ALIGNMENT;
  nControl |= ARM_CONTROL_UNALIGNED_PERMITTED;
#endif
#endif
  nControl |= MMU_MODE;

  RPI_Log(fromMMU, LOG_MMU, "Setting MMU_MODE");
  asm volatile ("mcr p15, 0, %0, c1, c0,  0" : : "r" (nControl) : "memory");

  RPI_Log(fromMMU, LOG_MMU, "MMU configured!");
  return 0;
}