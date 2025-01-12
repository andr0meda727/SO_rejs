#include "utils.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, RED "Usage: %s <shmid> <semid>" RESET "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shmid = atoi(argv[1]);
    int semid = atoi(argv[2]);

    SharedMemory *sm = attachSharedMemory(shmid);
    if (sm == (void *)-1) {
        perror(RED "shmat passenger" RESET);
        exit(EXIT_FAILURE);
    }

    printf(CYAN "=== Passenger %d ===" RESET " I'm entering the port!\n", getpid());

    int onShip = 0;  // flag: am I already on the ship?

    while (1) {
        // Check signals endOfDay, earlyVoyage in the critical section
        waitSemaphore(semid, SEM_MUTEX);
        int endOfDay = sm->signalEndOfDay;
        int shipSail = sm->shipSailing;
        int queueDir = sm->queueDirection;
        int shipFull = (sm->peopleOnShip >= SHIP_CAPACITY);
        signalSemaphore(semid, SEM_MUTEX);

        // 1. End of the day -> exit
        if (endOfDay) {
            if (onShip) {
                // Disembarking after signal
                waitSemaphore(semid, SEM_BRIDGE);
                waitSemaphore(semid, SEM_MUTEX);
                sm->peopleOnShip--;
                sm->peopleOnBridge++;
                printf(CYAN "=== Passenger %d ===" RESET " End of day signal received. I'm getting off the ship. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
                signalSemaphore(semid, SEM_MUTEX);

                sleep(1); // simulation of disembarking

                waitSemaphore(semid, SEM_MUTEX);
                sm->peopleOnBridge--;
                printf(CYAN "=== Passenger %d ===" RESET " I left the bridge. Exiting port. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
                signalSemaphore(semid, SEM_MUTEX);
                signalSemaphore(semid, SEM_BRIDGE); // Free the space on the bridge
            } else {
                // People waiting in port also exit
                printf(CYAN "=== Passenger %d ===" RESET " Exiting port.\n", getpid());
            }

            break;
        }

        // 2. If I haven't boarded yet then try to board
        if (!onShip) {
            // Boarding conditions: 
            // * queueDirection=0 (bridge towards the ship),
            // * shipSailing=0 (ship in port),
            // * !shipFull

            // First, I wait for the bridge semaphore
            waitSemaphore(semid, SEM_BRIDGE);

            // Critical section
            waitSemaphore(semid, SEM_MUTEX);
            // reload the current parameters again (they may have changed in the meantime)
            queueDir = sm->queueDirection;
            shipSail = sm->shipSailing;
            shipFull = (sm->peopleOnShip >= SHIP_CAPACITY);

            if (queueDir == 0 && shipSail == 0 && !shipFull) {
                // I can board the bridge
                sm->peopleOnBridge++;
                printf(CYAN "=== Passenger %d ===" RESET " I entered the bridge. PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
                signalSemaphore(semid, SEM_MUTEX);
                // Simulation of crossing the bridge
                sleep(1);

                // Attempt to board the ship, again checking conditions
                waitSemaphore(semid, SEM_MUTEX);
                if (sm->peopleOnShip < SHIP_CAPACITY && sm->queueDirection == 0 && sm->shipSailing == 0) {
                    sm->peopleOnShip++;
                    sm->peopleOnBridge--;
                    // Boarded so changing to 1
                    onShip = 1;
                    printf(CYAN "=== Passenger %d ===" RESET " I boarded the ship (voyage no. %d). PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n", getpid(), sm->currentVoyage + 1, sm->peopleOnShip, sm->peopleOnBridge);
                    signalSemaphore(semid, SEM_MUTEX);

                    // Release the bridge semaphore
                    signalSemaphore(semid, SEM_BRIDGE);
                } else {
                    // In the meantime, the ship has become full or the queueDirection has changed
                    sm->peopleOnBridge--;
                    printf(CYAN "=== Passenger %d ===" RESET " I cannot enter ship. I'm leaving the bridge. PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
                    signalSemaphore(semid, SEM_MUTEX);

                    signalSemaphore(semid, SEM_BRIDGE);
                    // wait for a moment and try again
                    usleep(200000);
                }
            } else {
                printf(CYAN "=== Passenger %d ===" RESET " I cannot enter bridge. PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
                signalSemaphore(semid, SEM_MUTEX);
                signalSemaphore(semid, SEM_BRIDGE);
                usleep(2000000);
            }
        }
        else {
            // 3. If I'm on the ship (onShip=1):
            // I wait for the voyage to end => shipSailing=0,
            // and queueDirection=1 (bridge towards the land) to disembark
            waitSemaphore(semid, SEM_MUTEX);
            int nowSail = sm->shipSailing;
            int nowqueueDir = sm->queueDirection;
            signalSemaphore(semid, SEM_MUTEX);

            if (nowSail == 0 && nowqueueDir == 1) {
                // Can disembark
                waitSemaphore(semid, SEM_BRIDGE);
                waitSemaphore(semid, SEM_MUTEX);
                sm->peopleOnShip--;
                sm->peopleOnBridge++;
                printf(CYAN "=== Passenger %d ===" RESET " Disembarking from ship. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
                signalSemaphore(semid, SEM_MUTEX);

                // simulation of crossing the bridge
                sleep(1);

                // successfully disembarked
                waitSemaphore(semid, SEM_MUTEX);
                sm->peopleOnBridge--;
                printf(CYAN "=== Passenger %d ===" RESET " Left bridge. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
                signalSemaphore(semid, SEM_MUTEX);

                signalSemaphore(semid, SEM_BRIDGE);

                break; // end of the passenger process
            }

            // otherwise, continue waiting
            usleep(200000);
        }
    }

    shmdt(sm);
    return 0;
}
