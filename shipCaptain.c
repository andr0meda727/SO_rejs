#include "utils.h"
#include "bridge_queue.h"
#include "shipCaptain.h"


volatile sig_atomic_t endOfDaySignal = 0; // Flag for sigusr2
SharedMemory *sm;


int shmid, semid, msq_id;
int earlyVoyage = 0;
int signalReceived = 0;
#define MAX_WAITING 2000

// Data for handling queues
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
        performCruiseOperations();
    }

    cleanupAndExit();
    return 0;
}


// Check end-of-day or early voyage conditions
void EndOfDayOrEarlyVoyage() {
    waitSemaphore(semid, SEM_MUTEX);
    int endOfDay = sm->signalEndOfDay;
    signalSemaphore(semid, SEM_MUTEX);

    if (endOfDay) {
        printf(YELLOW "=== Ship Captain ===" RESET " End-of-day signal received. Preparing for disembarking.\n");

        waitForAllPassengersToDisembark();
        printf(YELLOW "=== Ship Captain ===" RESET " Ending day as per signal.\n");

        cleanupAndExit();
    }

    if (earlyVoyage) {
        printf(YELLOW "=== Ship Captain ===" RESET " Early departure signal received.\n");
        earlyVoyage = 0;
    }

    signalReceived = 1;
}

// Handle passenger messages and queues
void handleBridgeQueue() {
    BridgeMsg msg;
    int processed = 0;

    while (processed < 20) {
        ssize_t rcv = msgrcv(msq_id, &msg, sizeof(msg) - sizeof(long), -2, IPC_NOWAIT);
        if (rcv == -1) {
            if (errno == ENOMSG) break;
            perror(RED "msgrcv handleBridgeQueue" RESET);
        }

       if (msg.mtype == MSG_ENTER_BRIDGE) {
            // Passenger entering the bridge and asks for a sequence number
            int seq = globalSequenceCounter++;
            printf(YELLOW "=== Ship Captain ===" RESET " Passenger %d enters the bridge -> assigned seq=%d\n", msg.pid, seq);
            
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
            waitSemaphore(semid, SEM_MUTEX);

            // We check if the passenger is the “next in line”
            // and whether SHIP_CAPACITY has not yet been exceeded.
            if (seq == nextSequenceToBoard && sm->peopleOnShip < SHIP_CAPACITY) {
                // Passenger can enter
                sm->peopleOnShip++;
                sm->peopleOnBridge--;
                int currentVoyage = sm->currentVoyage + 1;

                printf(CYAN "=== Passenger %d ===" RESET " Boarded the ship (voyage no. %d). PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n", pid, currentVoyage, sm->peopleOnShip, sm->peopleOnBridge);
                signalSemaphore(semid, SEM_MUTEX); 
                BridgeMsg ok;
                ok.mtype = pid;
                ok.pid = pid;
                ok.sequence = seq;

                if (msgsnd(msq_id, &ok, sizeof(ok) - sizeof(long), 0) == -1) {
                    perror("msgsnd MSG_BOARDING_OK immediate");
                }

                nextSequenceToBoard++;
                checkAndBoardNextInQueue(); 
            }
            else if (sm->peopleOnShip >= SHIP_CAPACITY) {
                // Passenger can't enter, ship full
                sm->shipSailing = 1;
                sm->queueDirection = 1;
                signalSemaphore(semid, SEM_MUTEX);

                BridgeMsg den;
                den.mtype = pid; 
                den.pid = pid;
                den.sequence = -1; // denied

                if (msgsnd(msq_id, &den, sizeof(den) - sizeof(long), 0) == -1) {
                    perror("msgsnd MSG_BOARDING_DENIED");
                }
            }
            else if (seq < nextSequenceToBoard) {
                // Old seq number, passenger late, shouldnt happen
                signalSemaphore(semid, SEM_MUTEX);
                fprintf(stderr, RED "=== Ship Captain ===" RESET " WARNING: passenger %d has old seq=%d\n", pid, seq);
            }
            else {
                // seq > nextSequenceToBoard => passenger queued
                if (seq >= MAX_WAITING) {
                    signalSemaphore(semid, SEM_MUTEX);
                    fprintf(stderr, RED "=== ShipCaptain ===" RESET " ERROR: seq=%d too large.\n", seq);
                } else {
                    waitingArray[seq] = pid;
                    printf(YELLOW "=== ShipCaptain ===" RESET " Passenger %d queued for boarding (seq=%d)\n", pid, seq);
                    signalSemaphore(semid, SEM_MUTEX);
                }
            }
        } else {
            // Other types of messages - i just ignore them
            fprintf(stderr, RED "=== Ship Captain === Unknown message type=%ld\n" RESET, msg.mtype);
        }
        processed++;
    }
}

// Check and board the next passenger in the queue
void checkAndBoardNextInQueue() {
    while (nextSequenceToBoard < MAX_WAITING && waitingArray[nextSequenceToBoard] != 0) {
        pid_t pid = waitingArray[nextSequenceToBoard];
        waitingArray[nextSequenceToBoard] = 0;

        if (nextSequenceToBoard < SHIP_CAPACITY) {
            // Send a message to the passenger: "You may board" (MSG_BOARDING_OK)
            BridgeMsg ok;
            ok.mtype = pid;
            ok.pid = pid;
            ok.sequence = nextSequenceToBoard; // informational

            if (msgsnd(msq_id, &ok, sizeof(ok) - sizeof(long), 0) == -1) {
                perror("msgsnd MSG_BOARDING_OK");
            }

            waitSemaphore(semid, SEM_MUTEX);
            int newShipCount = ++(sm->peopleOnShip);
            int newBridgeCount = --(sm->peopleOnBridge); 
            int currentVoyage = sm->currentVoyage + 1;
            signalSemaphore(semid, SEM_MUTEX);

            printf(CYAN "=== Passenger %d ===" RESET " Boarded the ship (voyage no. %d). PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n", pid, currentVoyage, newShipCount, newBridgeCount);

            nextSequenceToBoard++;
        } else {
            BridgeMsg den;
            den.mtype = pid; 
            den.pid = pid;
            den.sequence = -1; // denied

            if (msgsnd(msq_id, &den, sizeof(den) - sizeof(long), 0) == -1) {
                perror("msgsnd MSG_BOARDING_DENIED");
            }

            nextSequenceToBoard++;
        }
    }
}

void performCruiseOperations() {
    struct timeval start, current;
    long long elapsedSeconds = 0;

    // Timer to allow proper loading
    gettimeofday(&start, NULL); 
    while (elapsedSeconds < TIME_BETWEEN_TRIPS) {
        handleBridgeQueue();

        if(signalReceived) {
            signalReceived = 0;
            break;
        }
        
        gettimeofday(&current, NULL);
        elapsedSeconds = current.tv_sec - start.tv_sec; // diff in seconds
    }

    startCruisePreparation();
    performVoyage();
    performDisembarkation();
    getReadyForNextCruise();
}


// Prepare for the cruise
void startCruisePreparation() {
    printf(YELLOW "=== Ship Captain ===" RESET " All the people on the bridge have to go ashore, we are sailing away!\n");

    waitSemaphore(semid, SEM_MUTEX);
    sm->queueDirection = 1;
    sm->shipSailing = 1;
    signalSemaphore(semid, SEM_MUTEX);

    dumpPassengersFromWaitingArray();

    // Waiting for all passengers to get off the bridge
    while (1) { 
        handleBridgeQueue(); // clear message queue

        waitSemaphore(semid, SEM_MUTEX);
        int peopleOnBridge = sm->peopleOnBridge;
        signalSemaphore(semid, SEM_MUTEX);
        if (peopleOnBridge == 0) {
            break;
        }
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

    printf(YELLOW "=== Ship Captain ===" RESET " Starting voyage %d. TRIP DURATION: %ds\n", voyageNumber, TRIP_DURATION);

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
    int voyageNumber = ++(sm->currentVoyage);
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
    int currentVoyage = sm->currentVoyage;
    signalSemaphore(semid, SEM_MUTEX);

    if (currentVoyage >= NUMBER_OF_TRIPS_PER_DAY) {
        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 1;
        sm->signalEndOfDay = 1;
        signalSemaphore(semid, SEM_MUTEX);
        printf(YELLOW "=== Ship Captain ===" RESET " Reached daily trip limit %d. Ending work.\n", NUMBER_OF_TRIPS_PER_DAY);
        sendStopSignal();
        cleanupAndExit();
    }

    // Change bridge direction again
    waitSemaphore(semid, SEM_MUTEX);
    sm->queueDirection = 0; // towards ship, getting ready for next voyage
    signalSemaphore(semid, SEM_MUTEX);

    printf(YELLOW "=== Ship Captain ===" RESET " Bridge direction set back to boarding for the next voyage.\n");
}

// Wait for all passengers to disembark
void waitForAllPassengersToDisembark() {
    while (1) {
        handleBridgeQueue(); // clear message queue
        waitSemaphore(semid, SEM_MUTEX);
        int peopleOnShip = sm->peopleOnShip;
        int peopleOnBridge = sm->peopleOnBridge;
        signalSemaphore(semid, SEM_MUTEX);

        dumpPassengersFromWaitingArray();

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
        if (sm->shipSailing == 1) {
            printf(YELLOW "=== Ship Captain ===" RESET " I'm currently sailing, the signal cannot be made.\n");
            signalSemaphore(semid, SEM_MUTEX);
        } else if (sm->shipSailing == 0 && sm->queueDirection == 0) {
            earlyVoyage = 1;
            sm->queueDirection = 1;
            sm->shipSailing = 1;
            signalSemaphore(semid, SEM_MUTEX);
        } else {
            sm->queueDirection = 1;
            sm->shipSailing = 1;
            signalSemaphore(semid, SEM_MUTEX);
            waitForAllPassengersToDisembark();
            earlyVoyage = 1;
        }
    } else if (sig == SIGUSR2) {
        sendStopSignal();

        waitSemaphore(semid, SEM_MUTEX);
        int shipSailing = sm->shipSailing;
        signalSemaphore(semid, SEM_MUTEX);

        if (!shipSailing) {
            waitSemaphore(semid, SEM_MUTEX);
            sm->queueDirection = 1;
            sm->signalEndOfDay = 1;
            signalSemaphore(semid, SEM_MUTEX);
        } else {
            endOfDaySignal = 1;
        }
    }

    EndOfDayOrEarlyVoyage();
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
    exit(0);
}

void dumpPassengersFromWaitingArray() {
    for (int y = 0; y < globalSequenceCounter; y++) {
        pid_t pid = waitingArray[y];
        if (pid != 0) {
            BridgeMsg den;
            den.mtype = pid;
            den.pid = pid;
            den.sequence = -1; // denied
            msgsnd(msq_id, &den, sizeof(den) - sizeof(long), 0);
            waitingArray[y] = 0;
        }
    }
}