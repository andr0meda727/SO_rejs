#include "captains.h"

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        printf("=== Ship Captain === Early departure signal has been received\n");
        // make some logic later
    } else if (sig == SIGUSR2) {
        printf("=== Ship Captain === The end-of-day signal has been received\n");
        // some logic
        exit(0); // end proccess?
    }
}

void launchShipCaptain(int shmid, int semid) {}
