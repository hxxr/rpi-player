/* player - Helper functions for sound wave generation on GPIO pins  */

#pragma once

/* Pages for DMA control blocks.  */
#define PAGES 128




/* Type for miscellaneous player information.  */
typedef struct misc_t {
    double value;  /* Length of note, in beats. (<= 1)                        */

    char usingPs;  /* Whether we are using pitch slide.                       */
    double freqTo; /* If using pitch slide, desired ending frequency.         */
    double freqS;  /* If using pitch slide, start offset in beats. (<= 1)     */
    double freqE;  /* If using pitch slide, end offset in beats.              */

    char usingDs;  /* Whether we are using dutycycle slide.                   */
    double dutyTo; /* If using dutycycle slide, desired ending dutycycle.     */
    double dutyS;  /* If using dutycycle slide, start offset in beats. (<= 1) */
    double dutyE;  /* If using dutycycle slide, end offset in beats.          */

    char usingV;   /* Whether we are modifying vibrato settings.              */
    double vInt;   /* If modifying, vibrato range in cents.                   */
    unsigned vWth; /* If modifying, length of vibrato pulse in microseconds.  */

    char usingT;   /* Whether we are modifying tremolo settings.              */
    double tInt;   /* If modifying, tremolo range.                            */
    unsigned tWth; /* If modifying, length of tremolo pulse in microseconds.  */

    unsigned us;   /* If non-zero, global beat length is changed next beat.   */
} misc_t;




#ifndef NULL
#   define NULL ((void *)0)
#endif

/* Add a voice to the queue.
   pin:    GPIO pin number (BCM) through which the voice plays.
   freqs:  Array of frequencies (Hz). A zero (0) indicates pin should be off.
   duties: Array of duty cycles (0 to 1, exclusive).
   misc:   Array of misc_t pointers containing extra data. This may be NULL.  */
void queueAdd(int pin, double *freqs, double *duties, misc_t **misc);

/* Play queue. This function also consumes the queue.
   us:    Length of each beat in microseconds (60000000/BPM).
   beats: Total number of queued beats.  */
void queuePlay(unsigned int us, unsigned int beats);




/* Frequencies of notes in Hz (a4 = 440, equal temperament).
   c4 is c natural, while C4 is c sharp.  */
#define ___    NULL /* Fills in blank spaces in misc arrays  */
#define __    0.000 /* Fills in blank spaces in freq arrays  */
#define XX    0.000 /* Indicates somewhere where pitch/dutycycle slide occurs */
#define c0   16.351
#define C0   17.324
#define d0   18.354
#define D0   19.445
#define e0   20.601
#define f0   21.827
#define F0   23.124
#define g0   24.499
#define G0   25.956
#define a0   27.500
#define A0   29.135
#define b0   30.868
#define c1   32.703
#define C1   34.648
#define d1   36.708
#define D1   38.891
#define e1   41.203
#define f1   43.654
#define F1   46.249
#define g1   48.999
#define G1   51.913
#define a1   55.000
#define A1   58.270
#define b1   61.735
#define c2   65.406
#define C2   69.296
#define d2   73.416
#define D2   77.782
#define e2   82.407
#define f2   87.307
#define F2   92.499
#define g2   97.999
#define G2  103.826
#define a2  110.000
#define A2  116.541
#define b2  123.471
#define c3  130.813
#define C3  138.591
#define d3  146.832
#define D3  155.563
#define e3  164.814
#define f3  174.614
#define F3  184.997
#define g3  195.998
#define G3  207.652
#define a3  220.000
#define A3  233.082
#define b3  246.942
#define c4  261.626
#define C4  277.183
#define d4  293.665
#define D4  311.127
#define e4  329.628
#define f4  349.228
#define F4  369.994
#define g4  391.995
#define G4  415.305
#define a4  440.000
#define A4  466.164
#define b4  493.883
#define c5  523.251
#define C5  554.365
#define d5  587.330
#define D5  622.254
#define e5  659.255
#define f5  698.456
#define F5  739.989
#define g5  783.991
#define G5  830.609
#define a5  880.000
#define A5  932.328
#define b5  987.767
#define c6 1046.502
#define C6 1108.731
#define d6 1174.659
#define D6 1244.508
#define e6 1318.510
#define f6 1396.913
#define F6 1479.978
#define g6 1567.982
#define G6 1661.219
#define a6 1760.000
#define A6 1864.655
#define b6 1975.533
#define c7 2093.005
#define C7 2217.461
#define d7 2349.318
#define D7 2489.016
#define e7 2637.021
#define f7 2793.826
#define F7 2959.955
#define g7 3135.964
#define G7 3322.438
#define a7 3520.000
#define A7 3729.310
#define b7 3951.066
#define c8 4186.009
#define C8 4434.922
#define d8 4698.636
#define D8 4978.032
#define e8 5274.042
#define f8 5587.652
#define F8 5919.910
#define g8 6271.928
#define G8 6644.876
#define a8 7040.000
#define A8 7458.620
#define b8 7902.132
