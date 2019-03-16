/* player - Helper functions for sound wave generation on GPIO pins  */

#define _BSD_SOURCE

#include <string.h>  /* memcpy(), memset()                                    */
#include <unistd.h>  /* usleep()                                              */
#include <math.h>    /* pow()                                                 */

#include "regtool.h"
#include "player.h"




/* Type returned by waveGen, containing useful information about how it went  */
typedef struct wavegen_info_t {
    /* Length of generated waveform in transitions.
       Also half the length in control blocks.  */
    unsigned int length;

    /* Length of generated waveform in microseconds.  */
    unsigned int micros;

    /* What the "v_offset" argument should be for the next wave in order to sync
       properly without any "pop" sound  */
    unsigned int v_offset;

    /* What the "t_offset" argument should be for the next wave in order to sync
       properly without any "pop" sound  */
    unsigned int t_offset;

    /* What the "w_offset" argument should be for the next wave in order to sync
       properly without any "pop" sound  */
    unsigned int w_offset;

    /* What the "w_on" argument should be for the next wave in order to sync
       properly without any "pop" sound  */
    char w_on;

} wavegen_info_t;

/* Type for wave transitions. Waves are arrays of these transitions.  */
typedef struct pulse_t {
    /* Pins that should be turned on. (1<<pin)
       Please leave one of gpioOn or gpioOff equal to 0.  */
    unsigned int gpioOn;

    /* Pins that should be turned off. (1<<pin)
       Please leave one of gpioOn or gpioOff equal to 0.  */
    unsigned int gpioOff;

    /* Delay in microseconds after this transition.  */
    unsigned int usDelay;

} pulse_t;

static unsigned int cbs_index = 0;
static unsigned int cmd_index = 0;
static unsigned int dma_laps = 0;
static unsigned int dma_last = 0;
static unsigned int cbs_laps = 0;
static int firstWave = 1;
static unsigned int wOutLength = 0;

static double  *(_freq[32]);
static double  *(_duty[32]);
static misc_t **(_misc[32]);

static unsigned int pins;
static wavegen_info_t _info[32];

static unsigned int *cmdV, *cmdB, cmdH;
static pulse_t wIn1[PAGES*64];
static pulse_t wIn2[PAGES*64];
static pulse_t wOut[PAGES*64];




/*############################################################################*/


/* Returns the minimum of two doubles, or the first one if they are equal.  */
static double dmin(double a, double b) {
    return (a <= b) ? a : b;
}


/*############################################################################*/


/* Returns the maximum of two doubles, or the first one if they are equal.  */
static double dmax(double a, double b) {
    return (a >= b) ? a : b;
}


/*############################################################################*/


/* Calculate the frequency somewhere between two frequencies.
   For example interpolateFreq(c4, d4, 0.5) is the frequency of c sharp 4.  */
static double interpolateFreq(double freqStart, double freqEnd, double factor) {
    return freqStart*pow(freqEnd/freqStart,factor);
}


/*############################################################################*/


/* Calculate the duty cycle somewhere between two duty cycle values.
   For example interpolateDuty(0.1, 0.2, 0.5) is 0.15.  */
static double interpolateDuty(double dutyStart, double dutyEnd, double factor) {
    return dutyStart+(dutyEnd-dutyStart)*factor;
}


/*############################################################################*/


/* Interpolates for vibrato.
   base:      Base frequency.
   intensity: Vibrato range (cents, also known as hundredths of a semitone)
   width:     Length of each vibrato pulse in microseconds.
   us:        Time in microseconds since start.  */
static double vibrato(double base, double intensity, double width, double us) {
    double f;
    if (!intensity || !width) return base;
    f = ((4*us)/width) - ((unsigned int)((4*us)/width));
    switch (((unsigned int)((4*us)/width))%4) {
        case 0: return interpolateFreq(base, base*pow(2,intensity/1200), f);
        case 1: return interpolateFreq(base*pow(2,intensity/1200), base, f);
        case 2: return interpolateFreq(base, base/pow(2,intensity/1200), f);
        case 3: return interpolateFreq(base/pow(2,intensity/1200), base, f);
    }
    return base;
}


/*############################################################################*/


/* Interpolates for tremolo.
   base:      Base dutycycle.
   intensity: Tremolo range.
   width:     Length of each tremolo pulse in microseconds.
   us:        Time in microseconds since start.  */
static double tremolo(double base, double intensity, double width, double us) {
    double f;
    if (!intensity || !width) return base;
    f = ((4*us)/width) - ((unsigned int)((4*us)/width));
    switch (((unsigned int)((4*us)/width))%4) {
        case 0: return interpolateDuty(base, base+intensity, f);
        case 1: return interpolateDuty(base+intensity, base, f);
        case 2: return interpolateDuty(base, base-intensity, f);
        case 3: return interpolateDuty(base-intensity, base, f);
    }
    return base;
}


/*############################################################################*/


/* Generate a waveform and add it to the queue.
   pin:         GPIO pin (BCM number) to output to.
   freqS:       Frequency (Hz) at start of waveform.
   freqE:       Frequency (Hz) at end of waveform. If this is different from
                freqS there will be a linear pitch slide.
   freqDelayS:  Microseconds from start of waveform when pitch slide begins.
   freqDelayE:  Microseconds from start of waveform when pitch slide ends.
   dutyS:       Duty cycle (0 to 1, exclusive) at start of waveform.
   dutyE:       Duty cycle (0 to 1, exclusive) at end of waveform. If this is
                different from dutyS there will be a linear duty cycle slide.
   dutyDelayS:  Microseconds from start of waveform when dutycycle slide begins.
   dutyDelayE:  Microseconds from start of waveform when dutycycle slide ends.
   vIntensity:  Vibrato range (cents, also known as hundredths of a semitone).
   vWidth:      Length of each vibrato pulse in microseconds.
   tIntensity:  Tremolo range.
   tWidth:      Length of each tremolo pulse in microseconds.
   len:         Length of waveform in microseconds, including offset.
   value:       Amount of waveform (0 to 1, inclusive) that contains sound.
   v_offset:    Offset in microseconds for vibrato.
   v_offset:    Offset in microseconds for tremolo.
   w_offset:    Microseconds to add to beginning before wave starts.
   w_on:        1 if wave starts on, 0 if wave starts off.
                Offset starts opposite. */
static wavegen_info_t waveGen(int pin,
                           double freqS,
                           double freqE,
                         unsigned freqDelayS,
                         unsigned freqDelayE,
                           double dutyS,
                           double dutyE,
                         unsigned dutyDelayS,
                         unsigned dutyDelayE,
                           double vIntensity,
                         unsigned vWidth,
                           double tIntensity,
                         unsigned tWidth,
                         unsigned len,
                           double value,
                         unsigned v_offset,
                         unsigned t_offset,
                         unsigned w_offset,
                             char w_on) {
    /* Current frequency  */
    double freq = freqS;
    /* Current duty cycle  */
    double duty = dutyS;
    /* Current factor used for frequency interpolation  */
    double ffac = 0;
    /* Current factor used for duty cycle interpolation  */
    double dfac = 0;
    /* Amount of waveform (0 to 1) that has been generated  */
    double vfac = 0;
    /* Length (microseconds) of waveform still waiting to be generated  */
    unsigned int micros_left = len-w_offset;
    /* Average amount of microseconds between two transitions in main waveform*/
    unsigned int micros;
    /* Microseconds waveform spends on after a transition from OFF to ON  */
    unsigned int micros_on;
    /* Microseconds waveform spends off after a transition from ON to OFF  */
    unsigned int micros_off;
    /* Return value of this function  */
    wavegen_info_t info;

    /* These are used later in the wave combination phase, but they are defined
       here in order to conform to standards  */
    unsigned int wIn1Counter, wIn2Counter, wOutCounter, elapsed;
    unsigned int wIn1Delay, wIn2Delay;

    unsigned int i = 0;
    unsigned int p = 0;

    /* Avoid possible division by zero errors  */
    vWidth = (vWidth) ? vWidth : 1;  /* vWidth cannot be 0  */
    tWidth = (tWidth) ? tWidth : 1;  /* tWidth cannot be 0  */

    /* If frequency is 0 or duty cycle is 0 or 1, construct empty waveform  */
    if (!freq || duty <= 0 || duty >= 1) {
        wIn2[0].gpioOn  = 0;
        wIn2[0].gpioOff = 1<<pin;
        wIn2[0].usDelay = micros_left;
        info.w_offset = 0;
        info.w_on     = 1;
        info.v_offset = 0;
        info.t_offset = 0;
        info.length = 1;
        info.micros = micros_left;
    }

    /* Otherwise, construct waveform normally  */
    else {
        /* Add in the offset if required  */
        if (w_offset) {
            if ((p&1) == (w_on&1)) { /* transition is from OFF to ON  */
                wIn2[0].gpioOn  = 1<<pin;
                wIn2[0].gpioOff = 0;
                wIn2[0].usDelay = w_offset;
            } else { /* transition is from ON to OFF  */
                wIn2[0].gpioOn  = 0;
                wIn2[0].gpioOff = 1<<pin;
                wIn2[0].usDelay = w_offset;
            }
            i++;
        }

        /* Generate the main waveform  */
        while (vfac <= value) {
            /* Recalculation of values  */
            ffac = len - micros_left;
            ffac = dmax(dmin(ffac, freqDelayE), freqDelayS) - freqDelayS;
            ffac /= freqDelayE - freqDelayS;
            dfac = len - micros_left;
            dfac = dmax(dmin(dfac, dutyDelayE), dutyDelayS) - dutyDelayS;
            dfac /= dutyDelayE - dutyDelayS;
            freq = interpolateFreq(freqS, freqE, ffac);
            freq = vibrato(freq, vIntensity, vWidth, len-micros_left+v_offset);
            duty = interpolateDuty(dutyS, dutyE, dfac);
            duty = tremolo(duty, tIntensity, tWidth, len-micros_left+t_offset);
            micros     = 1000000/(2*freq);
            micros_on  = 2*micros*duty;
            micros_off = 2*micros-micros_on;

            if ((p&1) != (w_on&1)) { /* transition is from OFF to ON  */
                wIn2[i].gpioOn  = 1<<pin;
                wIn2[i].gpioOff = 0;
                wIn2[i].usDelay = micros_on;
                micros_left -= micros_on;
                if (micros_left < micros_off) {
                    info.w_offset = micros_off-micros_left;
                    break;
                }
            } else { /* transition is from ON to OFF  */
                wIn2[i].gpioOn  = 0;
                wIn2[i].gpioOff = 1<<pin;
                wIn2[i].usDelay = micros_off;
                micros_left -= micros_off;
                if (micros_left < micros_on) {
                    info.w_offset = micros_on-micros_left;
                    break;
                }
            }
            i++;
            p++;
            vfac = (double)(len-micros_left)/len;
        }

        /* Add in remaining microseconds to make waveform the correct length  */
        info.v_offset = (len-micros_left+v_offset) % vWidth;
        info.t_offset = (len-micros_left+t_offset) % tWidth;
        if (micros_left && vfac <= value) {
            /* Recalculation of values  */
            ffac = len-micros_left;
            ffac = dmax(dmin(ffac,freqDelayE),freqDelayS)-freqDelayS;
            ffac /= freqDelayE - freqDelayS;
            dfac = len-micros_left;
            dfac = dmax(dmin(dfac,dutyDelayE),dutyDelayS)-dutyDelayS;
            dfac /= dutyDelayE - dutyDelayS;
            freq = interpolateFreq(freqS, freqE, ffac);
            freq = vibrato(freq, vIntensity, vWidth, len-micros_left+v_offset);
            duty = interpolateDuty(dutyS, dutyE, dfac);
            duty = tremolo(duty, tIntensity, tWidth, len-micros_left+t_offset);
            micros     = 1000000/(2*freq);
            micros_on  = 2*micros*duty;
            micros_off = 2*micros-micros_on;

            i++;
            p++;
            if ((p&1) != (w_on&1)) { /* transition is from OFF to ON  */
                wIn2[i].gpioOn  = 1<<pin;
                wIn2[i].gpioOff = 0;
                wIn2[i].usDelay = micros_left;
            } else { /* transition is from ON to OFF  */
                wIn2[i].gpioOn  = 0;
                wIn2[i].gpioOff = 1<<pin;
                wIn2[i].usDelay = micros_left;
            }
        }
        else if (micros_left) {
            p    = 0;
            w_on = 0;
            info.w_offset   = 0;
            wIn2[i].gpioOn  = 0;
            wIn2[i].gpioOff = 1<<pin;
            wIn2[i].usDelay = micros_left;
        }
        info.w_on = (p&1) == (w_on&1);
        info.length = ++i;
        info.micros = len;
    }

    /* If the generated waveform has no tail, its "tail" may be calculated as
       having the same length as a whole transition. If this happens the
       w_on property will be miscalculated, so we must set the tail length to 0.
    */
    if (info.w_offset == micros)
        info.w_offset = 0;

    /* Combine waveform with other previously added ones  */

    /* This is the first waveform  */
    if (firstWave) {
        /* Recalculate combined waveform length  */
        wOutLength = info.length;
        firstWave = 0;

        /* Copy contents of wIn2 into wOut  */
        memcpy(wOut, wIn2, info.length*sizeof(pulse_t));
    }

    /* Other waveforms have been added before this one  */
    else {
        /* Copy contents of wOut into wIn1  */
        memcpy(wIn1, wOut, wOutLength*sizeof(pulse_t));
        /* Delete contents of wOut  */
        memset(wOut, 0, PAGES*64*sizeof(pulse_t));

        /* Array index counters  */
        wIn1Counter = 0;
        wIn2Counter = 0;
        wOutCounter = 0;

        /* Microseconds of waveform that has been combined  */
        elapsed = 0;

        /* Microseconds of wIn1 and wIn2 that have been added  */
        wIn1Delay = 0;
        wIn2Delay = 0;

        while (wIn1Counter <= wOutLength && wIn2Counter <= info.length) {
            /* A transition in wIn2 happens at the same time
               as a transition in wIn1.
               If this happens we insert both the wIn2 transition and the
               wIn1 transition.  */
            if (wIn1Delay == wIn2Delay) {
                /* Add the delay for the previous transition we inserted  */
                if (elapsed < wIn1Delay) {
                    wOut[wOutCounter-1].usDelay += wIn1Delay - elapsed;
                    elapsed = wIn1Delay;
                }

                /* Insert the wIn1 transition first  */
                wOut[wOutCounter].gpioOn  = wIn1[wIn1Counter].gpioOn;
                wOut[wOutCounter].gpioOff = wIn1[wIn1Counter].gpioOff;

                /* Recalculate index values  */
                wIn1Delay = elapsed + wIn1[wIn1Counter].usDelay;
                wIn1Counter++;
                wOutCounter++;

                /* Add the delay for the previous transition we inserted  */
                if (elapsed < wIn2Delay) {
                    wOut[wOutCounter-1].usDelay += wIn2Delay - elapsed;
                    elapsed = wIn2Delay;
                }

                /* Then insert the wIn2 transition  */
                wOut[wOutCounter].gpioOn  = wIn2[wIn2Counter].gpioOn;
                wOut[wOutCounter].gpioOff = wIn2[wIn2Counter].gpioOff;

                /* Recalculate index values  */
                wIn2Delay = elapsed + wIn2[wIn2Counter].usDelay;
                wIn2Counter++;
                wOutCounter++;
            }

            /* A transition in wIn2 happens before the next
               transition in wIn1.
               If this happens we insert the wIn2 transition but not the
               wIn1 transition.  */
            else if (wIn2Delay < wIn1Delay) {
                /* Add the delay for the previous transition we inserted  */
                if (elapsed < wIn2Delay) {
                    wOut[wOutCounter-1].usDelay += wIn2Delay - elapsed;
                    elapsed = wIn2Delay;
                }

                /* Insert the wIn2 transition  */
                wOut[wOutCounter].gpioOn  = wIn2[wIn2Counter].gpioOn;
                wOut[wOutCounter].gpioOff = wIn2[wIn2Counter].gpioOff;

                /* Recalculate index values  */
                wIn2Delay = elapsed + wIn2[wIn2Counter].usDelay;
                wIn2Counter++;
                wOutCounter++;
            }

            /* A transition in wIn1 happens before the next
               transition in wIn2.
               If this happens we insert the wIn1 transition but not the
               wIn2 transition.  */
            else {
                /* Add the delay for the previous transition we inserted  */
                if (elapsed < wIn1Delay) {
                    wOut[wOutCounter-1].usDelay += wIn1Delay - elapsed;
                    elapsed = wIn1Delay;
                }

                /* Insert the wIn1 transition  */
                wOut[wOutCounter].gpioOn  = wIn1[wIn1Counter].gpioOn;
                wOut[wOutCounter].gpioOff = wIn1[wIn1Counter].gpioOff;

                /* Recalculate index values  */
                wIn1Delay = elapsed + wIn1[wIn1Counter].usDelay;
                wIn1Counter++;
                wOutCounter++;
            }
        }

        /* Recalculate combined waveform length  */
        wOutLength = wOutCounter;
    }

    return info;
}


/*############################################################################*/


/* Transmit all queued waveforms. Deletes queued waveforms upon being run.
   Please note that if no control blocks are available for the waveform,
   this function sleeps until enough can be made available, and then adds it. */
static void waveTransmit(void) {
    int dmaRunning = dma_running();

    unsigned int wave_index = 0;
    if (!dmaRunning) cmd_index = 0;
    if (!dmaRunning) cbs_index = 0;

    /* No control blocks left to accomodate new waveform? No problem!
       Just recycle old unused control blocks, starting from the beginning  */
    if (cmd_index + wOutLength >= PAGES*64) {
        /* Point last written control block back to first control block  */
        cbs_v[cbs_index-1].nextconbk = (unsigned int)&cbs_b[0];

        /* Wait until DMA reads first control block of this iteration
           (so as to prevent writing over unread control blocks)  */
        while (cbs_laps == dma_laps + 1) {
            /* Increment dma_laps if current DMA index < previous DMA index
               (because it means DMA went back to first control block)  */
            if (dmaRunning) dma_last = dma_current_cb();
            usleep(1000);
            if (dma_current_cb() < dma_last) dma_laps++;
        }

        /* Reset indices to point back to first control block  */
        cmd_index = 0;
        cbs_index = 0;

        /* Increment cbs_laps  */
        cbs_laps++;
    }

    /* Prevent DMA from stopping when it reaches last written control block  */
    if (cbs_index)
        cbs_v[cbs_index-1].nextconbk = (unsigned int)&cbs_b[cbs_index];

    /* Manually create each control block using info from wOut  */
    for (; wave_index < wOutLength; cmd_index++, wave_index++) {
        /* Wait until DMA has read this control block before recycling it
           (so as to prevent writing over unread control blocks)  */
        while (cbs_laps == dma_laps + 1 && dma_current_cb() <= cbs_index) {
            /* Increment dma_laps if current DMA index < previous DMA index
               (because it means DMA went back to first control block)  */
            if (dmaRunning) dma_last = dma_current_cb();
            usleep(1000);
            if (dma_current_cb() < dma_last) dma_laps++;
        }

        /* Copy over the GPIO on/off commands from wOut for DMA to read  */
        cmdV[cmd_index]  = wOut[wave_index].gpioOn;
        cmdV[cmd_index] |= wOut[wave_index].gpioOff;

        /* Turn GPIO on/off  */
        if (wOut[wave_index].gpioOn)
            cbs_v[cbs_index].dest_ad = periph(GPIO_BASE, GPIO_SET);
        else
            cbs_v[cbs_index].dest_ad = periph(GPIO_BASE, GPIO_CLR);
        cbs_v[cbs_index].ti          =  TIBASE;
        cbs_v[cbs_index].source_ad   =  (unsigned int)&cmdB[cmd_index];
        cbs_v[cbs_index].txfr_len    =  4;
        cbs_v[cbs_index].nextconbk   =  (unsigned int)&cbs_b[cbs_index+1];
        cbs_index++;

        /* Delay  */
        cbs_v[cbs_index].ti          =  TIBASE | CB_DEST_DREQ | CB_PERMAP(5);
        cbs_v[cbs_index].source_ad   =  (unsigned int)&cmdB[0];
        cbs_v[cbs_index].dest_ad     =  periph(PWM_BASE, PWM_FIF1);
        cbs_v[cbs_index].txfr_len    =  4 * wOut[wave_index].usDelay;
        cbs_v[cbs_index].nextconbk   =  (unsigned int)&cbs_b[cbs_index+1];
        cbs_index++;
    }
    /* Cause DMA to stop when it reaches last written control block  */
    cbs_v[cbs_index-1].nextconbk = 0;

    if (!dmaRunning) activate_dma(0);

    /* Consume previous waveforms  */
    wOutLength = 0;
    firstWave = 1;
}


/*############################################################################*/


/* Add a voice to the queue.
   pin:    GPIO pin number (BCM) through which the voice plays.
   freqs:  Array of frequencies (Hz). A zero (0) indicates pin should be off.
   duties: Array of duty cycles (0 to 1, exclusive).
   misc:   Array of misc_t pointers containing extra data. This may be NULL.  */
void queueAdd(int pin, double *freqs, double *duties, misc_t **misc) {
    pins      |= 1<<pin;
    _freq[pin] = freqs;
    _duty[pin] = duties;
    _misc[pin] = misc;
}


/*############################################################################*/


/* Play queue. This function also consumes the queue.
   us:    Length of each beat in microseconds (60000000/BPM).
   beats: Total number of queued beats.  */
void queuePlay(unsigned int us, unsigned int beats) {
    unsigned int beat;
    unsigned int pin;
    unsigned int _pins;

    static double value;
    static double freqAS[32], freqAE[32];
    static double dutyAS[32], dutyAE[32];
    static char ifc = 0;
    static unsigned int ff = 0, fd = 0;
    static double initF[32], endF[32];
    static double initD[32], endD[32];
    static double facF[32], facD[32];
    static double freqFrom[32], freqTo[32], _freqTo[32], freqRS[32], freqRE[32];
    static double dutyFrom[32], dutyTo[32], _dutyTo[32], dutyRS[32], dutyRE[32];
    static double vIntensity[32], _vIntensity[32];
    static double tIntensity[32], _tIntensity[32];
    static unsigned int vWidth[32], _vWidth[32];
    static unsigned int tWidth[32], _tWidth[32];
    static unsigned int changeUs = 0;

    /* Setup DMA, allocate pages for control blocks  */
    regtool_setup(PAGES);

    /* Make pages for DMA to receive GPIO commands from  */
    cmdH = vc_create((void **)&cmdV, (void **)&cmdB, PAGES);

    /* Set initial "w_offset" value to 0, initial "w_on" value to 1,
       initial "t_offset" and "v_offset" values to 0 and
       initial intensity and width values to 0  */
    for (pin = 0; pin < 32; pin++) {
        _info[pin].v_offset = 0;
        _info[pin].t_offset = 0;
        _vIntensity[pin]    = 0;
        _vWidth[pin]        = 0;
        _tIntensity[pin]    = 0;
        _tWidth[pin]        = 0;
        _info[pin].w_offset = 0;
        _info[pin].w_on     = 1;
    }

    /* This loops through each beat. Generates one waveform per beat.  */
    for (beat = 0; beat < beats; beat++) {
        /* Change global beat length if it was requested  */
        if (changeUs) {
            us = changeUs;
            changeUs = 0;
        }
        /* This loops through each pin. Run waveGen() once for each pin
           in order to produce one combined waveform on several pins.  */
        for (_pins = pins, pin = 0; pin < 32; _pins >>= 1, pin++) {
            if (_pins&1) {
                freqFrom[pin]=(ff&pins&(1<<pin))?_freqTo[pin]:_freq[pin][beat];
                freqTo[pin]  = _freq[pin][beat];
                freqRS[pin]  = 0;
                freqRE[pin]  = us;
                dutyFrom[pin]=(fd&pins&(1<<pin))?_dutyTo[pin]:_duty[pin][beat];
                dutyTo[pin]  = _duty[pin][beat];
                dutyRS[pin]  = 0;
                dutyRE[pin]  = us;
                value        = 1;

                /* Record properties from misc  */
                /* Check if there are misc properties for current pin, beat  */
                ifc = _misc[pin] && _misc[pin][beat];
                /* If the note value is defined and non-zero, change
                   the note value from its default value of 1.  */
                if (ifc&&_misc[pin][beat]->value)
                    value = _misc[pin][beat]->value;
                /* If the usingPs property is on or if pitch slide is
                   already on, adjust frequency to correspond  */
                if ((ifc&&_misc[pin][beat]->usingPs)||(ff&pins&(1<<pin))) {
                    /* If this is the first beat of the pitch slide  */
                    if (!(ff&pins&(1<<pin))) {
                        ff |= 1<<pin;
                        /* Record initial frequency  */
                        initF[pin]  = _freq[pin][beat];
                        /* Record desired ending frequency  */
                        endF[pin]   = _misc[pin][beat]->freqTo;
                        /* Relative microseconds offset of slide start  */
                        freqRS[pin] = _misc[pin][beat]->freqS * us;
                        /* Relative microseconds offset of slide end  */
                        freqRE[pin] = _misc[pin][beat]->freqE * us;
                        /* Amount of beats from start of song of slide start  */
                        freqAS[pin] = _misc[pin][beat]->freqS + beat;
                        /* Amount of beats from start of song of slide end  */
                        freqAE[pin] = _misc[pin][beat]->freqE + beat;
                    }
                    /* Compute where the ending frequency of the current beat
                       should be between the starting and ending frequencies
                       of the entire pitch slide, expressed as a
                       double between 0 and 1  */
                    facF[pin] = (beat+1-freqAS[pin])/(freqAE[pin]-freqAS[pin]);
                    facF[pin] = dmax(dmin(facF[pin],1),0);
                    /* Using this data compute the actual ending frequency for
                       this beat  */
                    freqTo[pin] = endF[pin];
                    freqTo[pin] =
                        interpolateFreq(initF[pin],freqTo[pin],facF[pin]);
                    /* Record this ending frequency for later use, as it is the
                       starting frequency for the next beat  */
                    _freqTo[pin] = freqTo[pin];
                    /* If the factor is 1 (indicating the end of the slide
                       occurred somewhere within the current beat) stop
                       doing frequency slide  */
                    if (facF[pin] >= 1 && (ff&pins&(1<<pin))) ff&=~(1<<pin);
                }
                /* If the usingDs property is on or if dutycycle slide is
                   already on, adjust dutycycle to correspond  */
                if ((ifc&&_misc[pin][beat]->usingDs)||(fd&pins&(1<<pin))) {
                    /* If this is the first beat of the dutycycle slide  */
                    if (!(fd&pins&(1<<pin))) {
                        fd |= 1<<pin;
                        /* Record initial dutycycle  */
                        initD[pin]  = _duty[pin][beat];
                        /* Record desired ending dutycycle  */
                        endD[pin]   = _misc[pin][beat]->dutyTo;
                        /* Relative microseconds offset of slide start  */
                        dutyRS[pin] = _misc[pin][beat]->dutyS * us;
                        /* Relative microseconds offset of slide end  */
                        dutyRE[pin] = _misc[pin][beat]->dutyE * us;
                        /* Amount of beats from start of song of slide start  */
                        dutyAS[pin] = _misc[pin][beat]->dutyS + beat;
                        /* Amount of beats from start of song of slide end  */
                        dutyAE[pin] = _misc[pin][beat]->dutyE + beat;
                    }
                    /* Compute where the ending dutycycle of the current beat
                       should be between the starting and ending dutycycle
                       of the entire dutycycle slide, expressed as a
                       double between 0 and 1  */
                    facD[pin] = (beat+1-dutyAS[pin])/(dutyAE[pin]-dutyAS[pin]);
                    facD[pin] = dmax(dmin(facD[pin],1),0);
                    /* Using this data compute the actual ending dutycycle for
                       this beat  */
                    dutyTo[pin] = endD[pin];
                    dutyTo[pin] =
                        interpolateFreq(initD[pin],dutyTo[pin],facD[pin]);
                    /* Record this ending dutycycle for later use, as it is the
                       starting dutycycle for the next beat  */
                    _dutyTo[pin] = dutyTo[pin];
                    /* If the factor is 1 (indicating the end of the slide
                       occurred somewhere within the current beat) stop
                       doing dutycycle slide  */
                    if (facD[pin] >= 1 && (fd&pins&(1<<pin))) fd&=~(1<<pin);
                }
                /* If the usingV property is on, modify vibrato parameters  */
                if (ifc&&_misc[pin][beat]->usingV) {
                    vIntensity[pin]  = _misc[pin][beat]->vInt;
                    vWidth[pin]      = _misc[pin][beat]->vWth;
                    _vIntensity[pin] = vIntensity[pin];
                    _vWidth[pin]     = vWidth[pin];
                }
                /* If the usingV property is off, restore vibrato parameters  */
                else {
                    vIntensity[pin] = _vIntensity[pin];
                    vWidth[pin]     = _vWidth[pin];
                }
                /* If the usingT property is on, modify tremolo parameters  */
                if (ifc&&_misc[pin][beat]->usingT) {
                    tIntensity[pin]  = _misc[pin][beat]->tInt;
                    tWidth[pin]      = _misc[pin][beat]->tWth;
                    _tIntensity[pin] = tIntensity[pin];
                    _tWidth[pin]     = tWidth[pin];
                }
                /* If the usingT property is off, restore tremolo parameters  */
                else {
                    tIntensity[pin] = _tIntensity[pin];
                    tWidth[pin]     = _tWidth[pin];
                }
                /* If the us property is non-zero, change the global beat length
                   next beat  */
                if (ifc&&_misc[pin][beat]->us)
                    changeUs = _misc[pin][beat]->us;

                /* Set GPIO pin mode to output  */
                gpio_mode(pin, OUT);
                /* Run waveGen()  */
                _info[pin] = waveGen(
                    pin,                             /*       int pin         */
                    freqFrom[pin],                   /*    double freqS       */
                    freqTo[pin],                     /*    double freqE       */
                    dmin(us, freqRS[pin]),           /*  unsigned freqDelayS  */
                    dmin(us, freqRE[pin]),           /*  unsigned freqDelayE  */
                    dutyFrom[pin],                   /*    double dutyS       */
                    dutyTo[pin],                     /*    double dutyE       */
                    dmin(us, dutyRS[pin]),           /*  unsigned dutyDelayS  */
                    dmin(us, dutyRE[pin]),           /*  unsigned dutyDelayE  */
                    vIntensity[pin],                 /*    double vIntensity  */
                    vWidth[pin],                     /*  unsigned vWidth      */
                    tIntensity[pin],                 /*    double tIntensity  */
                    tWidth[pin],                     /*  unsigned tWidth      */
                    us,                              /*  unsigned len         */
                    value,                           /*    double value       */
                    _info[pin].v_offset,             /*  unsigned v_offset    */
                    _info[pin].t_offset,             /*  unsigned t_offset    */
                    _info[pin].w_offset,             /*  unsigned w_offset    */
                    _info[pin].w_on);                /*      char w_on        */
            }
        }

        /* Run waveTransmit() to send the combined waveform to DMA.
           This function sometimes unpredictably sleeps on its own.  */
        waveTransmit();
    }

    /* Sleep for remaining amount of time until DMA stops  */
    while (dma_running()) usleep(1000);

    /* Ensure that DMA has stopped  */
    stop_dma();

    /* Turn GPIO pins off  */
    for (_pins = pins, pin = 0; pin < 32; _pins >>= 1, pin++)
        if (_pins&1) gpio_write(pin, 0);

    /* Consume queue  */
    pins       = 0;
    cbs_index  = 0;
    cmd_index  = 0;
    dma_laps   = 0;
    dma_last   = 0;
    cbs_laps   = 0;
    wOutLength = 0;
    firstWave  = 1;

    /* Free resources  */
    vc_destroy(cmdH, cmdV, PAGES);
    regtool_cleanup();
}


/*############################################################################*/
