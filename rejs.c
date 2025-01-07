#include "utils.h"
#include "passenger.h"

#define NUM_PASSENGERS 10

int main() {
    handleInput();

    int shmid = initializeSharedMemory();
    SharedMemory *sm = attachSharedMemory(shmid);
    int semid = initializeSemaphores();

    // Shared memory initialization
    sm->peopleOnShip = 0;
    sm->peopleOnBridge = 0;
    sm->currentVoyage = 0;
    sm->signalEarlyVoyage = 0;
    sm->signalEndOfDay = 0;
    sm->queueDirection = 0;

    pid_t harbourCaptainPid = fork();
    if (harbourCaptainPid == -1) {
        perror("Error forking for harbourCaptain");
        exit(EXIT_FAILURE);
    } else if (harbourCaptainPid == 0) {
        if (execl("./harbourCaptain", "harbourCaptain", to_string(shmid), to_string(semid), NULL) == -1) {
            perror("execl harbourCaptain");
            exit(EXIT_FAILURE);
        }
    }

    pid_t shipCaptainPid = fork();
    if (shipCaptainPid < 0) {
        perror("Error forking for shipCaptain");
        exit(EXIT_FAILURE);
    } else if (shipCaptainPid == 0) {
        if (execl("./shipCaptain", "shipCaptain", to_string(shmid), to_string(semid), NULL) == -1) {
            perror("execl shipCaptain");
            exit(EXIT_FAILURE);
        }
    }

    sm->harbourCaptainPid = harbourCaptainPid;
    sm->shipCaptainPid = shipCaptainPid;

    // Creating threads of passengers
    pthread_t passengers[NUM_PASSENGERS];
    PassengerData passengerData[NUM_PASSENGERS];

    srand(time(NULL));

    for (int i = 0; i < NUM_PASSENGERS; i++) {
        passengerData[i].shmid = shmid;
        passengerData[i].semid = semid;
        passengerData[i].passengerID = i + 1;

        if (pthread_create(&passengers[i], NULL, Passenger, &passengerData[i]) != 0) {
            perror("Error creating passenger thread");
            exit(EXIT_FAILURE);
        }

        // <0.1s; 2s>
        int delay = rand() % 2000 + 100; 
        usleep(delay * 1000);
    }

    // Waiting for thread termination
    for (int i = 0; i < NUM_PASSENGERS; i++) {
        pthread_join(passengers[i], NULL);
    }

    // Waiting for the captains to finish
    waitpid(harbourCaptainPid, NULL, 0);
    waitpid(shipCaptainPid, NULL, 0);

    // Cleanup
    shmdt(sm);
    cleanupSharedMemory(shmid);
    cleanupSemaphores(semid);

    printf("Main process finished. Cleaned up shared memory and semaphores.\n");
    return 0;
}
