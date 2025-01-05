#include "captains.h"


void launchHabourCaptain(int shmid, int semid) {
    SharedMemory *sm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (sm == (void *)-1) {
        perror("shmat habour captain");
        exit(EXIT_FAILURE);
    }

    printf("=== Habour Captain === Starting\n");

    char c;
    while (1) {
        printf("=== Habour Captain === Enter: w = early cruise, k = end of day, q = exit\n");
        c = getchar();
        if (c == '\n') continue;

        if (c == 'w') {
            kill(sm->shipCaptainPid, SIGUSR1); // Sig 1
            printf("=== Habour Captain === Early departure signal sent.\n");
        } else if (c == 'k') {
            kill(sm->shipCaptainPid, SIGUSR2); // Sig 2
            printf("=== Habour Captain === End-of-day signal sent\n");
        } else if (c == 'q') {
            printf("=== Habour Captain === Exiting\n");
            break;
        } else {
            printf("=== Habour Captain === Unknown command\n");
        }

        while (getchar() != '\n'); // Clearing buff
    }

    shmdt(sm);
}


