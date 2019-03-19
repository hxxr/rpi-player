#include <stdio.h>    /* printf()                                             */
#include <string.h>   /* strncpy()                                            */

#include "include/driver.h"

/* String to copy  */
#define STR    "Hello, World!"

char *srcArrayV,  *srcArrayB;  /* Virtual and bus address of source       */
char *destArrayV, *destArrayB; /* Virtual and bus address of destination  */
unsigned int srcArrayH, destArrayH; /* Handles for releasing array memory */

int main(void) {
    unsigned int cbsLen;

    /* Setup DMA, allocate 1 page for control blocks  */
    driver_setup(1);

    /* Allocate 1 page for each of source array and destination array  */
    srcArrayH  = vc_create((void **)&srcArrayV,  (void **)&srcArrayB,  1);
    destArrayH = vc_create((void **)&destArrayV, (void **)&destArrayB, 1);

    cbsLen = cbs_len()/sizeof(cb_t);
    printf("\nMaximum amount of control blocks allowed: %u\n\n", cbsLen);

    /* Copy the string into srcArrayV  */
    strncpy(srcArrayV, STR, 4096);

    /* Create a control block at index 0 that tells DMA to transfer array data
       CB_SRC_INC: Increase source address by 4 for every 4 transfers
       CB_DEST_INC: Increase destination address by 4 for every 4 transfers  */
    cbs_v[0].ti        =  TIBASE | CB_SRC_INC | CB_DEST_INC;
    cbs_v[0].source_ad =  (unsigned int)srcArrayB;   /* Bus address of source */
    cbs_v[0].dest_ad   =  (unsigned int)destArrayB;  /* Bus address of dest   */
    cbs_v[0].txfr_len  =  4096;            /* Transfer length (array length)  */
    cbs_v[0].nextconbk =  0;               /* No next control block           */

    /* Begin DMA copy operation with control block at index 0  */
    printf("Before DMA, srcArrayV  reads: '%s'\n", srcArrayV);
    printf("Before DMA, destArrayV reads: '%s'\n\n", destArrayV);
    activate_dma(0);
    printf("After  DMA, srcArrayV  reads: '%s'\n", srcArrayV);
    printf("After  DMA, destArrayV reads: '%s'\n\n", destArrayV);

    /* Free resources  */
    vc_destroy(srcArrayH,  srcArrayV,  1);
    vc_destroy(destArrayH, destArrayV, 1);
    driver_cleanup();

    return 0;
}
