#include "utils.h"
#include "bridge_queue.h"
#include "passenger.h"


int shmid, semid, msq_id;
SharedMemory *sm;
pid_t myPID;
int onShip = 0; // flag: am I already on the ship?
int mySequence = -1; // unique sequence number


int main(int argc, char *argv[]) {
    initialize(argc, argv);

    while (1) {
        checkSignals();

        if (!onShip) {
            attemptBoardBridge();
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
    int shipFull = (sm->peopleOnShip + sm->peopleOnBridge + 1 > SHIP_CAPACITY);

    if (queueDir == 0 && shipSail == 0 && !shipFull) {
        // I can board the bridge
        sm->peopleOnBridge++;
        printf(CYAN "=== Passenger %d ===" RESET " I entered the bridge. PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n", myPID, sm->peopleOnShip, sm->peopleOnBridge);
        signalSemaphore(semid, SEM_MUTEX);

        // Send to captain MSG_ENTER_BRIDGE
        BridgeMsg msg;
        msg.mtype = MSG_ENTER_BRIDGE;
        msg.pid = myPID;
        msg.sequence = -1;  // so far we do not know
        if (msgsnd(msq_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            perror("msgsnd ENTER_BRIDGE");
        }

        // Wait for the sequence reply
        BridgeMsg reply;
        if (msgrcv(msq_id, &reply, sizeof(reply) - sizeof(long), MSG_SEQUENCE_REPLY, 0) == -1) {
            perror("msgrcv MSG_SEQUENCE_REPLY");
        } else {
            mySequence = reply.sequence;
        }

        // usleep(((rand() % 30) + 10) * 1); // random walking simulation
        attemptBoardShip();
    } else {
        signalSemaphore(semid, SEM_MUTEX);
        signalSemaphore(semid, SEM_BRIDGE);
    }
}


void attemptBoardShip() {
    waitSemaphore(semid, SEM_MUTEX);

    if (sm->peopleOnShip < SHIP_CAPACITY && sm->queueDirection == 0 && sm->shipSailing == 0) {
        // 1) We send to the captain: I want to get in (I have a sequence)
        BridgeMsg boardReq;
        boardReq.mtype = MSG_WANT_TO_BOARD;
        boardReq.pid = myPID;
        boardReq.sequence = mySequence;
        signalSemaphore(semid, SEM_MUTEX);

        if (msgsnd(msq_id, &boardReq, sizeof(boardReq) - sizeof(long), 0) == -1) {
            perror("msgsnd WANT_TO_BOARD");
        }

        // Wait for boarding confirmation
        BridgeMsg boardOk;
        if (msgrcv(msq_id, &boardOk, sizeof(boardOk) - sizeof(long), myPID, 0) == -1) {
            perror("msgrcv myPID -> boarding OK");
        }

        waitSemaphore(semid, SEM_MUTEX);
        int peopleOnShip = ++(sm->peopleOnShip);
        int peopleOnBridge = --(sm->peopleOnBridge);
        int currentVoyage = sm->currentVoyage + 1;
        signalSemaphore(semid, SEM_MUTEX);

        printf(CYAN "=== Passenger %d ===" RESET " I boarded the ship (voyage no. %d). PEOPLE ON SHIP: %d, PEOPLE ON BRIDGE: %d\n",
               myPID, currentVoyage, peopleOnShip, peopleOnBridge);
        fflush(stdout);

        onShip = 1;

        // Release the bridge semaphore
        signalSemaphore(semid, SEM_BRIDGE);
    } else {
        // In the meantime, the ship has become full or the queueDirection has changed
        sm->peopleOnBridge--;
        signalSemaphore(semid, SEM_MUTEX);
        signalSemaphore(semid, SEM_BRIDGE);
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

        sm->peopleOnShip--;
        sm->peopleOnBridge++;
        printf(CYAN "=== Passenger %d ===" RESET " Disembarking from ship. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE: %d\n", myPID, sm->peopleOnShip, sm->peopleOnBridge);
        signalSemaphore(semid, SEM_MUTEX);

        // simulation of crossing the bridge
        // usleep(10);

        // Successfully disembarked
        waitSemaphore(semid, SEM_MUTEX);
        sm->peopleOnBridge--;
        printf(CYAN "=== Passenger %d ===" RESET " Left bridge. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", myPID, sm->peopleOnShip, sm->peopleOnBridge);
        signalSemaphore(semid, SEM_MUTEX);

        signalSemaphore(semid, SEM_BRIDGE);

        shmdt(sm);
        exit(0);
    }
}


void disembarkAfterEndOfDaySignal() {
    waitSemaphore(semid, SEM_BRIDGE);
    waitSemaphore(semid, SEM_MUTEX);
    sm->peopleOnShip--;
    sm->peopleOnBridge++;
    printf(CYAN "=== Passenger %d ===" RESET " End of day signal received. I'm getting off the ship. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
    signalSemaphore(semid, SEM_MUTEX);

    // usleep(100); // simulation of disembarking

    waitSemaphore(semid, SEM_MUTEX);
    sm->peopleOnBridge--;
    printf(CYAN "=== Passenger %d ===" RESET " I left the bridge. Exiting port. PEOPLE ON SHIP LEFT: %d, PEOPLE ON BRIDGE LEFT: %d\n", getpid(), sm->peopleOnShip, sm->peopleOnBridge);
    signalSemaphore(semid, SEM_MUTEX);
    signalSemaphore(semid, SEM_BRIDGE); // Free the space on the bridge

    shmdt(sm);
    exit(1);
}
