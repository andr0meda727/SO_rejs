#include "captains.h"
#include <signal.h>

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

void launchShipCaptain(int shmid, int semid) {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("sigaction SIGUSR2");
        exit(EXIT_FAILURE);
    }
}
