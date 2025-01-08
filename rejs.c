#include "utils.h"
#include "passenger.h"

#define NUM_PASSENGERS 20

int main() {
    handleInput();

    int shmid = initializeSharedMemory();
    SharedMemory *sm = attachSharedMemory(shmid);
    int semid = initializeSemaphores();

    char shmidStr[10], semidStr[10];
    sprintf(shmidStr, "%d", shmid);
    sprintf(semidStr, "%d", semid);

    // Shared memory initialization
    waitSemaphore(semid, SEM_MUTEX);
    sm->peopleOnShip = 0;
    sm->peopleOnBridge = 0;
    sm->currentVoyage = 0;
    sm->signalEarlyVoyage = 0;
    sm->signalEndOfDay = 0;
    sm->queueDirection = 0;
    sm->harbourCaptainPid = 0;
    sm->shipCaptainPid = 0;
    signalSemaphore(semid, SEM_MUTEX);

    pid_t harbourCaptainPid = fork();
    if (harbourCaptainPid == -1) {
        perror(RED "Error forking for harbourCaptain" RESET);
        exit(EXIT_FAILURE);
    } else if (harbourCaptainPid == 0) {
        if (execl("./harbourCaptain", "harbourCaptain", shmidStr, NULL) == -1) {
            perror(RED "execl harbourCaptain" RESET);
            exit(EXIT_FAILURE);
        }
    }

    waitSemaphore(semid, SEM_MUTEX);
    sm->harbourCaptainPid = harbourCaptainPid;
    signalSemaphore(semid, SEM_MUTEX);

    pid_t shipCaptainPid = fork();
    if (shipCaptainPid == -1) {
        perror(RED "Error forking for shipCaptain" RESET);
        exit(EXIT_FAILURE);
    } else if (shipCaptainPid == 0) {
        if (execl("./shipCaptain", "shipCaptain", shmidStr, semidStr, NULL) == -1) {
            perror(RED "execl shipCaptain" RESET);
            exit(EXIT_FAILURE);
        }
    }

    waitSemaphore(semid, SEM_MUTEX);
    sm->shipCaptainPid = shipCaptainPid;
    signalSemaphore(semid, SEM_MUTEX);

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
            perror(RED "Error creating passenger thread" RESET);
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
    waitpid(shipCaptainPid, NULL, 0);
    printf(GREEN "=== Main === Ship Captain has finished." RESET "\n");
    waitpid(harbourCaptainPid, NULL, 0);
    printf(GREEN "=== Main === Harbour Captain has finished." RESET "\n");

    // Cleanup
    shmdt(sm);
    cleanupSharedMemory(shmid);
    cleanupSemaphores(semid);

    printf(GREEN "Main process finished. Cleaned up shared memory and semaphores." RESET "\n");
    return 0;
}
