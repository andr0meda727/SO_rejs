// bridge_queue.h
#ifndef BRIDGE_QUEUE_H
#define BRIDGE_QUEUE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define BRIDGE_QUEUE_KEY 1234
#define MSG_PERMISSIONS 0600

// Message types
#define MSG_ENTER_BRIDGE 1   // Passenger: "I am entering the bridge, please give me a sequence"
#define MSG_WANT_TO_BOARD 2  // Passenger: "I want to board the ship, I have a sequence"
#define MSG_SEQUENCE_REPLY 3 // Captain: "Here is your sequence"

// Message structure
typedef struct {
    long mtype;
    pid_t pid; // Passenger's PID
    int sequence; // assigned sequence number
} BridgeMsg;

#endif
