#include "utils.h"

int shmidToSignal;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, RED "Usage: %s <shmid> <semid>" RESET "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shmid = atoi(argv[1]);
    int semid = atoi(argv[2]);
    int writeFd = atoi(argv[3]);

    pid_t myPID = getpid();

    // Save the PID to pipe
    if (write(writeFd, &myPID, sizeof(myPID)) == -1) {
        perror(RED "write to pipe" RESET);
        exit(EXIT_FAILURE);
    }
    printf(YELLOW "=== Ship Captain ===" RESET " PID was sent to the harbour captain.\n");
    close(writeFd); // We close the pipe after sending the PID


    launchShipCaptain(shmid, semid);

    return 0;
}


void handle_signal(int sig) {
    SharedMemory *sm = attachSharedMemory(shmidToSignal);
    
    if (sm == (void *)-1) {
        perror(RED "shmat in signal handler" RESET);
        exit(EXIT_FAILURE);
    }

    if (sig == SIGUSR1) {
        printf(YELLOW "=== Ship Captain ===" RESET " Early departure signal has been received\n");
        waitSemaphore(shmidToSignal, SEM_MUTEX);
        sm->signalEarlyVoyage = 1;
        sm->queueDirection = 1; // queue towards land, so passenger can't enter 
        signalSemaphore(shmidToSignal, SEM_MUTEX);
    } else if (sig == SIGUSR2) {
        waitSemaphore(shmidToSignal, SEM_MUTEX);
        sm->signalEndOfDay = 1;
        sm->queueDirection = 1; // queue towards land, so passenger can't enter
        signalSemaphore(shmidToSignal, SEM_MUTEX);
    }

    shmdt(sm); // Detach after updating
}

void launchShipCaptain(int shmid, int semid) {
    SharedMemory *sm = attachSharedMemory(shmid);
    shmidToSignal = shmid;

    if (sm == (void *)-1) {
        perror(RED "shmat ship captain" RESET);
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;


    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror(RED "sigaction SIGUSR1" RESET);
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror(RED "sigaction SIGUSR2" RESET);
        exit(EXIT_FAILURE);
    }

    printf(YELLOW "=== Ship Captain ===" RESET " Starting\n");

    while (1) {
        waitSemaphore(semid, SEM_MUTEX);
        if (sm->currentVoyage >= NUMBER_OF_TRIPS_PER_DAY) {
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
            int earlyVoyage = sm->signalEarlyVoyage;
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
                        printf(YELLOW "=== Ship Captain ===" RESET " Bridge is empty. Departing early.\n");
                        break;
                    }

                    usleep(100000); // 0.1s
                }

                break; // Going to departure
            }

            sleep(1);
            slept++;
        }

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
        sm->currentVoyage++;
        int voyageNumber = sm->currentVoyage;
        int peopleOnVoyage = sm->peopleOnShip;
        signalSemaphore(semid, SEM_MUTEX);

        printf(YELLOW "=== Ship Captain ===" RESET " Sailing on cruise %d with %d passengers.\n", voyageNumber, peopleOnVoyage);

        // Simulation of cruise
        sleep(TRIP_DURATION);

        // Cruise end
        printf(YELLOW "=== Ship Captain ===" RESET " Cruise %d has ended. Arriving at port.\n", voyageNumber);



        printf(YELLOW "=== Ship Captain ===" RESET " Sailing on cruise %d with %d passengers.\n", voyageNumber, peopleOnVoyage);
        sleep(TRIP_DURATION);

        // Disembarkation
        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 1; // towards land
        signalSemaphore(semid, SEM_MUTEX);

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
