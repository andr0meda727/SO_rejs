#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>

#define SHIP_CAPACITY 100
#define BRIDGE_CAPACITY 10
#define TIME_BETWEEN_TRIPS 60
#define TRIP_DURATION 30
#define NUMBER_OF_TRIPS_PER_DAY 5

typedef struct {
    int peopleOnShip;
    int peopleOnBridge;
    int currentVoyage; // current number of completed voyages
    int signalEarlyVoyage; // signal1 
    int signalEndOfDay; // signal2
    int queueDirection; // 0 = towards ship, 1 = towards land
} SharedMemory;

void handleInput() {
    if (SHIP_CAPACITY <= BRIDGE_CAPACITY) {
        fprintf(stderr, "The bridge capacity must be smaller than the ship capacity.\n");
        exit(1);
    }

    if (BRIDGE_CAPACITY < 1) {
        fprintf(stderr, "The bridge capacity must be greater than 0.\n");
        exit(2);
    }

    if (SHIP_CAPACITY < 1) {
        fprintf(stderr, "The ship capacity must be greater than 0.\n");
        exit(3);
    }

    if (TIME_BETWEEN_TRIPS <= TRIP_DURATION) {
        fprintf(stderr, "The trip duration must be shorter than the time between trips.\n");
        exit(4);
    }

    if (TRIP_DURATION < 1) {
        fprintf(stderr, "The trip cannot last less than 1.\n");
        exit(5);
    }

    if (TIME_BETWEEN_TRIPS < 1) {
        fprintf(stderr, "The time between trips cannot be less than 1.\n");
        exit(6);
    }

    if (NUMBER_OF_TRIPS_PER_DAY < 1) {
        fprintf(stderr, "The number of trips per day must be greater than 0.\n");
        exit(7);
    }

    printf("All parameters have been correctly defined.\n");
}

int main() {
    handleInput();
}
