/* captains.h */
#ifndef CAPTAINS_H
#define CAPTAINS_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#define SHIP_CAPACITY 100
#define BRIDGE_CAPACITY 10
#define TIME_BETWEEN_TRIPS 60
#define TRIP_DURATION 30
#define NUMBER_OF_TRIPS_PER_DAY 5

#define SHM_PROJECT_ID 'A'
#define SEM_PROJECT_ID 'B'

// Semaphore indices in the semaphore array
#define SEM_MUTEX 0      // Semaphore for the critical section
#define SEM_BRIDGE 1     // Semaphore controlling the number of people on the bridge

typedef struct {
    int peopleOnShip;
    int peopleOnBridge;
    int currentVoyage;      // Current number of completed voyages
    int signalEarlyVoyage;  // Signal 1
    int signalEndOfDay;     // Signal 2
    int queueDirection;     // 0 = towards ship, 1 = towards land

    pid_t harbourCaptainPid;
    pid_t shipCaptainPid;
} SharedMemory;

void launchShipCaptain(int shmid, int semid);
void launchHabourCaptain(int shmid, int semid);

#endif 
