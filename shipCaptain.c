#include "utils.h"
#include "bridge_queue.h"

volatile sig_atomic_t endOfDaySignal = 0; // Flag for sigusr2

int shmid, semid;
int earlyVoyage = 0;

#define MAX_WAITING 2000

// Data for handling queues
static int msq_id = -1; // Queue ID
static int globalSequenceCounter = 0; // Starting from 0, increments
static int nextSequenceToBoard = 0; // Who is next to board the ship
static pid_t waitingArray[MAX_WAITING]; // waitingArray[seq] = Passenger's PID (or 0)


// The function checks if there is a passenger in the array with `sequence = nextSequenceToBoard`.
// If yes - it assigns him a boarding and increments `nextSequenceToBoard`,
// as long as it finds another waiting.
static void checkAndBoardNextInQueue(void) {
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

        printf(YELLOW "=== ShipCaptain ===" RESET " Passenger %d with seq=%d -> boarded!\n", pid, nextSequenceToBoard);

        nextSequenceToBoard++;
    }
}

// Passenger message handling function (non-blocking, so as not to block the loop)
static void handleBridgeQueue(void) {
    BridgeMsg msg;
    // In a loop we take all available messages (IPC_NOWAIT).
    while (1) {
        ssize_t rcv = msgrcv(msq_id, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);
        if (rcv == -1) {
            if (errno == ENOMSG) {
                // no more messages
                break;
            } else {
                perror(RED "msgrcv handleBridgeQueue" RESET);
                break;
            }
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
        }
        else if (msg.mtype == MSG_WANT_TO_BOARD) {
            // passenger signals: I want to get on the ship, I have a sequence num = ??
            int seq = msg.sequence;
            pid_t pid = msg.pid;

            printf(YELLOW "=== ShipCaptain ===" RESET " Passenger %d wants to board (seq=%d). nextSequenceToBoard=%d\n", pid, seq, nextSequenceToBoard);

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
                printf(YELLOW "=== ShipCaptain ===" RESET " Passenger %d boards immediately (seq=%d)\n", pid, seq);

                nextSequenceToBoard++;
                // After boarding, check if anyone else is waiting
                checkAndBoardNextInQueue();
            }
            else if (seq < nextSequenceToBoard) {
                // passenger late, shouldn't happen
                fprintf(stderr, RED "=== ShipCaptain ===" RESET " WARNING: passenger %d has old seq=%d\n", pid, seq);
            }
            else {
                // seq > nextSequenceToBoard => put back in the array
                if (seq >= MAX_WAITING) {
                    fprintf(stderr, RED "=== ShipCaptain ===" RESET " ERROR: seq=%d too large.\n", seq);
                } else {
                    waitingArray[seq] = pid;
                    printf(YELLOW "=== ShipCaptain ===" RESET " Passenger %d queued for boarding (seq=%d)\n", pid, seq);
                }
            }
        }
        else {
            // Other types of messages - i just ignore them
            fprintf(stderr, RED "=== ShipCaptain === Unknown message type=%ld\n" RESET, msg.mtype);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, RED "Usage: %s <shmid> <semid>" RESET "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shmid = atoi(argv[1]);
    semid = atoi(argv[2]);

    printf(YELLOW "=== Ship Captain ===" RESET " Starting\n");


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

    launchShipCaptain(shmid, semid);

    return 0;
}


void handle_signal(int sig) {
    SharedMemory *sm = attachSharedMemory(shmid);
    if (sm == (void *)-1) {
        perror(RED "shmat in signal handler" RESET);
        exit(EXIT_FAILURE);
    }

    if (sig == SIGUSR1) {
        waitSemaphore(semid, SEM_MUTEX);
        if (sm->shipSailing) {
            printf(YELLOW "=== Ship Captain ===" RESET " I'm currently sailing, the signal cannot be made.\n");
            signalSemaphore(semid, SEM_MUTEX);
        } else {
            earlyVoyage = 1;
            sm->queueDirection = 1; // queue towards land, so passenger can't enter 
            sm->shipSailing = 1;
            signalSemaphore(semid, SEM_MUTEX);
        }
    } else if (sig == SIGUSR2) {
        sendStopSignal();

        int shipSailing;

        // Critical section
        waitSemaphore(semid, SEM_MUTEX);
        shipSailing = sm->shipSailing;
        if (!shipSailing) {
            sm->signalEndOfDay = 1; // Set the flag in shared memory
        }
        signalSemaphore(semid, SEM_MUTEX);

        // Logic outside the critical section
        if (shipSailing) {
            endOfDaySignal = 1; // Local flag
        }
    }
    
    shmdt(sm);
}

void sendStopSignal() {
    // Open FIFO for writing
    int fifo_fd = open(FIFO_PATH_PASSENGERS, O_WRONLY);
    if (fifo_fd == -1) {
        perror(RED "open FIFO ship" RESET);
        exit(EXIT_FAILURE);
    }

    const char *msg = "stop";
    if (write(fifo_fd, msg, strlen(msg) + 1) == -1) {
        perror(RED "write to FIFO" RESET);
        exit(EXIT_FAILURE);
    }

    printf(YELLOW "=== Ship Captain ===" RESET " Sent 'stop' signal to main.\n");
    close(fifo_fd);
}

void launchShipCaptain(int shmid, int semid) {
    SharedMemory *sm = attachSharedMemory(shmid);

    if (sm == (void *)-1) {
        perror(RED "shmat ship captain" RESET);
        exit(EXIT_FAILURE);
    }

    // Create the message queue
    msq_id = msgget(BRIDGE_QUEUE_KEY, IPC_CREAT | MSG_PERMISSIONS);
    if (msq_id == -1) {
        perror("msgget shipCaptain");
        exit(EXIT_FAILURE);
    }
    // Initialize waitingArray
    memset(waitingArray, 0, sizeof(waitingArray));


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

    while (1) {
        // non-blocking handling of passenger messages
        handleBridgeQueue();

        waitSemaphore(semid, SEM_MUTEX);
        if (sm->currentVoyage >= NUMBER_OF_TRIPS_PER_DAY) {
            sm->queueDirection = 1;
            sm->signalEndOfDay = 1;
            signalSemaphore(semid, SEM_MUTEX);
            printf(YELLOW "=== Ship Captain ===" RESET " Reached daily trip limit %d. Ending work.\n", NUMBER_OF_TRIPS_PER_DAY);
            sendStopSignal();
            shmdt(sm);
            return;
        }
        signalSemaphore(semid, SEM_MUTEX);

        // Waiting between cruises with the possibility of an early departure
        int slept = 0;
        while (slept < TIME_BETWEEN_TRIPS) {
            // Every second, handle messages as well
            handleBridgeQueue();

            waitSemaphore(semid, SEM_MUTEX); // Lock
            int endOfDay = sm->signalEndOfDay;
            int numberOfPeopleOnShip = sm->peopleOnShip;
            signalSemaphore(semid, SEM_MUTEX); // Unlock

            if (endOfDay) {
                printf(YELLOW "=== Ship Captain ===" RESET " End-of-day signal received. Preparing for disembarking.\n");

                // Setting the direction of the bridge and waiting for passengers to leave
                waitSemaphore(semid, SEM_MUTEX);
                sm->queueDirection = 1;
                signalSemaphore(semid, SEM_MUTEX);

                // Waiting until there are no passengers on the ship and bridge
                while (1) {
                    waitSemaphore(semid, SEM_MUTEX);
                    int peopleOnShip = sm->peopleOnShip;
                    int peopleOnBridge = sm->peopleOnBridge;
                    signalSemaphore(semid, SEM_MUTEX);

                    if (peopleOnShip == 0 && peopleOnBridge == 0) {
                        printf(YELLOW "=== Ship Captain ===" RESET " All passengers have disembarked.\n");
                        break;
                    }

                    usleep(100000); // 0.1s
                }

                printf(YELLOW "=== Ship Captain ===" RESET " Ending day as per signal.\n");
                shmdt(sm);
                return;
            }

            if (earlyVoyage) {
                printf(YELLOW "=== Ship Captain ===" RESET " Early departure signal received.\n");

                // Waiting until the bridge is empty
                while (1) {
                    waitSemaphore(semid, SEM_MUTEX);
                    int peopleOnBridge = sm->peopleOnBridge;
                    signalSemaphore(semid, SEM_MUTEX);

                    if (peopleOnBridge == 0) {
                        break;
                    }

                    usleep(100000); // 0.1s
                }
                waitSemaphore(semid, SEM_MUTEX);
                earlyVoyage = 0;
                signalSemaphore(semid, SEM_MUTEX);

                break; // Going to departure
            }


            // if (numberOfPeopleOnShip >= SHIP_CAPACITY) {
            //     waitSemaphore(semid, SEM_MUTEX);
            //     sm->queueDirection = 1;
            //     signalSemaphore(semid, SEM_MUTEX);
            // }

            sleep(1);
            slept++;
        }

        printf(YELLOW "=== Ship Captain ===" RESET " All the people on the bridge have to go ashore, we are sailing away!\n");
        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 1; // queue towards land, so passenger can't enter 
        sm->shipSailing = 1;
        signalSemaphore(semid, SEM_MUTEX);

        // Waiting for all passengers to pass
        while (1) {
            waitSemaphore(semid, SEM_MUTEX);
            int peopleOnBridge = sm->peopleOnBridge;
            signalSemaphore(semid, SEM_MUTEX);
            printf("ppl on bridge: %d\n", peopleOnBridge);
            if (peopleOnBridge == 0) {
                break;
            }

            usleep(100000); // 0.1s
        }

        // Cruise starting
        waitSemaphore(semid, SEM_MUTEX);
        int voyageNumber = sm->currentVoyage + 1;
        int peopleOnVoyage = sm->peopleOnShip;
        signalSemaphore(semid, SEM_MUTEX);

        printf(YELLOW "=== Ship Captain ===" RESET " All passengers have descended. We sail away.\n");
        printf(YELLOW "=== Ship Captain ===" RESET " Sailing on cruise %d with %d passengers.\n", voyageNumber, peopleOnVoyage);

        // Simulation of cruise
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

        if (endOfDaySignal) {
            printf(YELLOW "=== Ship Captain ===" RESET " End-of-day signal received during voyage. Ending day as per signal.\n");

            waitSemaphore(semid, SEM_MUTEX);
            sm->signalEndOfDay = 1;
            sm->queueDirection = 1; // queue towards land, so passenger can't enter
            signalSemaphore(semid, SEM_MUTEX);

            shmdt(sm);
            return;
        }

        waitSemaphore(semid, SEM_MUTEX);
        sm->shipSailing = 0; // end of cruise
        sm->queueDirection = 1; // towards land to disembark
        sm->currentVoyage++;
        signalSemaphore(semid, SEM_MUTEX);
        
        printf(YELLOW "=== Ship Captain ===" RESET " Cruise %d has ended. Arriving at port.\n", voyageNumber);

        // Waiting for all passengers to get off
        while (1) {
            waitSemaphore(semid, SEM_MUTEX);
            int peopleOnShip = sm->peopleOnShip;
            int peopleOnBridge = sm->peopleOnBridge;
            signalSemaphore(semid, SEM_MUTEX);

            if (peopleOnShip == 0 && peopleOnBridge == 0) {
                printf(YELLOW "=== Ship Captain ===" RESET " All passengers have disembarked after cruise %d.\n", voyageNumber);
                break;
            }

            usleep(100000); // 0.1s
        }

        // Change bridge direction again
        waitSemaphore(semid, SEM_MUTEX);
        sm->queueDirection = 0; // towards ship, getting ready for next voyage
        signalSemaphore(semid, SEM_MUTEX);

        printf(YELLOW "=== Ship Captain ===" RESET " Bridge direction set back to boarding for the next voyage.\n");
    }
}
