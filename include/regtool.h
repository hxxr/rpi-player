/* regtool - Register Tool - Functions for controlling RPi peripherals  */

#pragma once

/* Pin modes for gpio_mode() and gpio_read_mode()  */
#define IN   0
#define OUT  1
#define ALT0 4
#define ALT1 5
#define ALT2 6
#define ALT3 7
#define ALT4 3
#define ALT5 2

/* Pin levels for gpio_write() and gpio_read()  */
#define LOW  0
#define HIGH 1

/* These are the physical base memory locations for the various registers.  */
/* Notice that they differ between different hardware versions of the Pi.  */
#ifndef HARDWARE
#   error \
"HARDWARE not defined. Please use gcc with the -DHARDWARE=X flag, \
where X is 1 (for Pi1/Pi0/Compute) or 2 (for Pi2/Pi3)."
#endif
#if HARDWARE == 1
/*  Pi Zero, Pi 1, Compute Module  */
#   define DMA_BASE   0x20007000
#   define CM_BASE    0x20101000
#   define GPIO_BASE  0x20200000
#   define PWM_BASE   0x2020C000
#   define MEM_FLAG   0xC
#else
/*  Pi 2, Pi 3  */
#   define DMA_BASE   0x3F007000
#   define CM_BASE    0x3F101000
#   define GPIO_BASE  0x3F200000
#   define PWM_BASE   0x3F20C000
#   define MEM_FLAG   0x4
#endif

/* These are the relative offsets for
   various locations of interest within registers.
   For example the base address of GPIO_SET is
   7 32-bit words (28 bytes) from the base address of the GPIO register.  */
#define GPIO_FSEL     0        /* GPIO Register, GPIO Function Select         */
#define GPIO_SET      7        /* GPIO Register, GPIO Pin Output Set          */
#define GPIO_CLR      10       /* GPIO Register, GPIO Pin Output Clear        */
#define GPIO_LEV      13       /* GPIO Register, GPIO Pin Output Level        */
#define DMACH(n)      ((n)*64) /* DMA Register, Base address of DMA channel n */
#define DMA_CS        0        /* DMA Register, Control and Status            */
#define DMA_CONBLK_AD 1        /* DMA Register, Control Block Address         */
#define DMA_DEBUG     8        /* DMA Register, Debug                         */
#define DMA_ENABLE    1020     /* DMA Register, Enable                        */
#define PWM_CTL       0        /* PWM Register, PWM Control                   */
#define PWM_STA       1        /* PWM Register, PWM Status                    */
#define PWM_DMAC      2        /* PWM Register, PWM DMA Configuration         */
#define PWM_RNG1      4        /* PWM Register, PWM Channel 1 Range           */
#define PWM_DAT1      5        /* PWM Register, PWM Channel 1 Data            */
#define PWM_FIF1      6        /* PWM Register, PWM FIFO Input                */
#define PWM_RNG2      8        /* PWM Register, PWM Channel 2 Range           */
#define PWM_DAT2      9        /* PWM Register, PWM Channel 2 Data            */
#define CM_PCMCTL     38       /* CM Register, PCM Clock Control              */
#define CM_PCMDIV     39       /* CM Register, PCM Clock Divisor              */
#define CM_PWMCTL     40       /* CM Register, PWM Clock Control              */
#define CM_PWMDIV     41       /* CM Register, PWM Clock Divisor              */

/* Commands that may be sent to DMA.                  Field type:  */
#define DMA_CS_ACTIVE                       (1<<0) /* Read and Write          */
#define DMA_CS_END                          (1<<1) /* Write 1 to clear        */
#define DMA_CS_INT                          (1<<2) /* Write 1 to clear        */
#define DMA_CS_PRIORITY(n)          ((15&(n))<<16) /* Read and Write          */
#define DMA_CS_PANIC_PRIORITY(n)    ((15&(n))<<20) /* Read and Write          */
#define DMA_CS_WAIT_FOR_OUTSTANDING_WRITES (1<<28) /* Read and Write          */
#define DMA_CS_ABORT                       (1<<30) /* Write 1 to activate     */
#define DMA_CS_RESET                       (1<<31) /* Write 1 to activate     */
#define DMA_DEBUG_READ_NOT_LAST_SET_ERROR   (1<<0) /* Write 1 to clear        */
#define DMA_DEBUG_FIFO_ERROR                (1<<1) /* Write 1 to clear        */
#define DMA_DEBUG_READ_ERROR                (1<<2) /* Write 1 to clear        */

/* Commands that may be sent to PWM.        Field type:  */
#define PWM_CTL_PWEN1             (1<<0) /* Read and Write                    */
#define PWM_CTL_MODE1             (1<<1) /* Read and Write                    */
#define PWM_CTL_RPTL1             (1<<2) /* Read and Write                    */
#define PWM_CTL_USEF1             (1<<5) /* Read and Write                    */
#define PWM_CTL_CLRF1             (1<<6) /* Write 1 to activate               */
#define PWM_CTL_PWEN2             (1<<8) /* Read and Write                    */
#define PWM_CTL_MODE2             (1<<9) /* Read and Write                    */
#define PWM_CTL_RPTL2            (1<<10) /* Read and Write                    */
#define PWM_CTL_USEF2            (1<<13) /* Read and Write                    */
#define PWM_STA_WERR              (1<<2) /* Read and Write                    */
#define PWM_STA_RERR              (1<<3) /* Read and Write                    */
#define PWM_STA_GAPO             (15<<4) /* Read and Write                    */
#define PWM_STA_BERR              (1<<8) /* Read and Write                    */
#define PWM_DMAC_DREQ(n)  ((255&(n))<<0) /* Read and Write                    */
#define PWM_DMAC_PANIC(n) ((255&(n))<<8) /* Read and Write                    */
#define PWM_DMAC_ENAB            (1<<31) /* Read and Write                    */

/* All writes to the Clock Manager register require bitwise OR with this passwd.
   The Clock Manager register must be undocumented for a good reason.  */
                              /* Field type:  */
#define CM_PASSWD    (90<<24) /* Write only                                   */

/* Commands that may be sent to the undocumented Clock Manager register.  */
                                        /* Field type:  */
#define CM_CTL_SRC(n)     ((15&(n))<<0) /* Read and Write                     */
#define CM_CTL_ENAB              (1<<4) /* Read and Write                     */
#define CM_CTL_KILL              (1<<5) /* Write 1 to activate                */
#define CM_CTL_BUSY              (1<<7) /* Read only                          */
#define CM_DIV_DIVI(n) ((4095&(n))<<12) /* Read and Write                     */

/* Commands that may be put inside DMA control blocks.  */
#define CB_TDMODE            (1<<1)
#define CB_WAIT_RESP         (1<<3)
#define CB_DEST_INC          (1<<4)
#define CB_DEST_DREQ         (1<<6)
#define CB_SRC_INC           (1<<8)
#define CB_SRC_DREQ         (1<<10)
#define CB_PERMAP(n) ((31&(n))<<16)
#define CB_NO_WIDE_BURSTS   (1<<26)
#define TIBASE              (CB_NO_WIDE_BURSTS | CB_WAIT_RESP)

/* Type for control blocks.  */
typedef struct cb_t {
    unsigned int ti;        /* Transfer Information                           */
    unsigned int source_ad; /* Source Address                                 */
    unsigned int dest_ad;   /* Destination Address                            */
    unsigned int txfr_len;  /* Transfer Length (bytes)                        */
    unsigned int stride;    /* 2D Stride (unused if 2D mode is not enabled)   */
    unsigned int nextconbk; /* Next Control Block Address                     */
    unsigned int reserved1; /* Reserved, do not use                           */
    unsigned int reserved2; /* Reserved, do not use                           */
} cb_t;

/* These pointers provide access to the part of the memory that contains
   DMA control blocks.  */
extern cb_t *cbs_v, *cbs_b;

/* Setup. Run before other functions.
   dmaPages: Amount of pages to allocate for cbs_v.
             Each page allows for 128 more control blocks in cbs_v.
             Set this to 0 if you are not planning to use DMA.
             A reminder that one page is 4096 bytes.
             Try not to allocate more than 4096 pages (16 MiB) of memory.  */
void regtool_setup(unsigned int dmaPages);

/* Cleanup. Run at end.  */
void regtool_cleanup(void);

/* Set pin mode to IN (0) or OUT (1).  */
/* You can also set to  */
/* ALT0 (4), ALT1 (5), ALT2 (6), ALT3 (7), ALT4 (3) or ALT5 (2).  */
void gpio_mode(int pin, int mode);

/* Read pin mode as
   IN (0), OUT(1), ALT0 (4), ALT1 (5), ALT2 (6), ALT3 (7), ALT4 (3) or ALT5 (2).
*/
int gpio_read_mode(int pin);

/* Set output pin level to LOW (0) or HIGH (1).  */
void gpio_write(int pin, int level);

/* Read the state of an input or output pin as LOW (0) or HIGH (1).  */
int gpio_read(int pin);

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
unsigned int vc_create(void **virtAddr, void **busAddr, unsigned int pages);

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
void vc_destroy(unsigned int handle, void *virtAddr, unsigned int pages);

/* Set DMA channel to use. You can use channel 0, 4, 5 or 6. Default 5.
   Run this before regtool_setup().  */
void set_dmach(int dmach);

/* Get maximum length (in bytes) of cbs_v.  */
unsigned int cbs_len(void);

/* Start DMA.
   index: The index of the first DMA control block to load from cbs_v.  */
void activate_dma(unsigned int index);

/* Stop DMA. This is called automatically with regtool_cleanup().  */
void stop_dma(void);

/* Returns 1 if DMA is active, otherwise returns 0.  */
int dma_running(void);

/* Returns the index of the DMA control block currently being output.  */
unsigned int dma_current_cb(void);

/* Get the physical address of a peripheral register location, for DMA purposes.
   base: One of DMA_BASE, CM_BASE, GPIO_BASE, PWM_BASE.
   offset: Offset in 32-bit words (for example GPIO_SET or DMA_CS or PWM_FIF1).
*/
unsigned int periph(unsigned int base, unsigned int offset);
