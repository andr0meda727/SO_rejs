#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define pojemnosc_statku 100
#define pojemnosc_mostka 10
#define czas_miedzy_rejsami 60
#define czas_trwania_rejsu 30
#define ilosc_rejsow_w_danym_dniu 5

void handleInput() {
#define pojemnosc_statku 100
#define pojemnosc_mostka 10
#define czas_miedzy_rejsami 60
#define czas_trwania_rejsu 30
#define ilosc_rejsow_w_danym_dniu 5

void handleInput() {
    if (pojemnosc_statku <= pojemnosc_mostka) {
        fprintf(stderr, "Mostek musi miec mniejsza pojemnosc od pojemnosci statku.\n");
        exit(1);
    }

    if (pojemnosc_mostka < 1) {
        fprintf(stderr, "Mostek musi miec pojemnosc wieksza od 0.\n");
        exit(2);
    }

    if (pojemnosc_statku < 1) {
        fprintf(stderr, "Statek musi miec pojemnosc wieksza od 0.\n");
        exit(3);
    }

    if (czas_miedzy_rejsami <= czas_trwania_rejsu) {
        fprintf(stderr, "Czas trwania rejsu musi byc mniejszy od czasu miedzy rejsami.\n");
        exit(4);
    }

    if (czas_trwania_rejsu < 1) {
        fprintf(stderr, "Rejs nie moze trwac mniej niz 1.\n");
        exit(5);
    }

    if (czas_miedzy_rejsami < 1) {
        fprintf(stderr, "Czas miedzy rejsami nie moze byc mniejszy od 1.\n");
        exit(6);
    }

    if (ilosc_rejsow_w_danym_dniu < 1) {
        fprintf(stderr, "Liczba rejsow w danym dniu musi byc wieksza od 0.\n");
        exit(7);
    }

    printf("Wszystkie parametry zostaly poprawnie zdefiniowane.\n");
}
}

int main() {
    handleInput();
}
