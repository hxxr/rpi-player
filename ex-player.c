#include "include/player.h"

/* GPIO pins to use (BCM number) */
#define PIN1   21
#define PIN2   20
#define PIN3   16
#define PIN4   13

static double freq1[] = {c4, d4, e4, f4, g4, g4, g4};
static double duty1[] = {.5, .5, .5, .5, .5, .5, .2};

static double freq2[] = {c5, g4, g4, a4, b4, b4, b4};
static double duty2[] = {.5, .5, .5, .5, .5, .5, .2};

static double freq3[] = {g3, a3, b3, c4, d4, d4, d4};
static double duty3[] = {.5, .5, .5, .5, .5, .5, .2};

static double freq4[] = {g4, a4, b4, c5, d5, d5, d5};
static double duty4[] = {.5, .5, .5, .5, .5, .5, .2};

int main(void) {
    queueAdd(PIN1, freq1, duty1, NULL);
    queueAdd(PIN2, freq2, duty2, NULL);
    queueAdd(PIN3, freq3, duty3, NULL);
    queueAdd(PIN4, freq4, duty4, NULL);

    queuePlay(1000000, 7);

    return 0;
}
