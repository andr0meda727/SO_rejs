#include "utils.h"
#include "bridge_queue.h"

#define NUM_PASSENGERS 1000

int shmid, semid, msq_id;
SharedMemory *sm;

void signalHandler(int sig) {
    // Cleaning after ctrl+c

    if (sig == SIGINT) {
        if (sm != NULL) {
            shmdt(sm);
        }

        cleanupSharedMemory(shmid);
        cleanupSemaphores(semid);
        if (msgctl(msq_id, IPC_RMID, NULL) == -1) {
            perror("msgctl IPC_RMID");
        }
        unlink(FIFO_PATH); // Delete FIFO file
        unlink(FIFO_PATH_PASSENGERS); // Delete FIFO file
        printf(GREEN "Cleanup complete, exiting.\n" RESET);
        exit(0);
    }
}

int main() {
    if (signal(SIGINT, signalHandler) == SIG_ERR) {
        perror(RED "Signal handler setup failed" RESET);
        exit(1);
    }

    handleInput();

    shmid = initializeSharedMemory();
    sm = attachSharedMemory(shmid);
    semid = initializeSemaphores();
    msq_id = msgget(BRIDGE_QUEUE_KEY, IPC_CREAT | MSG_PERMISSIONS);

    // Create FIFO for communication from ship captain
    if (mkfifo(FIFO_PATH, 0600) == -1  && errno != EEXIST) {
        perror(RED "mkfifo fifo_path" RESET);
        exit(EXIT_FAILURE);
    }

    // FIFO for passengers
    if (mkfifo(FIFO_PATH_PASSENGERS, 0600) == -1 && errno != EEXIST) {
        perror(RED "mkfifo fifo_path_passengers" RESET);
        exit(EXIT_FAILURE);
    }

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

    pid_t shipCaptainPid = fork();
    if (shipCaptainPid == -1) {
        perror(RED "Error forking for shipCaptain" RESET);
        exit(EXIT_FAILURE);
    } else if (shipCaptainPid == 0) {
        if (execl("./shipCaptain", "shipCaptain", shmStr, semStr, NULL) == -1) {
            perror(RED "execl shipCaptain" RESET);
            exit(EXIT_FAILURE);
        }
    }

    pid_t harbourCaptainPid = fork();
    if (harbourCaptainPid == -1) {
        perror(RED "Error forking for harbourCaptain" RESET);
        exit(EXIT_FAILURE);
    } else if (harbourCaptainPid == 0) {
        if (execl("./harbourCaptain", "harbourCaptain", NULL) == -1) {
            perror(RED "execl harbourCaptain" RESET);
            exit(EXIT_FAILURE);
        }
    }

    // Open FIFO for reading
    int fifo_fd = open(FIFO_PATH_PASSENGERS, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror(RED "open FIFO rejs" RESET);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    for (int i = 0; i < 500; i++) {
        char buffer[10]; // Buffer for message
        ssize_t bytesRead = read(fifo_fd, buffer, sizeof(buffer));

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; 
            if (strcmp(buffer, "stop") == 0) {
                printf(YELLOW "Received 'stop' signal from shipCaptain. Stopping passenger creation.\n" RESET);
                break;
            }
        }

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
        // usleep(((rand() % 10) + 100) * 10); // <0.1s; 2s>
    }

    while (wait(NULL) > 0) {}

    // Cleanup
    shmdt(sm);
    cleanupSharedMemory(shmid);
    cleanupSemaphores(semid);
    if (msgctl(msq_id, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID");
    }

    close(fifo_fd);
    unlink(FIFO_PATH); // Delete FIFO file
    unlink(FIFO_PATH_PASSENGERS);
    printf(GREEN "Main process finished. Cleaned up shared memory and semaphores." RESET "\n");
    return 0;
}
