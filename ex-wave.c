#include <stdio.h>    /* printf(), fgets(), stdin                             */

#include "include/driver.h"



/* GPIO pin to use (BCM number) */
#define PIN  21

/* Frequency (Hz) */
#define FREQ 440.00

/* Duty cycle (between 0 and 1, exclusive) */
#define DUTY 0.5


/* Virtual and bus address of GPIO command buffer */
unsigned int *cmdV, *cmdB, cmdH;

int main(void) {
    unsigned int cbsLen;
    char str[5];
    int usDelay, onDelay, offDelay;

    /* Setup DMA, allocate 1 page for control blocks */
    driver_setup(1);

    /* Make 1 page for DMA to receive GPIO commands from */
    cmdH = vc_create((void **)&cmdV, (void **)&cmdB, 1);

    usDelay  = 500000/FREQ;  /* Average delay (microseconds) for transitions */
    onDelay  = usDelay*DUTY*2;    /* Delay for on transitions  */
    offDelay = usDelay*2-onDelay; /* Delay for off transitions */
    cmdV[0] = 1<<PIN;

    cbsLen = cbs_len()/sizeof(cb_t);
    printf("\nMaximum amount of control blocks allowed: %u\n\n", cbsLen);

    printf("Playing on GPIO %u at %g Hz with %g%% duty cycle.\n\n",
        PIN,
        (double)FREQ,
        (double)DUTY*100);

    /* Set GPIO pin mode to output */
    gpio_mode(PIN, OUT);

    /* On
       Copy a 32-bit integer (4 bytes per 32-bit integer) from cmdV[0] to the
       GPIO_SET location in the GPIO register, turning that pin on.
       Afterwards, go to Delay1 phase. */
    cbs_v[0].ti         =  TIBASE;                      /* Regular Copy       */
    cbs_v[0].source_ad  =  (unsigned int)&cmdB[0];      /* Src:    cmdV[0]    */
    cbs_v[0].dest_ad    =  periph(GPIO_BASE, GPIO_SET); /* Dest:   GPIO_SET   */
    cbs_v[0].txfr_len   =  4;                           /* Length: 4*1        */
    cbs_v[0].nextconbk  =  (unsigned int)&cbs_b[1];     /* Next:   cbs_v[1]   */

    /* Delay1
       Copy onDelay 32-bit integers (4 bytes per 32-bit integer) from
       a dummy location (cmdV[0]) to the PWM FIFO.
       We set up the clock so that each 32-bit integer written to the FIFO
       causes exactly 1 microsecond of delay.
       Afterwards, go to Off phase. */
    cbs_v[1].ti = TIBASE | CB_DEST_DREQ | CB_PERMAP(5); /* Sync with PWM FIFO */
    cbs_v[1].source_ad  =  (unsigned int)&cmdB[0];      /* Src:    cmdV[0]    */
    cbs_v[1].dest_ad    =  periph(PWM_BASE, PWM_FIF1);  /* Dest:   PWM_FIF1   */
    cbs_v[1].txfr_len   =  4 * onDelay;                 /* Length: 4*onDelay  */
    cbs_v[1].nextconbk  =  (unsigned int)&cbs_b[2];     /* Next:   cbs_v[2]   */

    /* Off
       Copy a 32-bit integer (4 bytes per) from cmdV[0] to the
       GPIO_CLR location in the GPIO register, turning that pin off.
       Afterwards, go to Delay2 phase. */
    cbs_v[2].ti         =  TIBASE;                      /* Regular Copy       */
    cbs_v[2].source_ad  =  (unsigned int)&cmdB[0];      /* Src:    cmdV[0]    */
    cbs_v[2].dest_ad    =  periph(GPIO_BASE, GPIO_CLR); /* Dest:   GPIO_CLR   */
    cbs_v[2].txfr_len   =  4;                           /* Length: 4*1        */
    cbs_v[2].nextconbk  =  (unsigned int)&cbs_b[3];     /* Next:   cbs_v[3]   */

    /* Delay2
       Copy offDelay 32-bit integers (4 bytes per) from
       a dummy location (cmdV[0]) to the PWM FIFO.
       We set up the clock so that each 32-bit integer written to the FIFO
       causes exactly 1 microsecond of delay.
       Afterwards, go back to On phase. */
    cbs_v[3].ti = TIBASE | CB_DEST_DREQ | CB_PERMAP(5); /* Sync with PWM FIFO */
    cbs_v[3].source_ad  =  (unsigned int)&cmdB[0];      /* Src:    cmdV[0]    */
    cbs_v[3].dest_ad    =  periph(PWM_BASE, PWM_FIF1);  /* Dest:   PWM_FIF1   */
    cbs_v[3].txfr_len   =  4 * offDelay;                /* Length: 4*offDelay */
    cbs_v[3].nextconbk  =  (unsigned int)&cbs_b[0];     /* Next:   cbs_v[0]   */

    /* Begin DMA copy operation with control block at index 0 */
    activate_dma(0);

    /* Wait */
    printf("Press RETURN to stop.\n");
    fgets(str, 5, stdin);

    /* Stop DMA */
    stop_dma();

    /* Turn GPIO pin off */
    gpio_write(PIN, 0);

    /* Free resources */
    vc_destroy(cmdH, cmdV, 1);
    driver_cleanup();

    return 0;
}
