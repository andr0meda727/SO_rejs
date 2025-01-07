#ifndef PASSENGER_H
#define PASSENGER_H

#include "utils.h"

typedef struct {
    int shmid;
    int semid;
    int idPasazera;
} PassengerData;


void *Passenger(void *arg);

#endif 
