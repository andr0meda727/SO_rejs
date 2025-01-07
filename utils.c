#include "utils.h"

int waitSemaphore(int semID, int number) {
   struct sembuf operation;
   operation.sem_num = number;
   operation.sem_op = -1;
   operation.sem_flg = 0;

   if (semop(semID, &operation, 1) == -1) {
      perror("Error in waitSemaphore");
      exit(EXIT_FAILURE);
   }

}

void signalSemaphore(int semID, int number) {
   struct sembuf operation;
   operation.sem_num = number;
   operation.sem_op = 1;
   operation.sem_flg = 0;

   if (semop(semID, &operation, 1) == -1) {
        perror("Error in signalSemaphore");
        exit(EXIT_FAILURE);
   }
}

int initializeSemaphores() {
    key_t semKey = ftok(".", SEM_PROJECT_ID);
    if (semKey == -1) {
        perror("ftok for sem");
        exit(EXIT_FAILURE);
    }

    int semid = semget(semKey, 2, IPC_CREAT | IPC_EXCL | 0600);
    if (semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    unsigned short initValues[2];
    initValues[SEM_MUTEX] = 1;
    initValues[SEM_BRIDGE] = BRIDGE_CAPACITY;

    if (semctl(semid, 0, SETALL, initValues) == -1) {
        perror("semctl SETALL");
        exit(EXIT_FAILURE);
    }

    printf("Semaphores created and initialized.\n");
    
    return semid;
}

void cleanupSemaphores(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    }
}

int initializeSharedMemory() {
    key_t memoryKey = ftok(".", SHM_PROJECT_ID);
    if (memoryKey == -1) {
        perror("ftok for shm");
        exit(EXIT_FAILURE);
    }

    int shmid = shmget(memoryKey, sizeof(SharedMemory), IPC_CREAT | IPC_EXCL | 0600);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    return shmid;
}

void cleanupSharedMemory(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }
}

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

SharedMemory* attachSharedMemory(int shmid) {
    SharedMemory* sm = (SharedMemory *)shmat(shmid, NULL, 0);
    if (sm == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    return sm;
}

pid_t createHarbourCaptain(int shmid, int semid) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork for harbourCaptain");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        launchHabourCaptain(shmid, semid);
        exit(EXIT_SUCCESS);
    }
    return pid;
}

pid_t createShipCaptain(int shmid, int semid) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork for shipCaptain");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        launchShipCaptain(shmid, semid);
        exit(EXIT_SUCCESS);
    }
    return pid;
}