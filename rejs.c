#include "utils.h"

int main() {
    handleInput();

    int shmid = initializeSharedMemory();
    SharedMemory *sm = attachSharedMemory(shmid);

    int semid = initializeSemaphores();

    sm->peopleOnShip = 0;
    sm->peopleOnBridge = 0;
    sm->currentVoyage = 0;
    sm->signalEarlyVoyage = 0;
    sm->signalEndOfDay = 0;
    sm->queueDirection = 0;

    pid_t harbourCaptainPid = createHarbourCaptain(shmid, semid);
    pid_t shipCaptainPid = createShipCaptain(shmid, semid);

    sm->harbourCaptainPid = harbourCaptainPid;
    sm->shipCaptainPid = shipCaptainPid;


    waitpid(harbourCaptainPid, NULL, 0);
    waitpid(shipCaptainPid, NULL, 0);

    cleanupSharedMemory(shmid);
    cleanupSemaphores(semid);

    return 0;
}
