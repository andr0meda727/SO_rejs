#include "utils.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, RED "Usage: %s <shipCaptainPID>" RESET "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pid_t shipCaptainPID = (pid_t)atoi(argv[1]);
 
    launchHarbourCaptain(shipCaptainPID);
    
    return 0;
}


void launchHarbourCaptain(pid_t shipCaptainPID) {
    printf(MAGENTA "=== Habour Captain ===" RESET " Starting\n");
    printf(MAGENTA "=== Habour Captain ===" RESET " Enter: w = early cruise, k = end of day, q = exit\n");

    char c;
    while (1) {
        c = getchar();
        if (c == '\n') continue;

        // Buffer clearing
        while (getchar() != '\n');

        if (c == 'w') {
            // sigusr1 to ship captain
            if (kill(shipCaptainPID, SIGUSR1) == -1) {
                perror(RED "kill SIGUSR1" RESET);
            } else {
                printf(MAGENTA "=== Harbour Captain ===" RESET " Early departure signal sent.\n");
            }
        } 
        else if (c == 'k') {
            // sigusr2 to ship captain
            if (kill(shipCaptainPID, SIGUSR2) == -1) {
                perror(RED "kill SIGUSR2" RESET);
            } else {
                printf(MAGENTA "=== Harbour Captain ===" RESET " End-of-day signal sent. I'm finishing up my work for today.\n");
                break;
            }
        }
        else if (c == 'q') {
            printf(MAGENTA "=== Harbour Captain ===" RESET " I'm finishing up my work for today.\n");
            break;
        }
        else {
            printf(MAGENTA "=== Harbour Captain ===" RESET " Unknown command.\n");
        }
    }

    return;
}
