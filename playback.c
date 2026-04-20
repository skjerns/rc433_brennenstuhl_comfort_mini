// Raw 433 MHz signal playback.
// Reads a recorded .rf file and replays it via the transmitter.
//
// Build:  gcc -O2 -o playback playback.c -lwiringPi
// Run:    sudo ./playback <name> [repeats]
//         Reads from <name>.rf, default 1 repeat (recording already has frames)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <wiringPi.h>

#define TX_PIN     0      // WiringPi pin 0 = BCM 17, standard 433Utils TX pin
#define MAX_PULSES 20000
#define GAP_BETWEEN_REPEATS 50000  // 50 ms gap between full replays (us)

static volatile int running = 1;

static void sighandler(int sig) {
    (void)sig;
    running = 0;
    digitalWrite(TX_PIN, LOW);  // Kill transmitter immediately
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <name> [repeats]\n", argv[0]);
        fprintf(stderr, "Replays raw 433 MHz signal from <name>.rf\n");
        fprintf(stderr, "repeats: number of times to replay (default 1)\n");
        return 1;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "%s.rf", argv[1]);

    int repeats = 1;
    if (argc >= 3) repeats = atoi(argv[2]);
    if (repeats < 1) repeats = 1;

    // Read the capture file
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return 1;
    }

    int *levels = malloc(MAX_PULSES * sizeof(int));
    unsigned int *durations = malloc(MAX_PULSES * sizeof(unsigned int));
    if (!levels || !durations) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), f) && count < MAX_PULSES) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int lev;
        unsigned int dur;
        if (sscanf(line, "%d %u", &lev, &dur) == 2) {
            levels[count] = lev;
            durations[count] = dur;
            count++;
        }
    }
    fclose(f);

    if (count == 0) {
        fprintf(stderr, "No pulses found in %s\n", filename);
        free(levels);
        free(durations);
        return 1;
    }

    printf("Loaded %d pulses from %s\n", count, filename);

    if (wiringPiSetup() == -1) {
        fprintf(stderr, "wiringPiSetup failed\n");
        free(levels);
        free(durations);
        return 1;
    }
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, LOW);
    piHiPri(99);  // Raise priority for accurate timing

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    for (int r = 0; r < repeats && running; r++) {
        if (repeats > 1)
            printf("Sending %d/%d...\n", r + 1, repeats);
        else
            printf("Sending...\n");

        for (int i = 0; i < count && running; i++) {
            digitalWrite(TX_PIN, levels[i] ? HIGH : LOW);
            delayMicroseconds(durations[i]);
        }
        digitalWrite(TX_PIN, LOW);

        if (r < repeats - 1)
            delayMicroseconds(GAP_BETWEEN_REPEATS);
    }

    digitalWrite(TX_PIN, LOW);
    printf("Done.\n");

    free(levels);
    free(durations);
    return 0;
}
