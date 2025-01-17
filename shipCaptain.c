#include "utils.h"
#include "bridge_queue.h"
#include "shipCaptain.h"

volatile sig_atomic_t endOfDaySignal = 0; // Flag for sigusr2
SharedMemory *sm;

int shmid, semid;
int earlyVoyage = 0;

#define MAX_WAITING 2000

// Data for handling queues
static int msq_id = -1; // Queue ID
static int globalSequenceCounter = 0; // Starting from 0, increments
static int nextSequenceToBoard = 0; // Who is next to board the ship
static pid_t waitingArray[MAX_WAITING]; // waitingArray[seq] = Passenger's PID (or 0)


// Main Function
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, RED "Usage: %s <shmid> <semid>" RESET "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shmid = atoi(argv[1]);
    semid = atoi(argv[2]);

    printf(YELLOW "=== Ship Captain ===" RESET " Starting\n");

    sm = attachSharedMemory(shmid);
    if (sm == (void *)-1) {
        perror("shmat main");
        exit(EXIT_FAILURE);
    }

    sendPID();
    initializeMessageQueue();
    setupSignalHandlers();

    while (1) {
        handleBridgeQueue();

        performLoading(); // change to perform all??
    }

    cleanupAndExit();
    return 0;
}


// Check end-of-day or early voyage conditions
int isEndOfDayOrEarlyVoyage() {
    waitSemaphore(semid, SEM_MUTEX);
    int endOfDay = sm->signalEndOfDay;
    signalSemaphore(semid, SEM_MUTEX);

    if (endOfDay) {
        printf(YELLOW "=== Ship Captain ===" RESET " End-of-day signal received. Preparing for disembarking.\n");

        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 1;
        signalSemaphore(semid, SEM_MUTEX);

        waitForAllPassengersToDisembark();
        printf(YELLOW "=== Ship Captain ===" RESET " Ending day as per signal.\n");

        cleanupAndExit();
    }

    if (earlyVoyage) {
        printf(YELLOW "=== Ship Captain ===" RESET " Early departure signal received.\n");
        earlyVoyage = 0;

        return 1;
    }

    return 0; 
}

// Handle passenger messages and queues
void handleBridgeQueue() {
    BridgeMsg msg;

    while (1) {
        ssize_t rcv = msgrcv(msq_id, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);
        if (rcv == -1) {
            if (errno == ENOMSG) break;
            perror(RED "msgrcv handleBridgeQueue" RESET);
        }

       if (msg.mtype == MSG_ENTER_BRIDGE) {
            // Passenger entering the bridge and asks for a sequence number
            int seq = globalSequenceCounter++;
            printf(YELLOW "=== ShipCaptain ===" RESET " Passenger %d enters the bridge -> assigned seq=%d\n", msg.pid, seq);

            // Sending back to the passenger reply
            BridgeMsg reply;
            reply.mtype = MSG_SEQUENCE_REPLY;
            reply.pid = msg.pid;
            reply.sequence = seq;

            if (msgsnd(msq_id, &reply, sizeof(reply) - sizeof(long), 0) == -1) {
                perror(RED "msgsnd MSG_SEQUENCE_REPLY" RESET);
            }
        } else if (msg.mtype == MSG_WANT_TO_BOARD) {
            pid_t pid = msg.pid;
            int seq = msg.sequence;

            printf(YELLOW "=== Ship Captain ===" RESET " Passenger %d wants to board (seq=%d). nextSequenceToBoard=%d\n", pid, seq, nextSequenceToBoard);

            // Check if this passenger is "next to board"
            if (seq == nextSequenceToBoard) {
                // Can board immediately
                BridgeMsg ok;
                ok.mtype = pid;
                ok.pid = pid;
                ok.sequence = seq;

                if (msgsnd(msq_id, &ok, sizeof(ok) - sizeof(long), 0) == -1) {
                    perror("msgsnd MSG_BOARDING_OK immediate");
                }
                usleep(30); // synchronizacja wyjscia

                nextSequenceToBoard++;
                checkAndBoardNextInQueue();
            } else {
                waitingArray[seq] = pid;
                printf(YELLOW "=== Ship Captain ===" RESET " Passenger %d queued for boarding (seq=%d)\n", pid, seq);
            }
        } else {
            // Other types of messages - i just ignore them
            fprintf(stderr, RED "=== ShipCaptain === Unknown message type=%ld\n" RESET, msg.mtype);
        }
    }
}

// Check and board the next passenger in the queue
void checkAndBoardNextInQueue() {
    while (nextSequenceToBoard < MAX_WAITING && waitingArray[nextSequenceToBoard] != 0) {
        pid_t pid = waitingArray[nextSequenceToBoard];
        waitingArray[nextSequenceToBoard] = 0;

        // Send a message to the passenger: "You may board" (MSG_BOARDING_OK)
        BridgeMsg ok;
        ok.mtype = pid;
        ok.pid = pid;
        ok.sequence = nextSequenceToBoard; // informational

        if (msgsnd(msq_id, &ok, sizeof(ok) - sizeof(long), 0) == -1) {
            perror("msgsnd MSG_BOARDING_OK");
        }

        printf(YELLOW "=== Ship Captain ===" RESET " Passenger %d with seq=%d -> boarded!\n", pid, nextSequenceToBoard);

        nextSequenceToBoard++;
    }
}

void performLoading() { // perform all xd
    int slept = 0; 
    while (slept < TIME_BETWEEN_TRIPS * 1000) {
        handleBridgeQueue();

        if(isEndOfDayOrEarlyVoyage()) {
            break;
        }

        usleep(1);
        slept++;
    }

    startCruisePreparation();
    performVoyage();
    performDisembarkation();
    getReadyForNextCruise();
}


// Prepare for the cruise
void startCruisePreparation() {
    waitSemaphore(semid, SEM_MUTEX);
    sm->queueDirection = 1;
    sm->shipSailing = 1;
    signalSemaphore(semid, SEM_MUTEX);

    printf(YELLOW "=== Ship Captain ===" RESET " All the people on the bridge have to go ashore, we are sailing away!\n");

    // Waiting for all passengers to get off the bridge
    while (1) { 
        waitSemaphore(semid, SEM_MUTEX);
        int peopleOnBridge = sm->peopleOnBridge;
        signalSemaphore(semid, SEM_MUTEX);
        printf("ppl on bridge: %d\n", peopleOnBridge);
        if (peopleOnBridge == 0) {
            break;
        }

        // usleep(100000); // 0.1s
    }

    waitSemaphore(semid, SEM_MUTEX);
    int voyageNumber = sm->currentVoyage + 1;
    int peopleOnVoyage = sm->peopleOnShip;
    signalSemaphore(semid, SEM_MUTEX);

    printf(YELLOW "=== Ship Captain ===" RESET " All passengers have descended. We sail away.\n");
    printf(YELLOW "=== Ship Captain ===" RESET " Sailing on cruise %d with %d passengers.\n", voyageNumber, peopleOnVoyage);
}

// Perform the cruise
void performVoyage() {
    waitSemaphore(semid, SEM_MUTEX);
    int voyageNumber = sm->currentVoyage + 1;
    signalSemaphore(semid, SEM_MUTEX);

    printf(YELLOW "=== Ship Captain ===" RESET " Starting voyage %d.\n", voyageNumber);

    //Simulation of cruise
    struct timespec req, rem;
    req.tv_sec = TRIP_DURATION;
    req.tv_nsec = 0;

    // Loop to ensure the full trip duration is completed
    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            printf(YELLOW "=== Ship Captain ===" RESET " Signal received during voyage, resuming sleep...\n");
            req = rem; // Use remaining time to continue sleeping
        } else {
            perror(RED "nanosleep" RESET);
            exit(EXIT_FAILURE);
        }
    }
}


void performDisembarkation() {
    // End-of-day received during cruise, disembark all passengers and end
    if (endOfDaySignal) {
        printf(YELLOW "=== Ship Captain ===" RESET " End-of-day signal received during voyage. Ending day as per signal.\n");

        waitSemaphore(semid, SEM_MUTEX);
        sm->signalEndOfDay = 1;
        sm->queueDirection = 1; // queue towards land, so passenger can't enter
        signalSemaphore(semid, SEM_MUTEX);

        cleanupAndExit();
    }

    waitSemaphore(semid, SEM_MUTEX);
    sm->shipSailing = 0; // end of cruise
    sm->queueDirection = 1; // towards land to disembark
    sm->currentVoyage++;
    int voyageNumber = sm->currentVoyage + 1;
    signalSemaphore(semid, SEM_MUTEX);

    printf(YELLOW "=== Ship Captain ===" RESET " Cruise %d has ended. Arriving at port.\n", voyageNumber);

    // Reset waiting queue
    memset(waitingArray, 0, sizeof(waitingArray));
    globalSequenceCounter = 0;
    nextSequenceToBoard = 0; // reset numbers

    waitForAllPassengersToDisembark();
}

void getReadyForNextCruise() {
    waitSemaphore(semid, SEM_MUTEX);
    if (sm->currentVoyage >= NUMBER_OF_TRIPS_PER_DAY) {
        sm->queueDirection = 1;
        sm->signalEndOfDay = 1;
        signalSemaphore(semid, SEM_MUTEX);
        printf(YELLOW "=== Ship Captain ===" RESET " Reached daily trip limit %d. Ending work.\n", NUMBER_OF_TRIPS_PER_DAY);
        sendStopSignal();
        cleanupAndExit();
    }
    signalSemaphore(semid, SEM_MUTEX);

    // Change bridge direction again
    waitSemaphore(semid, SEM_MUTEX);
    sm->queueDirection = 0; // towards ship, getting ready for next voyage
    signalSemaphore(semid, SEM_MUTEX);

    printf(YELLOW "=== Ship Captain ===" RESET " Bridge direction set back to boarding for the next voyage.\n");
}

// Wait for all passengers to disembark
void waitForAllPassengersToDisembark() {
    while (1) {
        waitSemaphore(semid, SEM_MUTEX);
        int peopleOnShip = sm->peopleOnShip;
        int peopleOnBridge = sm->peopleOnBridge;
        signalSemaphore(semid, SEM_MUTEX);

        if (peopleOnShip == 0 && peopleOnBridge == 0) {
            printf(YELLOW "=== Ship Captain ===" RESET " All passengers have disembarked.\n");
            break;
        } 
    }
}


// Send PID to harbour captain
void sendPID() {
    int fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open fifo");
        exit(EXIT_FAILURE);
    }

    pid_t myPID = getpid();

    // Writing PID to FIFO
    if (write(fifo_fd, &myPID, sizeof(myPID)) == -1) {
        perror(RED "write to FIFO" RESET);
        exit(EXIT_FAILURE);
    }


    printf(YELLOW "=== Ship Captain ===" RESET " PID was sent to the harbour captain.\n");
    close(fifo_fd); // Closing FIFO
}

// Initialize the message queue
void initializeMessageQueue() {
    msq_id = msgget(BRIDGE_QUEUE_KEY, IPC_CREAT | MSG_PERMISSIONS);
    if (msq_id == -1) {
        perror(RED "msgget shipCaptain" RESET);
        exit(EXIT_FAILURE);
    }
    memset(waitingArray, 0, sizeof(waitingArray));
}

// Handle signals for early voyage or end of day
void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        waitSemaphore(semid, SEM_MUTEX);
        if (sm->shipSailing) {
            printf(YELLOW "=== Ship Captain ===" RESET " I'm currently sailing, the signal cannot be made.\n");
        } else {
            earlyVoyage = 1;
            sm->queueDirection = 1;
            sm->shipSailing = 1;
        }
        signalSemaphore(semid, SEM_MUTEX);
    } else if (sig == SIGUSR2) {
        sendStopSignal();

        waitSemaphore(semid, SEM_MUTEX);
        if (!sm->shipSailing) {
            sm->signalEndOfDay = 1;
        } else {
            endOfDaySignal = 1;
        }
        signalSemaphore(semid, SEM_MUTEX);
    }
}

// Setup signal handlers for SIGUSR1 and SIGUSR2
void setupSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror(RED "sigaction SIGUSR1" RESET);
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror(RED "sigaction SIGUSR2" RESET);
        exit(EXIT_FAILURE);
    }
}


// Send the stop signal to passengers
void sendStopSignal() {
    int fifo_fd = open(FIFO_PATH_PASSENGERS, O_WRONLY);
    if (fifo_fd == -1) {
        perror(RED "open FIFO ship" RESET);
        exit(EXIT_FAILURE);
    }

    const char *msg = "stop";
    if (write(fifo_fd, msg, strlen(msg) + 1) == -1) {
        perror(RED "write to FIFO" RESET);
    }
    close(fifo_fd);
}

// Cleanup resources and exit
void cleanupAndExit() {
    shmdt(sm);
    printf(YELLOW "=== Ship Captain ===" RESET " Cleanup complete. Exiting.\n");
    exit(0);
}