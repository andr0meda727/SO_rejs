#include "utils.h"

volatile sig_atomic_t endOfDaySignal = 0; // Flag for sigusr2

int shmid, semid;
int earlyVoyage = 0;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, RED "Usage: %s <shmid> <semid>" RESET "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shmid = atoi(argv[1]);
    semid = atoi(argv[2]);

    printf(YELLOW "=== Ship Captain ===" RESET " Starting\n");

    // Create FIFO for communication from ship captain
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror(RED "mkfifo" RESET);
        exit(EXIT_FAILURE);
    }

    int fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open fifo");
        exit(EXIT_FAILURE);
    }

    pid_t myPID = getpid();

    // Writing PID to FIFO
    if (write(fifo_fd, &myPID, sizeof(myPID)) == -1) {
        perror(RED "write to FIFO" RESET);
        exit(EXIT_FAILURE);
    }

    printf(YELLOW "=== Ship Captain ===" RESET " PID was sent to the harbour captain.\n");
    close(fifo_fd); // Closing FIFO

    launchShipCaptain(shmid, semid);

    return 0;
}


void handle_signal(int sig) {
    SharedMemory *sm = attachSharedMemory(shmid);
    if (sm == (void *)-1) {
        perror(RED "shmat in signal handler" RESET);
        exit(EXIT_FAILURE);
    }


    if (sig == SIGUSR1) {
        waitSemaphore(semid, SEM_MUTEX);
        if (sm->shipSailing) {
            printf(YELLOW "=== Ship Captain ===" RESET " I'm currently sailing, the signal cannot be made.\n");
            signalSemaphore(semid, SEM_MUTEX);
        } else {
            earlyVoyage = 1;
            sm->queueDirection = 1; // queue towards land, so passenger can't enter 
            sm->shipSailing = 1;
            signalSemaphore(semid, SEM_MUTEX);
        }
    } else if (sig == SIGUSR2) {
        int shipSailing;

        // Critical section
        waitSemaphore(semid, SEM_MUTEX);
        shipSailing = sm->shipSailing;
        if (!shipSailing) {
            sm->signalEndOfDay = 1; // Set the flag in shared memory
        }
        signalSemaphore(semid, SEM_MUTEX);

        // Logic outside the critical section
        if (shipSailing) {
            endOfDaySignal = 1; // Local flag
        }
    }
    
    shmdt(sm);
}


void launchShipCaptain(int shmid, int semid) {
    SharedMemory *sm = attachSharedMemory(shmid);

    if (sm == (void *)-1) {
        perror(RED "shmat ship captain" RESET);
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;


    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror(RED "sigaction SIGUSR1" RESET);
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror(RED "sigaction SIGUSR2" RESET);
        exit(EXIT_FAILURE);
    }

    while (1) {
        waitSemaphore(semid, SEM_MUTEX);
        if (sm->currentVoyage >= NUMBER_OF_TRIPS_PER_DAY) {
            sm->queueDirection = 1;
            signalSemaphore(semid, SEM_MUTEX);
            printf(YELLOW "=== Ship Captain ===" RESET " Reached daily trip limit %d. Ending work.\n", NUMBER_OF_TRIPS_PER_DAY);
            shmdt(sm);
            return;
        }
        signalSemaphore(semid, SEM_MUTEX);

        // Waiting between cruises with the possibility of an early departure
        int slept = 0;
        while (slept < TIME_BETWEEN_TRIPS) {
            waitSemaphore(semid, SEM_MUTEX); // Lock
            int endOfDay = sm->signalEndOfDay;
            int numberOfPeopleOnShip = sm->peopleOnShip;
            signalSemaphore(semid, SEM_MUTEX); // Unlock

            if (endOfDay) {
                printf(YELLOW "=== Ship Captain ===" RESET " End-of-day signal received. Preparing for disembarking.\n");

                //  Setting the direction of the bridge and waiting for passengers to leave
                waitSemaphore(semid, SEM_MUTEX);
                sm->queueDirection = 1;
                signalSemaphore(semid, SEM_MUTEX);

                // Waiting until there are no passengers on the ship and bridge
                while (1) {
                    waitSemaphore(semid, SEM_MUTEX);
                    int peopleOnShip = sm->peopleOnShip;
                    int peopleOnBridge = sm->peopleOnBridge;
                    signalSemaphore(semid, SEM_MUTEX);

                    if (peopleOnShip == 0 && peopleOnBridge == 0) {
                        printf(YELLOW "=== Ship Captain ===" RESET " All passengers have disembarked.\n");
                        break;
                    }

                    usleep(100000); // 0.1s
                }

                printf(YELLOW "=== Ship Captain ===" RESET " Ending day as per signal.\n");
                shmdt(sm);
                return;
            }

            if (earlyVoyage) {
                printf(YELLOW "=== Ship Captain ===" RESET " Early departure signal received.\n");

                // Waiting until the bridge is empty
                while (1) {
                    waitSemaphore(semid, SEM_MUTEX);
                    int peopleOnBridge = sm->peopleOnBridge;
                    signalSemaphore(semid, SEM_MUTEX);

                    if (peopleOnBridge == 0) {
                        break;
                    }

                    usleep(100000); // 0.1s
                }
                waitSemaphore(semid, SEM_MUTEX);
                earlyVoyage = 0;
                signalSemaphore(semid, SEM_MUTEX);

                break; // Going to departure
            }

            if (numberOfPeopleOnShip >= SHIP_CAPACITY) {
                waitSemaphore(semid, SEM_MUTEX);
                sm->queueDirection = 1;
                signalSemaphore(semid, SEM_MUTEX);
            }

            sleep(1);
            slept++;
        }

        printf(YELLOW "=== Ship Captain ===" RESET " All the people on the bridge have to go ashore, we are sailing away!\n");
        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 1; // queue towards land, so passenger can't enter 
        sm->shipSailing = 1;
        signalSemaphore(semid, SEM_MUTEX);

        // Waiting for all passengers to pass
        while (1) {
            waitSemaphore(semid, SEM_MUTEX);
            int peopleOnBridge = sm->peopleOnBridge;
            signalSemaphore(semid, SEM_MUTEX);

            if (peopleOnBridge == 0) {
                break;
            }

            usleep(100000); // 0.1s
        }

        // Cruise starting
        waitSemaphore(semid, SEM_MUTEX);
        int voyageNumber = sm->currentVoyage + 1;
        int peopleOnVoyage = sm->peopleOnShip;
        signalSemaphore(semid, SEM_MUTEX);

        printf(YELLOW "=== Ship Captain ===" RESET " All passengers have descended. We sail away.\n");
        printf(YELLOW "=== Ship Captain ===" RESET " Sailing on cruise %d with %d passengers.\n", voyageNumber, peopleOnVoyage);

        // Simulation of cruise
        struct timespec req, rem;
        req.tv_sec = TRIP_DURATION;
        req.tv_nsec = 0;

        // Loop to ensure the full trip duration is completed
        while (nanosleep(&req, &rem) == -1) {
            if (errno == EINTR) {
                printf(YELLOW "=== Ship Captain ===" RESET " Signal received during voyage, resuming sleep...\n");
                req = rem; // Use remaining time to continue sleeping
            } else {
                perror(RED "nanosleep" RESET);
                exit(EXIT_FAILURE);
            }
        }

        if (endOfDaySignal) {
            printf(YELLOW "=== Ship Captain ===" RESET " End-of-day signal received during voyage. Ending day as per signal.\n");

            waitSemaphore(semid, SEM_MUTEX);
            sm->signalEndOfDay = 1;
            sm->queueDirection = 1; // queue towards land, so passenger can't enter
            signalSemaphore(semid, SEM_MUTEX);

            shmdt(sm);
            return;
        }

        waitSemaphore(semid, SEM_MUTEX);
        sm->shipSailing = 0;       // end of cruise
        sm->queueDirection = 1;    // towards land to disembark
        sm->currentVoyage++;
        signalSemaphore(semid, SEM_MUTEX);
        
        printf(YELLOW "=== Ship Captain ===" RESET " Cruise %d has ended. Arriving at port.\n", voyageNumber);

        // Waiting for all passengers to get off
        while (1) {
            waitSemaphore(semid, SEM_MUTEX);
            int peopleOnShip = sm->peopleOnShip;
            int peopleOnBridge = sm->peopleOnBridge;
            signalSemaphore(semid, SEM_MUTEX);

            if (peopleOnShip == 0 && peopleOnBridge == 0) {
                printf(YELLOW "=== Ship Captain ===" RESET " All passengers have disembarked after cruise %d.\n", voyageNumber);
                break;
            }

            usleep(100000); // 0.1s
        }

        // Change bridge direction again
        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 0; // towards ship, getting ready for next voyage
        signalSemaphore(semid, SEM_MUTEX);

        printf(YELLOW "=== Ship Captain ===" RESET " Bridge direction set back to boarding for the next voyage.\n");
    }
}
