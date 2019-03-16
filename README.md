
# rpi-player

### Description
A collection of programs written in C that demonstrate the playing of music (PWM waves) through a passive piezo buzzer (or passive speaker) through the Raspberry Pi's GPIO pins.

Also includes two examples of interacting with the Raspberry Pi's DMA engine using regtool.c.

## Highlights
* Hardware-timed playing through any of the GPIO pins on the 40-pin header
* Hardware-timed playing through several GPIO pins simultaneously (and thus the ability to produce simultaneous tones)
* Any frequency from 0-20000 Hz (including non-integer frequencies)
* Ability to change timbre of sound by changing duty cycle
* Much improved sound quality and tuning over software-timed PWM programs such as [the Imperial March buzzer example from here](https://www.youtube.com/watch?v=j8HnKM58QXk)
* Support for frequency ramps and duty cycle ramps (i.e. "pitch slide" and "timbre slide")
* Support for vibrato and tremolo effects

## Programs Included
**ex-helloworld.c** - Example using regtool.c showing how to copy text from one part of the memory to another part of the memory using DMA engine.

**ex-wave.c** - Example using regtool.c showing production of square wave on a GPIO pin using DMA engine. By default the square wave is produced on **GPIO 21 at 440 Hz**, however this may be changed inside the file (near the top).

**ex-player.c** - Very simple example using the player.c library to play chords through 4 GPIO pins simultaneously. By default it plays through **GPIO 21, 20, 16, 13**, but this may be changed inside the file.

There will be more programs (actual music) and videos within the next few months.

## Installation
Ensure that you have installed the programs gcc and make. They are most likely installed by default, but just to be sure:
```
sudo apt-get update
sudo apt-get install gcc make
```
Now download this repository:
```
cd
git clone https://github.com/hxxr/rpi-player.git
cd rpi-player
```
To compile the programs you must run the correct command based on the hardware revision of your Raspberry Pi:
```
make pi0    # Raspberry Pi Zero or Zero W
make pi1    # Raspberry Pi 1
make pi2    # Raspberry Pi 2
make pi3    # Raspberry Pi 3
```
**Please note: I only have a Raspberry Pi 2, so I cannot guarantee the programs will function on the other hardware revisions. However I have reason to believe they will function.**

The programs are now compiled and can be run like any other program. However they must be run using sudo. For example:
```
sudo ./ex-player
```
\
\
Later, in case you want to remove the compiled files you may run this command from inside of your copy of the repository:
```
make clean
```
## Wiring Setup
The diagram below shows how it is possible to set up 4 GPIO pins for use with this program to simultaneously control one piezo buzzer or speaker (requires 1 passive speaker/buzzer, 1 9V battery, 4 NPN BJTs, 8 10 ohm resistors, 4 LEDs, although the LEDs may be omitted). The VOICE wires represent GPIO pins, while GND is one of the Raspberry Pi's ground pins.
![The wiring setup!](http://oi68.tinypic.com/f52k45.jpg)
\
\
\
\
\
\
\
\
\
.
## How to Write a Program
**C programs written by the user which use player.c or regtool.c should be placed inside the root directory of this repository. To compile them simply run `make` again. The Makefile automatically scans for new C source files.**

### Basic Usage
player.c contains two functions, `queueAdd()` which loads notes to the player queue, and `queuePlay()` which plays whatever has been loaded into the queue. They are declared as such inside of player.h:
```
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
```
A program using these two functions must include player.h:
```
#include "include/player.h"

/* ...  */
```
To define notes to be played, you need to create an array of type `double` for each GPIO pin you want to use. Each element of the array is the frequency (Hz) for that beat. If you do not want anything to be played in that beat, set it to 0. The frequency arrays for each pin need to be the same length. This is an example of a frequency array for 4 GPIO pins:
```
#include "include/player.h"

double freq1[] = {c4, d4, e4, f4, g4, g4, g4};
double freq2[] = {c5, g4, g4, a4, b4, b4, b4};
double freq3[] = {g3, a3, b3, c4, d4, d4, d4};
double freq4[] = {g4, a4, b4, c5, d5, d5, d5};

/* ...  */
```
The frequencies of musical notes are defined inside of player.h. To produce a natural note use lowercase letters (for example the frequency of C natural in octave 4 is "c4"). To produce sharps use uppercase letters (for example the frequency of C sharp in octave 4 is "C4"). The player.h header file also defines "__" as 0, so you may use it to fill in areas where the pin is to be off.
\
You also need to define another array of type `double` for each pin that contains the duty cycles for each beat. The duty cycle is the percentage of the time the sound wave spends on. It controls the timbre (tone colour) of the sound. It varies between 0 and 1 (exclusive) where 0 is off all the time and 1 is on all the time. As such setting the duty cycle to 0.5 creates a square wave. Lowering the duty cycle appears to have the same effect (acoustically) as raising the duty cycle, it causes the sound to become more similar to a triangle or sawtooth wave.
```
#include "include/player.h"

double freq1[] = {c4, d4, e4, f4, g4, g4, g4};
double freq2[] = {c5, g4, g4, a4, b4, b4, b4};
double freq3[] = {g3, a3, b3, c4, d4, d4, d4};
double freq4[] = {g4, a4, b4, c5, d5, d5, d5};

double duty1[] = {.5, .5, .5, .5, .5, .5, .2};
double duty2[] = {.5, .5, .5, .5, .5, .5, .2};
double duty3[] = {.5, .5, .5, .5, .5, .5, .2};
double duty4[] = {.5, .5, .5, .5, .5, .5, .2};

/* ...  */
```
Now in the `main()` function you must run `queueAdd()` several times, once for each pin. This is the declaration of `queueAdd()`:
```
void queueAdd(int pin, double *freqs, double *duties, misc_t **misc);
```
The first argument is the pin number (BCM). The second argument is the frequency array for that pin. The third argument is the duty cycle array for that pin. The fourth argument is an array containing miscellaneous effects (pitch slide, vibrato, etc...) which may be (and will be in this example) set to NULL if unused. NULL is defined inside of player.h if it is not defined elsewhere.
```
#include "include/player.h"

double freq1[] = {c4, d4, e4, f4, g4, g4, g4};
double freq2[] = {c5, g4, g4, a4, b4, b4, b4};
double freq3[] = {g3, a3, b3, c4, d4, d4, d4};
double freq4[] = {g4, a4, b4, c5, d5, d5, d5};

double duty1[] = {.5, .5, .5, .5, .5, .5, .2};
double duty2[] = {.5, .5, .5, .5, .5, .5, .2};
double duty3[] = {.5, .5, .5, .5, .5, .5, .2};
double duty4[] = {.5, .5, .5, .5, .5, .5, .2};

int main(void) {
    queueAdd(21, freq1, duty1, NULL);
    queueAdd(20, freq2, duty2, NULL);
    queueAdd(16, freq3, duty3, NULL);
    queueAdd(13, freq4, duty4, NULL);

    /* ...  */

    return 0;
}
```
Finally, run `queuePlay()` inside of `main()` in order to play the data stored in the queue. The declaration of `queuePlay()` is as such:
```
void queuePlay(unsigned int us, unsigned int beats);
```
The first argument is the length of each beat in microseconds, which may be calculated using the BPM by dividing 60000000 (sixty million) by the BPM. The second argument is the total number of beats, which is the length of any of the frequency or duty cycle arrays.
```
#include "include/player.h"

double freq1[] = {c4, d4, e4, f4, g4, g4, g4};
double freq2[] = {c5, g4, g4, a4, b4, b4, b4};
double freq3[] = {g3, a3, b3, c4, d4, d4, d4};
double freq4[] = {g4, a4, b4, c5, d5, d5, d5};

double duty1[] = {.5, .5, .5, .5, .5, .5, .2};
double duty2[] = {.5, .5, .5, .5, .5, .5, .2};
double duty3[] = {.5, .5, .5, .5, .5, .5, .2};
double duty4[] = {.5, .5, .5, .5, .5, .5, .2};

int main(void) {
    queueAdd(21, freq1, duty1, NULL);
    queueAdd(20, freq2, duty2, NULL);
    queueAdd(16, freq3, duty3, NULL);
    queueAdd(13, freq4, duty4, NULL);

    queuePlay(1000000, 7);

    return 0;
}
```
The program is now complete. To compile this program:
* Save the code above into a new file named "test.c".
* Move the file into the root directory of the downloaded version of this repository (by default at `~/rpi-player/`).
* `cd` into the root directory of the downloaded version of this repository (by default at `~/rpi-player/`).
    ```
    cd ~/rpi-player/    # Or wherever it is...
    ```
* Compile by running `make` again, choosing the correct command for your hardware revision. This project's Makefile is designed to detect the presence of new C source files and accomodate for them.
    ```
    make pi0    # Raspberry Pi Zero or Zero W
    make pi1    # Raspberry Pi 1
    make pi2    # Raspberry Pi 2
    make pi3    # Raspberry Pi 3
    ```

### Addendum 1: Usage of miscellaneous effects (pitch slide, vibrato, etc...)
To use miscellaneous effects you are required define a variable of type `misc_t` for each specific effect type you are to use in your program.
The following miscellaneous effects are supported:
* Pitch slide
* Duty cycle slide
* Vibrato (oscillating pitch slide)
* Tremolo (oscillating duty cycle slide)
* Staccato (shortened note duration)

\
A `misc_t` effect can be created using the following code template:
```
misc_t mc = {
    1,      /* Length of note, in beats. (<= 1)                               */

    0,      /* Whether we are using pitch slide.                              */
    0,      /* If using pitch slide, desired ending frequency.                */
    0,      /* If using pitch slide, start offset in beats. (<= 1)            */
    0,      /* If using pitch slide, end offset in beats.                     */

    0,      /* Whether we are using dutycycle slide.                          */
    0,      /* If using dutycycle slide, desired ending dutycycle.            */
    0,      /* If using dutycycle slide, start offset in beats. (<= 1)        */
    0,      /* If using dutycycle slide, end offset in beats.                 */

    0,      /* Whether we are modifying vibrato settings.                     */
    0,      /* If modifying, vibrato range in cents.                          */
    0,      /* If modifying, length of vibrato pulse in microseconds.         */

    0,      /* Whether we are modifying tremolo settings.                     */
    0,      /* If modifying, tremolo range.                                   */
    0       /* If modifying, length of tremolo pulse in microseconds.         */
};
```
The above template shows all of the effects off. Below is a rigorous explanation of the effect of each field within the `misc_t`:
1. `(double)`  Length of note in beats. This must be more than 0 and less than or equal to 1. For the note to play for the entire duration of the beat set this to 1, otherwise set this to a number of lesser value.
\
...
2. `(char)` 1 if pitch slide effect is to be used, otherwise 0.
3. `(double)` If pitch slide is to be used, desired ending frequency for the pitch slide. The starting frequency of the pitch slide is read from the frequency array.
4. `(double)` If pitch slide is to be used, amount of beats before the pitch slide starts. This must be less than 1. If this value is not 0 the pitch slide will start somewhere in the middle of the beat rather than at the very beginning.
5. `(double)` If pitch slide is to be used, amount of beats before the pitch slide ends. This must be more than the fourth value (the amount of beats before the pitch slide starts).
\
...
6. `(char)` 1 if duty cycle slide effect is to be used, otherwise 0.
7. `(double)` If duty cycle slide is to be used, desired ending duty cycle for the duty cycle slide. The starting duty cycle of the duty cycle slide is read from the duty cycle array.
8. `(double)` If duty cycle slide is to be used, amount of beats before the duty cycle slide starts. This must be less than 1. If this value is not 0 the duty cycle slide will start somewhere in the middle of the beat rather than at the very beginning.
9. `(double)` If duty cycle slide is to be used, amount of beats before the duty cycle slide ends. This must be more than the fourth value (the amount of beats before the duty cycle slide starts).
\
...
10.  `(char)` 1 if vibrato settings are to be modified, otherwise 0.
11. `(double)` If vibrato settings are to be modified, vibrato range, in cents (hundredths of a semitone). The pitch of the note will be offset (at maximum) by this many cents by the vibrato.
12. `(unsigned int)` If vibrato settings are to be modified, length of each vibrato pulse, in microseconds.
\
...
13.  `(char)` 1 if tremolo settings are to be modified, otherwise 0.
14. `(double)` If tremolo settings are to be modified, tremolo range, in the ordinary 0-1 units. The duty cycle of the note will be offset (at maximum) by this many units by the tremolo.
15. `(unsigned int)` If tremolo settings are to be modified, length of each tremolo pulse, in microseconds.

Afterwards you must define (in addition to the frequency and duty cycle arrays) an additional array for each pin, this time of type `misc_t *`. Each element of the array is a pointer to a `misc_t`, or `NULL` if no effects are to be used for that beat. The player.h header file defines "___" as NULL.
```
misc_t *misc1[] = {&mc,___,___,___,___,___,___};
```
This array must then be added to the queue with the frequency and duty cycle arrays for that pin.
```
/* ...  */

int main(void) {
    queueAdd(21, freq1, duty1, misc1);
    queuePlay(1000000, 7);
    return 0;
}
```

Below is an example program that plays a pitch slide from c4 to c5 on GPIO 21 over 8 beats ("XX" is defined in player.h as 0, which I will use as a placeholder for beats where a pitch slide or duty cycle slide occurs):
```
#include "include/player.h"

misc_t mc = {
    1,      /* Length of note, in beats. (<= 1)                               */

    1,      /* Whether we are using pitch slide.                              */
    c5,     /* If using pitch slide, desired ending frequency.                */
    0,      /* If using pitch slide, start offset in beats. (<= 1)            */
    8,      /* If using pitch slide, end offset in beats.                     */

    0,      /* Whether we are using dutycycle slide.                          */
    0,      /* If using dutycycle slide, desired ending dutycycle.            */
    0,      /* If using dutycycle slide, start offset in beats. (<= 1)        */
    0,      /* If using dutycycle slide, end offset in beats.                 */

    0,      /* Whether we are modifying vibrato settings.                     */
    0,      /* If modifying, vibrato range in cents.                          */
    0,      /* If modifying, length of vibrato pulse in microseconds.         */

    0,      /* Whether we are modifying tremolo settings.                     */
    0,      /* If modifying, tremolo range.                                   */
    0       /* If modifying, length of tremolo pulse in microseconds.         */
};

double freq1[]  =  {c4, XX, XX, XX, XX, XX, XX, XX};
double duty1[]  =  {.5, .5, .5, .5, .5, .5, .5, .5};
misc_t *misc1[] = {&mc,___,___,___,___,___,___,___};

int main(void) {
    queueAdd(21, freq1, duty1, misc1);
    queuePlay(1000000, 8);
    return 0;
}
```
