#include "utils.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <shmid> <semid>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shmid = atoi(argv[1]);
    int semid = atoi(argv[2]);

    launchHarbourCaptain(shmid, semid);
    
    return 0;
}


void launchHarbourCaptain(int shmid, int semid) {
    SharedMemory *sm = attachSharedMemory(shmid);
    
    if (sm == (void *)-1) {
        perror("shmat habour captain");
        exit(EXIT_FAILURE);
    }

    printf("=== Habour Captain === Starting\n");
    printf("=== Habour Captain === Enter: w = early cruise, k = end of day, q = exit\n");

    char c;
    while (1) {
        c = getchar();
        if (c == '\n') continue;

        waitSemaphore(semid, SEM_MUTEX);
        if (c == 'w') {
            kill(sm->shipCaptainPid, SIGUSR1); // Send SIGUSR1 for early departure
            printf("=== Habour Captain === Early departure signal sent\n");
        } else if (c == 'k') {
            kill(sm->shipCaptainPid, SIGUSR2); // Send SIGUSR2 for end-of-day
            printf("=== Habour Captain === End-of-day signal sent\n");
            break;
        } else if (c == 'q') {
            signalSemaphore(semid, SEM_MUTEX);
            printf("=== Habour Captain === Exiting\n");
            break;
        } else {
            printf("=== Habour Captain === Unknown command\n");
        }
        signalSemaphore(semid, SEM_MUTEX); 

        // input validation not strong enough probably
        while (getchar() != '\n'); // Clearing buff
    }

    shmdt(sm);
}
