#ifndef UTILS_H
#define UTILS_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>


#define SHIP_CAPACITY 100
#define BRIDGE_CAPACITY 10
#define TIME_BETWEEN_TRIPS 60 // [s]
#define TRIP_DURATION 30 // [s]
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

void handleInput();
void launchShipCaptain(int shmid, int semid);
void launchHabourCaptain(int shmid, int semid);
int initializeSharedMemory();
int initializeSemaphores();
void cleanupSemaphores(int semid);
void cleanupSharedMemory(int shmid);
int waitSemaphore(int semID, int number);
void signalSemaphore(int semID, int number);
SharedMemory* attachSharedMemory(int shmid);

#endif 