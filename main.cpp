#pragma once

#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <time.h>


#define RED "\033[41m"
#define GRN "\033[42m"
#define YLW "\033[43m"
#define NLC "\033[0m\n"

using namespace std;

enum Tag {
    REQ, ACK, ACCEPT
};
struct Msg {
    Tag tag;
    int time;
    int energy;
    int rank;
};
enum Type {
    Y, Z
};

int energy = 10;
int max_rank;
int my_rank;
int my_time = 0;
int yoda_count = 0;
int zet_count = 0;
int acknowledged = 0;
Type my_type;
pthread_mutex_t yodaSentMutex;
pthread_mutex_t timeMutex;
pthread_mutex_t energyMutex;
pthread_mutex_t acknowledgedMutex;
bool yodaSent = false;

#define pretty_print(format, ...) fprintf(stderr, "%6d P%d %s " format, my_time, my_rank, my_type == 0 ? "Yoda" : "Zet", ##__VA_ARGS__)

void tick(int new_time = 0) {
    pthread_mutex_lock(&timeMutex);
    my_time = max(my_time, new_time) + 1;
    pthread_mutex_unlock(&timeMutex);
}

void send(int dst, Tag tag) {
    Msg msg{.tag = tag, .time = my_time, .energy = energy, .rank = my_rank};
    MPI_Send(&msg, sizeof(Msg), MPI_BYTE, dst, 0, MPI_COMM_WORLD);
}

void *yodaCommunicationThread(void *arg) {
    Msg received_message{};

//    pretty_print(GRN "startuje komunikacje" NLC);
    while (true) {
        MPI_Recv(&received_message, sizeof(Msg), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        switch (received_message.tag) {
            case REQ:
//                pretty_print(GRN "dostałem REQ od %d" NLC, received_message.rank);

                pthread_mutex_lock(&yodaSentMutex);
                if (!yodaSent) {
                    send(received_message.rank, ACK);
                } else {
                    if (received_message.time > my_time ||
                        (received_message.time == my_time && received_message.rank < my_rank)) {
                        pretty_print(GRN "Dosta ACCEPT" NLC);
                        pthread_mutex_lock(&acknowledgedMutex);
                        acknowledged += 1;
                        pthread_mutex_unlock(&acknowledgedMutex);
                    }

                }

                pthread_mutex_unlock(&yodaSentMutex);
                break;
            case ACK:
                tick(received_message.time);
                pthread_mutex_lock(&energyMutex);
                energy = min(energy, received_message.energy);
                pthread_mutex_unlock(&energyMutex);
//                pretty_print(RED "dostałem ACK od: P%d" NLC, received_message.rank);
                break;
            case ACCEPT:
                pretty_print(GRN "dostałem ACCEPT" NLC);
                break;
        }
    }
}

void *startComThreadZet(void *ptr);

void yoda() {
    while (true) {
        if (energy > 0) {
            pthread_mutex_lock(&yodaSentMutex);
            if (!yodaSent) {
                tick();
                pretty_print(YLW "Ubiegam się o dostęp" NLC);
                for (int i = 0; i < yoda_count; i++) {
                    if (i != my_rank) {
                        send(i, REQ);
                    }
                }
                yodaSent = true;

                pthread_mutex_lock(&acknowledgedMutex);
                acknowledged = 0;
                pthread_mutex_unlock(&acknowledgedMutex);
            }
            pthread_mutex_unlock(&yodaSentMutex);

//            int energyPossiblyTaken = 0;
//        pretty_print(RED "Moje acknowledged: %d" NLC, acknowledged);
            if ((acknowledged == yoda_count - 1) && energy > 0 && yodaSent) {
//            pretty_print(RED "Pobieram energie" NLC);
                pthread_mutex_lock(&energyMutex);
                energy--;
//                energy -= energyPossiblyTaken;
                pretty_print(GRN "Pobieram energię zostało: %d" NLC, energy);
                if (energy == 0) {
                    pretty_print(RED "skończyła się energia!!!" NLC);
                }
                pthread_mutex_unlock(&energyMutex);
                int sleepTime = rand() % (6 - 1) + 1;
                tick();
                for (int i = 0; i < yoda_count; i++) {
                    if (i != my_rank) {
//                    pretty_print(RED "wysyłam ACK do %d" NLC, i);
                        send(i, ACK);
                    }
                }

                pthread_mutex_lock(&yodaSentMutex);
                yodaSent = false;
                pthread_mutex_unlock(&yodaSentMutex);
                pretty_print(YLW "śpie przez: %d sekund" NLC, sleepTime);
                sleep(sleepTime);
                pretty_print(YLW "skończyłem spać" NLC);
            }
        } else {
            pretty_print(RED "Czekam na uzupełnienie" NLC);
            sleep(1);
        }
    }
}

void zet() {
    pretty_print(GRN "REQ time: %d" NLC, 12);
}

int main(int argc, char **argv) {

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &max_rank);

//    my_type = Type(my_rank%2);
//    yoda_count = (int) ceil(max_rank/2.0);
//    zet_count = max_rank - yoda_count;
    my_type = Type(0);
    yoda_count = max_rank;
    srand(my_rank * time(nullptr));

    pthread_t threads;

    timeMutex = PTHREAD_MUTEX_INITIALIZER;
    energyMutex = PTHREAD_MUTEX_INITIALIZER;
    acknowledgedMutex = PTHREAD_MUTEX_INITIALIZER;

    switch (my_type) {
        case Y:
            yodaSentMutex = PTHREAD_MUTEX_INITIALIZER;
            pthread_create(&threads, nullptr, yodaCommunicationThread, nullptr);
            yoda();
            break;
//        case Z:
//            zet();
//            pthread_create(&threads, nullptr, startComThreadZet, nullptr);
//            break;
    }

    MPI_Finalize();
    return 0;
}