#include "passenger.h"

void *Passenger(void *arg) {
    PassengerData *pd = (PassengerData *)arg;

    SharedMemory *sm = (SharedMemory *)shmat(pd->shmid, NULL, 0);
    if (pd == (void *)-1) {
        perror("shmat passenger");
        pthread_exit(NULL);
    }

    printf("=== Passenger %d === Trying to enter the ship\n", pd->passengerID);

    // checking if passenger can enter the ship


    // pthread_exit(NULL);
}
