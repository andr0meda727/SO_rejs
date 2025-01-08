#include "utils.h"

void waitSemaphore(int semID, int number) {
   struct sembuf operation;
   operation.sem_num = number;
   operation.sem_op = -1;
   operation.sem_flg = 0;

   if (semop(semID, &operation, 1) == -1) {
      perror(RED "Error in waitSemaphore" RESET);
      exit(EXIT_FAILURE);
   }
}

void signalSemaphore(int semID, int number) {
   struct sembuf operation;
   operation.sem_num = number;
   operation.sem_op = 1;
   operation.sem_flg = 0;

   if (semop(semID, &operation, 1) == -1) {
        perror(RED "Error in signalSemaphore" RESET);
        exit(EXIT_FAILURE);
   }
}

int initializeSemaphores() {
    key_t semKey = ftok(".", SEM_PROJECT_ID);
    if (semKey == -1) {
        perror(RED "ftok for sem" RESET);
        exit(EXIT_FAILURE);
    }

    int semid = semget(semKey, 2, IPC_CREAT | IPC_EXCL | 0600);
    if (semid == -1) {
        perror(RED "semget" RESET);
        exit(EXIT_FAILURE);
    }

    unsigned short initValues[2];
    initValues[SEM_MUTEX] = 1;
    initValues[SEM_BRIDGE] = BRIDGE_CAPACITY;

    if (semctl(semid, 0, SETALL, initValues) == -1) {
        perror(RED "semctl SETALL" RESET);
        exit(EXIT_FAILURE);
    }

    printf(GREEN "Semaphores created and initialized." RESET "\n");
    
    return semid;
}

void cleanupSemaphores(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror(RED "semctl IPC_RMID" RESET);
    } else {
        printf(GREEN "Semaphores cleaned up successfully." RESET "\n");
    }
}

int initializeSharedMemory() {
    key_t memoryKey = ftok(".", SHM_PROJECT_ID);
    if (memoryKey == -1) {
        perror(RED "ftok for shm" RESET);
        exit(EXIT_FAILURE);
    }

    int shmid = shmget(memoryKey, sizeof(SharedMemory), IPC_CREAT | IPC_EXCL | 0600);
    if (shmid == -1) {
        perror(RED "shmget" RESET);
        exit(EXIT_FAILURE);
    }

    printf(GREEN "Shared memory segment created successfully." RESET "\n");
    return shmid;
}

void cleanupSharedMemory(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror(RED "shmctl" RESET);
    } else {
        printf(GREEN "Shared memory segment cleaned up successfully." RESET "\n");
    }
}

void handleInput() {
    if (SHIP_CAPACITY <= BRIDGE_CAPACITY) {
        fprintf(stderr, RED "The bridge capacity must be smaller than the ship capacity." RESET "\n");
        exit(1);
    }

    if (BRIDGE_CAPACITY < 1) {
        fprintf(stderr, RED "The bridge capacity must be greater than 0." RESET "\n");
        exit(2);
    }

    if (SHIP_CAPACITY < 1) {
        fprintf(stderr, RED "The ship capacity must be greater than 0." RESET "\n");
        exit(3);
    }

    if (TIME_BETWEEN_TRIPS <= TRIP_DURATION) {
        fprintf(stderr, RED "The trip duration must be shorter than the time between trips." RESET "\n");
        exit(4);
    }

    if (TRIP_DURATION < 1) {
        fprintf(stderr, RED "The trip cannot last less than 1." RESET "\n");
        exit(5);
    }

    if (TIME_BETWEEN_TRIPS < 1) {
        fprintf(stderr, RED "The time between trips cannot be less than 1." RESET "\n");
        exit(6);
    }

    if (NUMBER_OF_TRIPS_PER_DAY < 1) {
        fprintf(stderr, RED "The number of trips per day must be greater than 0." RESET "\n");
        exit(7);
    }

    printf(GREEN "All parameters have been correctly defined." RESET "\n");
}

SharedMemory* attachSharedMemory(int shmid) {
    SharedMemory* sm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (sm == (void *)-1) {
        perror(RED "shmat" RESET);
        exit(EXIT_FAILURE);
    }
    printf(GREEN "Shared memory attached successfully." RESET "\n");
    return sm;
}
