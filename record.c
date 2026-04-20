// Raw 433 MHz signal recorder.
// Records continuously until Enter is pressed or timeout expires.
//
// Build:  gcc -O2 -o record record.c -lwiringPi -lpthread
// Run:    sudo ./record <name> [timeout_sec]
//         Saves to <name>.rf. Default timeout: 5 seconds.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <wiringPi.h>

#define RX_PIN        2       // WiringPi pin 2 = BCM 27, standard 433Utils RX pin
#define MIN_PULSE     100     // Ignore pulses shorter than this (noise filter, us)
#define MAX_PULSES    200000  // Max pulses to record (enough for ~5s of signal)

static volatile int recording = 1;

static void sighandler(int sig) {
    (void)sig;
    recording = 0;
}

// Thread: wait for Enter key
static void *stdin_thread(void *arg) {
    (void)arg;
    getchar();
    recording = 0;
    return NULL;
}

// Thread: timeout
static void *timeout_thread(void *arg) {
    int secs = *(int *)arg;
    for (int i = secs; i > 0 && recording; i--) {
        sleep(1);
    }
    recording = 0;
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <name> [timeout_sec]\n", argv[0]);
        fprintf(stderr, "Records raw 433 MHz pulses and saves to <name>.rf\n");
        fprintf(stderr, "Stops on Enter, Ctrl+C, or timeout (default 5s).\n");
        return 1;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "%s.rf", argv[1]);

    int timeout_sec = 5;
    if (argc >= 3) timeout_sec = atoi(argv[2]);
    if (timeout_sec < 1) timeout_sec = 1;

    if (wiringPiSetup() == -1) {
        fprintf(stderr, "wiringPiSetup failed\n");
        return 1;
    }
    pinMode(RX_PIN, INPUT);
    piHiPri(99);

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    int *levels = malloc(MAX_PULSES * sizeof(int));
    unsigned int *durations = malloc(MAX_PULSES * sizeof(unsigned int));
    if (!levels || !durations) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    printf("Recording on WiringPi pin %d (BCM 27) for up to %ds...\n", RX_PIN, timeout_sec);
    printf("Press the remote button now. Press Enter or Ctrl+C to stop.\n");

    // Start stop-threads
    pthread_t tid_stdin, tid_timeout;
    pthread_create(&tid_stdin, NULL, stdin_thread, NULL);
    pthread_create(&tid_timeout, NULL, timeout_thread, &timeout_sec);

    // Record all transitions continuously
    int count = 0;
    int prev = digitalRead(RX_PIN);

    while (recording && count < MAX_PULSES) {
        unsigned int t0 = micros();

        while (recording && digitalRead(RX_PIN) == prev)
            ;
        if (!recording) break;

        unsigned int dur = micros() - t0;
        levels[count] = prev;
        durations[count] = dur;
        count++;
        prev = !prev;
    }

    printf("\nRecorded %d pulses.\n", count);

    // Filter: trim leading noise (everything before the first sync gap)
    int trim_start = 0;
    for (int i = 0; i < count; i++) {
        if (levels[i] == 0 && durations[i] > 5000) {
            // Found first sync gap - keep a few pulses before it for the sync HIGH
            trim_start = (i > 0) ? i - 1 : i;
            break;
        }
    }

    // Trim trailing noise (everything after the last sync gap + one frame)
    int trim_end = count;
    for (int i = count - 1; i >= trim_start; i--) {
        if (levels[i] == 0 && durations[i] > 5000) {
            // Last sync gap - include one frame worth after it (~70 pulses)
            trim_end = (i + 70 < count) ? i + 70 : count;
            break;
        }
    }

    int trimmed = trim_end - trim_start;
    printf("Trimmed to %d pulses (removed %d leading, %d trailing noise pulses).\n",
           trimmed, trim_start, count - trim_end);

    // Print summary of trimmed data
    unsigned int total_us = 0;
    for (int i = trim_start; i < trim_end; i++)
        total_us += durations[i];
    printf("Duration: %.1f ms\n", total_us / 1000.0);

    // Count sync gaps in trimmed data
    int syncs = 0;
    for (int i = trim_start; i < trim_end; i++)
        if (levels[i] == 0 && durations[i] > 5000)
            syncs++;
    printf("Sync gaps (frames): %d\n", syncs);

    // Save trimmed data
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Cannot open %s for writing\n", filename);
        free(levels);
        free(durations);
        return 1;
    }

    fprintf(f, "# 433 MHz raw capture, %d pulses (trimmed)\n", trimmed);
    fprintf(f, "# Format: level(1=HIGH,0=LOW) duration_us\n");
    for (int i = trim_start; i < trim_end; i++) {
        fprintf(f, "%d %u\n", levels[i], durations[i]);
    }
    fclose(f);

    printf("Saved to %s\n", filename);

    free(levels);
    free(durations);

    // Cancel threads (they may be blocking)
    pthread_cancel(tid_stdin);
    pthread_cancel(tid_timeout);

    return 0;
}
