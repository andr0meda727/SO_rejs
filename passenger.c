#include "passenger.h"

void *Passenger(void *arg) {
    PassengerData *pd = (PassengerData *)arg;

    SharedMemory *sm = (SharedMemory *)shmat(pd->shmid, NULL, 0);
    if (sm == (void *)-1) {
        perror("shmat passenger");
        pthread_exit(NULL);
    }

    printf("=== Passenger %d === Trying to enter the bridge\n", pd->passengerID);


    // Entering bridge
    waitSemaphore(pd->semid, SEM_BRIDGE);
    // pthread_exit(NULL); add check for error (waitsem dont return anything maybe another similar function idk)
    
    // Critical section
    waitSemaphore(pd->semid, SEM_MUTEX);

    if (sm->peopleOnShip < SHIP_CAPACITY) {
        // Can enter
        sm->peopleOnBridge++;
        printf("=== Passenger %d === I entered the bridge\n", pd->passengerID);
        sleep(1);
        sm->peopleOnShip++;
        sm->peopleOnBridge--;

        printf("=== Passenger %d === I entered the ship. (number of people on ship: %d / %d)\n", 
               pd->passengerID, sm->peopleOnShip, SHIP_CAPACITY);

        signalSemaphore(pd->semid, SEM_MUTEX);

        // Freeing up space on the bridge, as we have already stepped off it (its okay for now, but bridge can hold up to BRIDGE_CAPACITY people on it, so have to adjust it a little)
        signalSemaphore(pd->semid, SEM_BRIDGE);

        // waiting for cruise end ?in loop?
        while (1) {
            waitSemaphore(pd->semid, SEM_MUTEX);
            int cruiseEnded = sm->peopleOnShip > 0 && sm->queueDirection == 1; // Ship arrived and bridge direction changed
            signalSemaphore(pd->semid, SEM_MUTEX);

            if (cruiseEnded) {
                waitSemaphore(pd->semid, SEM_MUTEX);

                if (sm->peopleOnShip > 0) {
                    sm->peopleOnShip--;
                    printf("=== Passenger %d === Disembarking. Remaining on ship: %d\n",
                        pd->passengerID, sm->peopleOnShip);

                    signalSemaphore(pd->semid, SEM_MUTEX);
                    
                    sleep(1); // 1s for passenger
                    break; // Passenger descends
                }
                
                signalSemaphore(pd->semid, SEM_MUTEX);
            }
            usleep(200000); // 0.2s
        }
    } else {
        printf("=== Passenger %d === Ship is full. I can't enter\n", pd->passengerID);
        signalSemaphore(pd->semid, SEM_MUTEX);
        // freeing space on bridge
        signalSemaphore(pd->semid, SEM_BRIDGE);
    }

    
    // shmdt(sm);
    // pthread_exit(NULL);
}
