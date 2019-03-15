/* regtool - Register Tool - Functions for controlling RPi peripherals  */

#define _BSD_SOURCE

#include <stdio.h>     /* fprintf(), stderr                                   */
#include <stdlib.h>    /* exit()                                              */
#include <fcntl.h>     /* open()                                              */
#include <unistd.h>    /* close(), usleep()                                   */
#include <sys/mman.h>  /* mmap(), mlock(), munlock()                          */
#include <string.h>    /* memset()                                            */
#include <sys/ioctl.h> /* ioctl(), _IOWR()                                    */

#include "regtool.h"




/* DMA channel to use.
   Only channels 0, 4, 5 and 6 are available for use.  */
static unsigned int dch = 5;

/* These pointers provide access to the part of the memory that contains
   DMA control blocks.  */
cb_t *cbs_v, *cbs_b;

/* This is used to free the memory for the DMA control blocks later.  */
static unsigned int dmah;

/* Length of cbs_v in pages (1 page = 128 control blocks = 4096 bytes).  */
static unsigned int cbs_pages;

/* These pointers provide access to the parts of the memory that
   control the GPIO pins and other hardware peripherals.  */
static volatile unsigned int *gpio_reg;  /* GPIO Register           */
static volatile unsigned int *dma_reg;   /* DMA Register            */
static volatile unsigned int *pwm_reg;   /* PWM Register            */
static volatile unsigned int *cm_reg;    /* Clock Manager Register  */

/* Each register corresponds to
   a specific memory location in the Raspberry Pi's memory.
   Each register is an array of 32-bit unsigned integers which
   control certain Raspberry Pi hardware features.

   Info on the GPIO, DMA and PWM registers can be found here:
   https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
   Pages 90, 39 and 141.

   The CM (clock manager) register is undocumented. Info on it is here:
   https://www.scribd.com/doc/127599939/BCM2835-Audio-clocks
   It's only 3 pages long. Look through all of it.  */


/*############################################################################*/


/* These functions use the Raspberry Pi's mailbox property interface to manage
   GPU memory. More information can be found here:
   https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
*/


static int mailbox_open() {
    int fd = open("/dev/vcio", 0);
    return fd;
}
static void mailbox_close(int fd) {
    close(fd);
}
static int mailbox_property(int fd, void *buf) {
    return ioctl(fd, _IOWR(100, 0, char *), buf);
}
static unsigned int mailbox_alloc(int fd,
                                 unsigned int size,
                                 unsigned int align,
                                 unsigned int flags) {
    int i = 1;
    unsigned int p[32];
    p[i++] = 0;
    p[i++] = 0x3000c;
    p[i++] = 12;
    p[i++] = 12;
    p[i++] = size;
    p[i++] = align;
    p[i++] = flags;
    p[i++] = 0;
    p[0]   = i * sizeof(*p);
    mailbox_property(fd, p);
    return p[5];
}
static unsigned int mailbox_free(int fd, unsigned int handle) {
    int i = 1;
    unsigned int p[32];
    p[i++] = 0;
    p[i++] = 0x3000f;
    p[i++] = 4;
    p[i++] = 4;
    p[i++] = handle;
    p[i++] = 0;
    p[0]   = i * sizeof(*p);
    mailbox_property(fd, p);
    return p[5];
}
static unsigned int mailbox_lock(int fd, unsigned int handle) {
    int i = 1;
    unsigned int p[32];
    p[i++] = 0;
    p[i++] = 0x3000d;
    p[i++] = 4;
    p[i++] = 4;
    p[i++] = handle;
    p[i++] = 0;
    p[0]   = i * sizeof(*p);
    mailbox_property(fd, p);
    return p[5];
}
static unsigned int mailbox_unlock(int fd, unsigned int handle) {
    int i = 1;
    unsigned int p[32];
    p[i++] = 0;
    p[i++] = 0x3000e;
    p[i++] = 4;
    p[i++] = 4;
    p[i++] = handle;
    p[i++] = 0;
    p[0]   = i * sizeof(*p);
    mailbox_property(fd, p);
    return p[5];
}
static void *mailbox_mapmem(unsigned int base, unsigned int size) {
    int fd;
    void *mem;
    unsigned int offset = base % 4096;
    base = base - offset;
    size = size + offset;
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, base);
    close(fd);
    return mem;
}
static void mailbox_unmapmem(void *addr, unsigned int size) {
    munmap(addr, size);
}


/*############################################################################*/


/* Map a part of the system memory to virtual memory so that it can be modified.
   base: Memory address of first byte to map
   pages: Amount of pages to map (1 page = 4096 bytes)  */
static void *memory_map(unsigned int base, unsigned int pages) {
    void *mem;

    /* Attempt to open the device file "/dev/mem".  */
    /* This file provides access to Raspberry Pi's physical memory.  */
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr,
        "ERROR: regtool_setup(): Cannot open /dev/mem. Try using sudo.\n");
        exit(1);
    }

    /* Now, attempt to map.  */
    mem = mmap(NULL, 4096*pages, PROT_READ|PROT_WRITE,
                     MAP_SHARED, fd, base);

    /* Close file and return the virtual memory location.  */
    close(fd);
    return mem;
}


/*############################################################################*/


/* Allocate GPU memory using mailbox interface, so that it is completely
   contiguous in physical memory, even across pages, and is cache-coherent.
   virtAddr: Location to store the virtual address of the memory location.
             Use the virtual address to edit the memory from your program.
   busAddr:  Location to store the bus address of the memory location.
             Use the bus address to access the memory from DMA control blocks.
   pages:    Amount of pages to allocate (1 page = 4096 bytes).
   Usage example:
        void *virt, *bus;
        unsigned int handle = vc_create(&virt, &bus, 1);
        // Do whatever...
        vc_destroy(handle, virt, 1);
*/
unsigned int vc_create(void **virtAddr, void **busAddr, unsigned int pages) {
    int fd = mailbox_open();
    unsigned int size = 4096*pages;
    unsigned int handle = mailbox_alloc(fd, size, 4096, MEM_FLAG);
    unsigned int bus_addr = mailbox_lock(fd, handle);
    void *virt_addr = mailbox_mapmem(bus_addr & ~0xC0000000, size);
    mailbox_close(fd);
    memset(virt_addr, 0, size); /* Zero fill  */
    *virtAddr = virt_addr;
    *busAddr  = (void *)bus_addr;
    return handle;
}


/*############################################################################*/


/* Free the memory alllocated using vc_create().
   handle:   Return value of vc_create().
   virtAddr: Virtual address of allocated memory.
   pages:    Amount of pages that were allocated.
   Usage example:
        void *virt, *bus;
        unsigned int handle = vc_create(&virt, &bus, 1);
        // Do whatever...
        vc_destroy(handle, virt, 1);
*/
void vc_destroy(unsigned int handle, void *virtAddr, unsigned int pages) {
    int fd = mailbox_open();
    unsigned int size = 4096*pages;
    mailbox_unmapmem(cbs_v, size);
    mailbox_unlock(fd, handle);
    mailbox_free(fd, handle);
    mailbox_close(fd);
}


/*############################################################################*/


/* Set pin mode to IN (0) or OUT (1).
   You can also set to
   ALT0 (4), ALT1 (5), ALT2 (6), ALT3 (7), ALT4 (3) or ALT5 (2).  */
void gpio_mode(int pin, int mode) {
    /* Record the 32-bit integer from GPIO_FSEL in the
       GPIO register that contains the pin's state  */
    unsigned int fsel = gpio_reg[GPIO_FSEL + pin/10];

    /* Replace the pin's corresponding three bits with the desired mode:  */
    /* Set all of the three bits to 0 first, using a bitwise AND  */
    fsel &= ~(7 << (3*(pin%10)));
    /* Now set the three bits to the desired mode using a bitwise OR  */
    fsel |= (mode << (3*(pin%10)));

    /* Write the modified pin mode to GPIO_FSEL in the GPIO register  */
    gpio_reg[GPIO_FSEL + pin/10] = fsel;
}


/*############################################################################*/


/* Read pin mode as
   IN (0), OUT(1), ALT0 (4), ALT1 (5), ALT2 (6), ALT3 (7), ALT4 (3) or ALT5 (2).
*/
int gpio_read_mode(int pin) {
    /* Read three bits from GPIO_FSEL in the GPIO register  */
    return 7 & (gpio_reg[GPIO_FSEL + pin/10] >> (3*(pin%10)));
}


/*############################################################################*/


/* Set output pin level to LOW (0) or HIGH (1).  */
void gpio_write(int pin, int level) {
    if (level) {
        /* Turn GPIO pin on by writing to GPIO_SET in the GPIO register  */
        gpio_reg[GPIO_SET + pin/32] = 1 << (pin%32);
    } else {
        /* Turn GPIO pin off by writing to GPIO_CLR in the GPIO register  */
        gpio_reg[GPIO_CLR + pin/32] = 1 << (pin%32);
    }
}


/*############################################################################*/


/* Read the state of an input or output pin as LOW (0) or HIGH (1).  */
int gpio_read(int pin) {
    /* Read one bit from GPIO_LEV in the GPIO register to get the pin state  */
    return 1 & (gpio_reg[GPIO_LEV + pin/32] >> (pin%32));
}


/*############################################################################*/


/* Set DMA channel to use. You can use channel 0, 4, 5 or 6. Default 5.
   Run this before regtool_setup().  */
void set_dmach(int dmach) {
    dch = dmach;
}


/*############################################################################*/


/* Get maximum length (in bytes) of cbs_v.  */
unsigned int cbs_len(void) {
    return 4096*cbs_pages;
}


/*############################################################################*/


/* Start DMA.
   index: The index of the first DMA control block to load from cbs_v.  */
void activate_dma(unsigned int index) {
    /* Make sure DMA channel is enabled by
       writing the corresponding bit in DMA_ENABLE in the DMA register to 1  */
    dma_reg[DMA_ENABLE] |= 1 << dch;

    /* Stop DMA, if it was already started  */
    dma_reg[DMACH(dch) + DMA_CS] = DMA_CS_RESET;

    /* Clear DMA status flags  */
    dma_reg[DMACH(dch) + DMA_CS] = DMA_CS_INT |  /* Interrupted flag  */
                                   DMA_CS_END;   /* Transmission ended flag  */

    /* Set the bus address of the control block to load  */
    dma_reg[DMACH(dch) + DMA_CONBLK_AD] = (unsigned int)&cbs_b[index];

    /* Clear any DMA errors from previous transmissions  */
    dma_reg[DMACH(dch) + DMA_DEBUG] = DMA_DEBUG_FIFO_ERROR |
                                      DMA_DEBUG_READ_ERROR |
                                      DMA_DEBUG_READ_NOT_LAST_SET_ERROR;

    /* Set DMA priority to priority 7 (highest 15, lowest 0) and start DMA  */
    dma_reg[DMACH(dch) + DMA_CS] = DMA_CS_PRIORITY(7)                 |
                                   DMA_CS_PANIC_PRIORITY(7)           |
                                   DMA_CS_WAIT_FOR_OUTSTANDING_WRITES |
                                   DMA_CS_ACTIVE;
}


/*############################################################################*/


/* Stop DMA. This is called automatically with regtool_cleanup().  */
void stop_dma(void) {
    dma_reg[DMACH(dch) + DMA_CS] = DMA_CS_RESET;
}


/*############################################################################*/


/* Returns 1 if DMA is active, otherwise returns 0.  */
int dma_running(void) {
    return !!(dma_reg[DMACH(dch) + DMA_CS] & DMA_CS_ACTIVE);
}


/*############################################################################*/


/* Returns the index of the DMA control block currently being output.  */
unsigned int dma_current_cb(void) {
    return (dma_reg[DMACH(dch) + DMA_CONBLK_AD] - (int)cbs_b) / sizeof(cb_t);
}


/*############################################################################*/


/* Get the physical address of a peripheral register location, for DMA purposes.
   base: One of DMA_BASE, CM_BASE, GPIO_BASE, PWM_BASE.
   offset: Offset in 32-bit words (for example GPIO_SET or DMA_CS or PWM_FIF1).
*/
unsigned int periph(unsigned int base, unsigned int offset) {
    return ((base + offset*4) & 0x00FFFFFF) | 0x7E000000;
}


/*############################################################################*/


/* Setup. Run before other functions.
   dmaPages: Amount of pages to allocate for cbs_v.
             Each page allows for 128 more control blocks in cbs_v.
             Set this to 0 if you are not planning to use DMA.
             A reminder that one page is 4096 bytes.
             Try not to allocate more than 4096 pages (16 MiB) of memory.  */
void regtool_setup(unsigned int dmaPages) {
    /* Map certain parts of the Raspberry Pi's memory to virtual memory.  */
    dma_reg   = (unsigned int *)memory_map(DMA_BASE,  1);
    pwm_reg   = (unsigned int *)memory_map(PWM_BASE,  1);
    cm_reg    = (unsigned int *)memory_map(CM_BASE,   1);
    gpio_reg  = (unsigned int *)memory_map(GPIO_BASE, 1);

    cbs_pages = 0;

    if (dmaPages) {
        cbs_pages = dmaPages;

        /* Stop DMA  */
        stop_dma();

        /* Allocate pages for DMA control blocks  */
        dmah = vc_create((void **)&cbs_v, (void **)&cbs_b, cbs_pages);
    }



    /* Now we need to start and configure the PWM clock so that we may
       use it for accurate DMA delays.  */



    /* Disable PWM  */
    pwm_reg[PWM_CTL] &= (~PWM_CTL_PWEN1);
    pwm_reg[PWM_CTL] &= (~PWM_CTL_PWEN2);

    /* Disable the PWM clock by turning off the ENAB bit in CM_PWMCTL  */
    cm_reg[CM_PWMCTL] = CM_PASSWD | (cm_reg[CM_PWMCTL] & (~CM_CTL_ENAB));

    /* Wait until the BUSY bit in CM_PWMCTL is off
      (wait until clock turns off)  */
    if (cm_reg[CM_PWMCTL] & CM_CTL_BUSY) {
        do {
            cm_reg[CM_PWMCTL] = CM_PASSWD | CM_CTL_KILL;
        } while (cm_reg[CM_PWMCTL] & CM_CTL_BUSY);
    }

    /* Set clock source to source 6 "PLLD" (constant 500 MHz clock source)  */
    cm_reg[CM_PWMCTL] = CM_PASSWD | CM_CTL_SRC(6);
    usleep(10);

    /* Set clock divisor to 50 (500 MHz / 50 = 10 MHz)  */
    cm_reg[CM_PWMDIV] = CM_PASSWD | CM_DIV_DIVI(50);
    usleep(10);

    /* Enable clock  */
    cm_reg[CM_PWMCTL] |= CM_PASSWD | CM_CTL_ENAB;

    /* Wait until the BUSY bit in CM_PWMCTL is on
      (wait until clock turns on)  */
    do {} while ((cm_reg[CM_PWMCTL] & CM_CTL_BUSY) == 0);

    /* Reset PWM  */
    pwm_reg[PWM_CTL] = 0;   /* Set every bit in PWM_CTL to 0  */
    usleep(10);
    pwm_reg[PWM_STA] = -1;  /* Set every bit in PWM_STA to 1  */
    usleep(10);

    /* Set number of bits to transmit to 10 (10 MHz / 10 = 1 MHz)
       1 MHz => 1 microsecond delay per 32-bit word written to FIFO  */
    pwm_reg[PWM_RNG1] = 10;
    usleep(10);

    /* Enable sending DREQ signal to DMA  */
    pwm_reg[PWM_DMAC] = PWM_DMAC_DREQ(15) | PWM_DMAC_PANIC(15) | PWM_DMAC_ENAB;
    usleep(10);

    /* Clear FIFO  */
    pwm_reg[PWM_CTL] = PWM_CTL_CLRF1;
    usleep(10);

    /* Enable PWM channel 1, and make it use FIFO  */
    pwm_reg[PWM_CTL] = PWM_CTL_USEF1 | PWM_CTL_MODE1 | PWM_CTL_PWEN1;
}


/*############################################################################*/


/* Cleanup. Run at end.  */
void regtool_cleanup(void) {
    if (cbs_pages) {
        /* Stop DMA  */
        stop_dma();

        /* Release DMA control blocks  */
        vc_destroy(dmah, cbs_v, cbs_pages);
    }

    /* Unmap registers  */
    munmap((void *)dma_reg,  4096);
    munmap((void *)pwm_reg,  4096);
    munmap((void *)cm_reg,   4096);
    munmap((void *)gpio_reg, 4096);
}


/*############################################################################*/
