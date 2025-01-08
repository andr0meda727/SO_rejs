#include "passenger.h"

#define CYAN "\033[36m"
#define RESET "\033[0m"


void *Passenger(void *arg) {
    PassengerData *pd = (PassengerData *)arg;

    SharedMemory *sm = (SharedMemory *)shmat(pd->shmid, NULL, 0);
    if (sm == (void *)-1) {
        perror("shmat passenger");
        pthread_exit(NULL);
    }

    printf(CYAN "=== Passenger %d ===\033" RESET " Starting\n", pd->passengerID);

    // Trying to enter bridge
    while (1) {
        // Check for end-of-day signal before entering the bridge
        waitSemaphore(pd->semid, SEM_MUTEX);
        int endOfDay = sm->signalEndOfDay;
        int earlyVoyage = sm->signalEarlyVoyage;
        signalSemaphore(pd->semid, SEM_MUTEX);

        if (endOfDay) {
            printf(CYAN "=== Passenger %d ===" RESET " End-of-day signal detected. Exiting.\n", pd->passengerID);
            break;
        }

        if (earlyVoyage) {
            printf(CYAN "=== Passenger %d ===" RESET " Early departure signal detected. Exiting.\n", pd->passengerID);
            break;
        }

        // with error checking
        // if (semop(pd->semid, SEM_BRIDGE, -1) == -1) {
        //     perror("semop SEM_BRIDGE -1 passenger");
        //     pthread_exit(NULL);
        // }

        waitSemaphore(pd->semid, SEM_BRIDGE);

        // Check bridge direction and ship availability
        waitSemaphore(pd->semid, SEM_MUTEX);
        if (sm->queueDirection == 1) {
            // Bridge towards land, cannot be entered
            signalSemaphore(pd->semid, SEM_MUTEX);
            // Freeup bridge
            signalSemaphore(pd->semid, SEM_BRIDGE);
            printf(CYAN "=== Passenger %d ===" RESET " Bridge direction towards land. Cannot board. Exiting.\n", pd->passengerID);
            break;
        }

        if (sm->peopleOnShip >= SHIP_CAPACITY) {
            // Ship full
            signalSemaphore(pd->semid, SEM_MUTEX);
            // Freeup bridge
            signalSemaphore(pd->semid, SEM_BRIDGE);
            printf(CYAN "=== Passenger %d ===" RESET " Ship is full. Leaving bridge. Exiting.\n", pd->passengerID);
            break;
        }

        // Enter bridge
        sm->peopleOnBridge++;
        printf(CYAN "=== Passenger %d ===" RESET " Entered the bridge. People on bridge: %d / %d\n", pd->passengerID, sm->peopleOnBridge, BRIDGE_CAPACITY);
        signalSemaphore(pd->semid, SEM_MUTEX);

        // Going through bridge
        sleep(1);

        // Trying to enter ship
        waitSemaphore(pd->semid, SEM_MUTEX);
        if (sm->peopleOnShip < SHIP_CAPACITY) {
            sm->peopleOnShip++;
            sm->peopleOnBridge--;
            printf(CYAN "=== Passenger %d ===" RESET " Boarded the ship. People on ship: %d / %d\n", 
                pd->passengerID, sm->peopleOnShip, SHIP_CAPACITY);
            signalSemaphore(pd->semid, SEM_MUTEX);

            // Freeup bridge
            signalSemaphore(pd->semid, SEM_BRIDGE);
        } else {
            // Ship full while passenger was on bridge
            sm->peopleOnBridge--;
            signalSemaphore(pd->semid, SEM_MUTEX);
            // Freeup bridge
            signalSemaphore(pd->semid, SEM_BRIDGE);
            printf(CYAN "=== Passenger %d ===" RESET " Ship became full while on bridge. Leaving bridge. Exiting.\n", pd->passengerID);
            break;
        }

        break; // Entered ship successfully
    }


    // Waiting for end-of-day signal or end of cruise
    while (1) {
        // Checking if cruises ended or signal was sent
        waitSemaphore(pd->semid, SEM_MUTEX);
        int cruiseEnded = (sm->queueDirection == 1 && sm->peopleOnShip > 0);
        int endOfDay = sm->signalEndOfDay;
        signalSemaphore(pd->semid, SEM_MUTEX);

        if (cruiseEnded || endOfDay) {
            printf(CYAN "=== Passenger %d ===" RESET " Cruise ended or end-of-day. Disembarking.\n", pd->passengerID);

            // Going off
            waitSemaphore(pd->semid, SEM_MUTEX);
            sm->peopleOnShip--;
            sm->peopleOnBridge++;
            printf(CYAN "=== Passenger %d ===" RESET " Disembarked the ship. People on ship: %d, on bridge: %d\n", pd->passengerID, sm->peopleOnShip, sm->peopleOnBridge);
            signalSemaphore(pd->semid, SEM_MUTEX);

            // Going off bridge
            sleep(1);

            // Exited bridge
            waitSemaphore(pd->semid, SEM_MUTEX);
            sm->peopleOnBridge--;
            printf(CYAN "=== Passenger %d ===" RESET " Exited the bridge. People on bridge: %d\n", pd->passengerID, sm->peopleOnBridge);
            signalSemaphore(pd->semid, SEM_MUTEX);

            // Freeup bridge
            signalSemaphore(pd->semid, SEM_BRIDGE);

            break;
        }

        usleep(200000); // 0.2s
    }

    // Ending passenger thread
    printf(CYAN "=== Passenger %d ===" RESET " Finished the day and exited.\n", pd->passengerID);
    // shmdt(sm);
    pthread_exit(NULL);
}