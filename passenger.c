#include "utils.h"
#include "bridge_queue.h"
#include "passenger.h"

int shmid, semid, msq_id;
int onShip, mySequence, lastTripTried, waitingForNextArrival;
SharedMemory *sm;
pid_t myPID;

int main(int argc, char *argv[]) {
    initialize(argc, argv);

    while (1) {
        checkSignals();

        if (!onShip) {
            if (waitingForNextArrival) {
                waitForShipToReturn();
            } else {
                attemptBoardBridge();
            }
        } else {
            disembarkShip();
        }
    }
}


void initialize(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, RED "Usage: %s <shmid> <semid>" RESET "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    shmid = atoi(argv[1]);
    semid = atoi(argv[2]);

    sm = attachSharedMemory(shmid);
    if (sm == (void *)-1) {
        perror(RED "shmat passenger" RESET);
        exit(EXIT_FAILURE);
    }

    msq_id = msgget(BRIDGE_QUEUE_KEY, 0);
    if (msq_id == -1) {
        perror("msgget passenger");
        exit(EXIT_FAILURE);
    }

    myPID = getpid();

    onShip = 0; // flag: am I already on the ship?
    mySequence = -1; // unique sequence number
    lastTripTried = -1; // Cruise we recently tried to enter
    waitingForNextArrival = 0; // After unsuccessful attempt to board ship, passenger waits in port for next voyage
}


void checkSignals() {
    waitSemaphore(semid, SEM_MUTEX);
    int endOfDay = sm->signalEndOfDay;
    signalSemaphore(semid, SEM_MUTEX);

    if (endOfDay) {
        if (onShip) {
            disembarkAfterEndOfDaySignal();
        } else {
            printf(CYAN "=== Passenger %d ===" RESET " Exiting port.\n", myPID);
        }

        shmdt(sm);
        exit(0);
    }
}


void attemptBoardBridge() {
    waitSemaphore(semid, SEM_BRIDGE);
    waitSemaphore(semid, SEM_MUTEX);

    int queueDir = sm->queueDirection;
    int shipSail = sm->shipSailing;
    int currentTrip = sm->currentVoyage;

    if (queueDir == 0 && shipSail == 0) {
        // I can board the bridge
        printf(CYAN "=== Passenger %d ===" RESET " I entered the bridge. PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n", myPID, sm->peopleOnShip, ++(sm->peopleOnBridge));
        signalSemaphore(semid, SEM_MUTEX);

        // Send to captain MSG_ENTER_BRIDGE
        BridgeMsg msg;
        msg.mtype = MSG_ENTER_BRIDGE;
        msg.pid = myPID;
        msg.sequence = -1;
        if (msgsnd(msq_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            perror("msgsnd ENTER_BRIDGE");
        }

        // Wait for the sequence reply
        BridgeMsg reply;
        if (msgrcv(msq_id, &reply, sizeof(reply) - sizeof(long), MSG_SEQUENCE_REPLY, 0) == -1) {
            perror(RED "msgrcv MSG_SEQUENCE_REPLY" RESET);
        } else {
            mySequence = reply.sequence;
        }


        // random walking time simulation
        // usleep(((rand() % 1000) + 10) * 1000); 


        // Didn't the captain say he was about to sail away and ask us to leave the bridge during our simulated walk?
        waitSemaphore(semid, SEM_MUTEX);
        if (sm->queueDirection == 1 || sm->shipSailing == 1) {
            // He did, we have to leave
            int peopleOnBridge = --sm->peopleOnBridge;
            signalSemaphore(semid, SEM_MUTEX);

            printf(CYAN "=== Passenger %d ===" RESET " I leave the bridge because the captain is about to sail away. People on bridge left: %d\n", myPID, peopleOnBridge);

            signalSemaphore(semid, SEM_BRIDGE);
            return;
        }
        signalSemaphore(semid, SEM_MUTEX);

        attemptBoardShip(currentTrip);
    } else {
        signalSemaphore(semid, SEM_MUTEX);
        signalSemaphore(semid, SEM_BRIDGE);
    }
}


void attemptBoardShip(int tripWhenTried) {
    BridgeMsg boardReq;
    boardReq.mtype = MSG_WANT_TO_BOARD;
    boardReq.pid = myPID;
    boardReq.sequence = mySequence;

    if (msgsnd(msq_id, &boardReq, sizeof(boardReq) - sizeof(long), 0) == -1) {
        perror("msgsnd WANT_TO_BOARD");
    }

    // We receive a response (OK or refusal), wait for a message of type = myPID
    BridgeMsg boardResp;
    if (msgrcv(msq_id, &boardResp, sizeof(boardResp) - sizeof(long), myPID, 0) == -1) {
        perror("msgrcv myPID -> boarding response");
    }

    // sequence >= 0 => OK
    if (boardResp.sequence >= 0) {
        // Boarding
        onShip = 1;

        signalSemaphore(semid, SEM_BRIDGE);
    } else {
        // sequence == -1 => denial, ship full
        waitSemaphore(semid, SEM_MUTEX);
        sm->peopleOnBridge--;
        printf(CYAN "=== Passenger %d ===" RESET " Denied boarding (ship full). Exiting bridge.\n", myPID);
        signalSemaphore(semid, SEM_MUTEX);
        signalSemaphore(semid, SEM_BRIDGE);

        // We already tried and we got denied, so we wait for next voyage
        lastTripTried = tripWhenTried;
        waitingForNextArrival = 1;
    }
}



void disembarkShip() {
    waitSemaphore(semid, SEM_MUTEX);
    int nowSail = sm->shipSailing;
    int nowQueueDir = sm->queueDirection;
    signalSemaphore(semid, SEM_MUTEX);

    if (nowSail == 0 && nowQueueDir == 1) {
        // Can disembark
        waitSemaphore(semid, SEM_BRIDGE);
        waitSemaphore(semid, SEM_MUTEX);
        int peopleOnShip = --(sm->peopleOnShip);
        int peopleOnBridge = ++(sm->peopleOnBridge);
        signalSemaphore(semid, SEM_MUTEX);

        printf(CYAN "=== Passenger %d ===" RESET " Disembarking from ship. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE: %d\n", myPID, peopleOnShip, peopleOnBridge);

        // Simulation of crossing the bridge in disembarking
        // sleep(1);
        // usleep(100)

        // Successfully disembarked
        waitSemaphore(semid, SEM_MUTEX);
        printf(CYAN "=== Passenger %d ===" RESET " Left bridge. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", myPID, sm->peopleOnShip, --(sm->peopleOnBridge));
        signalSemaphore(semid, SEM_MUTEX);

        signalSemaphore(semid, SEM_BRIDGE);

        shmdt(sm);
        exit(0);
    }
}


void disembarkAfterEndOfDaySignal() {
    waitSemaphore(semid, SEM_BRIDGE);
    waitSemaphore(semid, SEM_MUTEX);
    printf(CYAN "=== Passenger %d ===" RESET " End of day signal received. I'm getting off the ship. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), --(sm->peopleOnShip), ++(sm->peopleOnBridge));
    signalSemaphore(semid, SEM_MUTEX);

    // simulation of crossing the bridge in disembarking on signal
    // sleep(1);
    // usleep(100);

    waitSemaphore(semid, SEM_MUTEX);
    printf(CYAN "=== Passenger %d ===" RESET " I left the bridge. Exiting port. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), sm->peopleOnShip, --(sm->peopleOnBridge));
    signalSemaphore(semid, SEM_MUTEX);
    signalSemaphore(semid, SEM_BRIDGE); // Free the space on the bridge

    shmdt(sm);
    exit(0);
}

void waitForShipToReturn() {
    while (1) {
        checkSignals(); 
        waitSemaphore(semid, SEM_MUTEX);
        int currentTrip = sm->currentVoyage;
        signalSemaphore(semid, SEM_MUTEX);

        if (currentTrip > lastTripTried) {
            waitingForNextArrival = 0;
            break;
        }
    }
}


