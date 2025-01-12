#include "utils.h"

#define NUM_PASSENGERS 100

int shmid, semid;
SharedMemory *sm;

void signalHandler(int sig) {
    // Cleaning after ctrl+c
    if (sm != NULL) {
        shmdt(sm);
    }
    cleanupSharedMemory(shmid);
    cleanupSemaphores(semid);
    printf(GREEN "Cleanup complete, exiting.\n" RESET);
    exit(0);
}

int main() {
    handleInput();

    // pipefd[0] - read
    // pipefd[1] - write
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror(RED "pipe" RESET);
        exit(EXIT_FAILURE);
    }

    shmid = initializeSharedMemory();
    sm = attachSharedMemory(shmid);
    semid = initializeSemaphores();

    char shmStr[16], semStr[16];
    sprintf(shmStr, "%d", shmid);
    sprintf(semStr, "%d", semid);

    // Shared memory initialization
    waitSemaphore(semid, SEM_MUTEX);
    sm->peopleOnShip = 0;
    sm->peopleOnBridge = 0;
    sm->currentVoyage = 0;
    sm->signalEndOfDay = 0;
    sm->queueDirection = 0;
    sm->shipSailing = 0;
    signalSemaphore(semid, SEM_MUTEX);

    if (signal(SIGINT, signalHandler) == SIG_ERR) {
        perror(RED "Signal handler setup failed" RESET);
        exit(1);
    }

    pid_t shipCaptainPid = fork();
    if (shipCaptainPid == -1) {
        perror(RED "Error forking for shipCaptain" RESET);
        exit(EXIT_FAILURE);
    } else if (shipCaptainPid == 0) {
        close(pipefd[0]); // Close reading
        char writeFdStr[10];
        sprintf(writeFdStr, "%d", pipefd[1]); // We pass the writing descriptor
        if (execl("./shipCaptain", "shipCaptain", shmStr, semStr, writeFdStr, NULL) == -1) {
            perror(RED "execl shipCaptain" RESET);
            exit(EXIT_FAILURE);
        }
    }

    pid_t harbourCaptainPid = fork();
    if (harbourCaptainPid == -1) {
        perror(RED "Error forking for harbourCaptain" RESET);
        exit(EXIT_FAILURE);
    } else if (harbourCaptainPid == 0) {
        close(pipefd[1]); // Close writing
        char readFdStr[10];
        sprintf(readFdStr, "%d", pipefd[0]); // We pass the reading descriptor
        if (execl("./harbourCaptain", "harbourCaptain", readFdStr, NULL) == -1) {
            perror(RED "execl harbourCaptain" RESET);
            exit(EXIT_FAILURE);
        }
    }

    close(pipefd[0]); // We close both ends of the pipe in the main process
    close(pipefd[1]);

    srand(time(NULL));

    for (int i = 0; i < NUM_PASSENGERS; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror(RED "Error forking passenger" RESET);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            if (execl("./passenger", "passenger", shmStr, semStr, NULL) == -1) {
                perror(RED "execl passenger" RESET);
                exit(EXIT_FAILURE);
            }
        }
        usleep(((rand() % 1901) + 100) * 1000); // <0.1s; 2s>
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
