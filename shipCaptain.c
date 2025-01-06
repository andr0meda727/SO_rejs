#include "utils.h"

void handle_signal(int sig) {
    SharedMemory *sm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (sm == (void *)-1) {
        perror("shmat in signal handler");
        exit(EXIT_FAILURE);
    }

    if (sig == SIGUSR1) {
        printf("=== Ship Captain === Early departure signal has been received\n");
        sm->signalEarlyVoyage = 1;
    } else if (sig == SIGUSR2) {
        printf("=== Ship Captain === The end-of-day signal has been received\n");
        sm->signalEndOfDay = 1;
    }

    shmdt(sm); // Detach after updating
}

void launchShipCaptain(int shmid, int semid) {
    SharedMemory *sm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (sm == (void *)-1) {
        perror("shmat ship captain");
        exit(EXIT_FAILURE);
    }

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

    printf("=== Ship Captain === Starting\n");

    while (1) {
        int slept = 0;
        while (slept < TIME_BETWEEN_TRIPS) {
            waitSemaphore(semid, SEM_MUTEX); // Lock
            int endOfDay = sm->signalEndOfDay;
            int earlyVoyage = sm->signalEarlyVoyage;
            signalSemaphore(semid, SEM_MUTEX); // Unlock

            if (endOfDay) {
                printf("=== Ship Captain === Ending day due to signal\n");
                shmdt(sm);
                return;
            }

            if (earlyVoyage) {
                printf("=== Ship Captain === Early departure triggered\n");
                waitSemaphore(semid, SEM_MUTEX);
                sm->signalEarlyVoyage = 0;
                signalSemaphore(semid, SEM_MUTEX);
                break;
            }

            sleep(1);
            slept++;
        }
    }
}
