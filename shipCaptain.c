#include "utils.h"

int shmidToSignal;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <shmid> <semid>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shmid = atoi(argv[1]);
    int semid = atoi(argv[2]);

    launchShipCaptain(shmid, semid);
    
    return 0;
}


void handle_signal(int sig) {
    SharedMemory *sm = attachSharedMemory(shmidToSignal);
    
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
        exit(1);
    }

    shmdt(sm); // Detach after updating
}

void launchShipCaptain(int shmid, int semid) {
    SharedMemory *sm = attachSharedMemory(shmid);
    shmidToSignal = shmid;

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
                waitSemaphore(semid, SEM_MUTEX);
                // simulate people getting and change the queue direction 
                sm->peopleOnShip = 0;
                signalSemaphore(semid, SEM_MUTEX);
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

        waitSemaphore(semid, SEM_MUTEX);
        if (sm->currentVoyage >= NUMBER_OF_TRIPS_PER_DAY) {
            signalSemaphore(semid, SEM_MUTEX);
            printf("=== Ship Captain === The cruise limit %d has been reached. End of work for today.”\n", NUMBER_OF_TRIPS_PER_DAY);
            shmdt(sm);
            return;
        }
        signalSemaphore(semid, SEM_MUTEX);

        int bridgeCheck = 0;
        while (1) {
            waitSemaphore(semid, SEM_MUTEX);
            bridgeCheck = sm->peopleOnBridge;
            int endOfDay = sm->signalEndOfDay;
            signalSemaphore(semid, SEM_MUTEX);

            if (endOfDay) {
                // Checking to see if someone has sent an end-of-day signal while waiting for an empty bridge
                printf("=== Ship Captain === End-of-day signal was received just before departure.\n");
                // End of day, people have to leave ship
                waitSemaphore(semid, SEM_MUTEX);
                sm->peopleOnShip = 0;
                signalSemaphore(semid, SEM_MUTEX);

                shmdt(sm);
                return;
            }

            if (bridgeCheck == 0) {
                break;
            }
            // checking every 0.5s
            usleep(500000);
        }

        waitSemaphore(semid, SEM_MUTEX);
        int voyageNumber = ++sm->currentVoyage;
        int peopleOnVoyage = sm->peopleOnShip; // have to be <= ship capacity
        signalSemaphore(semid, SEM_MUTEX);
        printf("=== Ship Captain === Sailing on cruise %d with %d passengers.\n", voyageNumber, peopleOnVoyage);

        sleep(TRIP_DURATION);

        // End of cruise and changing bridge direction
        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 1; // 1 = towards land
        signalSemaphore(semid, SEM_MUTEX);

        printf("=== Ship Captain === Cruise %d has ended. Bridge direction set to disembarking.\n", voyageNumber);

        while (1) {
            waitSemaphore(semid, SEM_MUTEX);
            if (sm->peopleOnShip == 0 && sm->peopleOnBridge == 0) {
                signalSemaphore(semid, SEM_MUTEX);
                break;
            }
            signalSemaphore(semid, SEM_MUTEX);
            usleep(100000); // 0.1s
        }

        // Check whether the end-of-day signal came during the cruise
        waitSemaphore(semid, SEM_MUTEX);
        int endOfDay = sm->signalEndOfDay;
        signalSemaphore(semid, SEM_MUTEX);
        if (endOfDay) {
            // End-of-day signal received. We are ending the day
            printf("=== Ship Captain === I end after cruise %d, because signal was sent.\n", voyageNumber);
            shmdt(sm);
            return;
        }

        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 0; // If brigde is empty, queueDirection again set to 0 (queue towards ship)
        signalSemaphore(semid, SEM_MUTEX);

        printf("=== Ship Captain === Bridge direction set to boarding for the next voyage.\n");
    }

    shmdt(sm);
}
