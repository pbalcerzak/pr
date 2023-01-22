#pragma once

#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <ctime>


#define RED "\033[41m"
#define GRN "\033[42m"
#define YLW "\033[43m"
#define NLC "\033[0m\n"
#define CYA "\033[46m"

#define PROPORTION 2

using namespace std;
using namespace std::chrono;

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
int max_energy = 10;
int max_rank;
int my_rank;
int my_time = 0;
int yoda_count = 0;
int zet_count = 0;
int acknowledged = 0;
int energyPossiblyTaken = 0;
int energyPossiblyAdded = 0;
pthread_mutex_t yodaSentMutex;
pthread_mutex_t zetSentMutex;
pthread_mutex_t timeMutex;
pthread_mutex_t energyMutex;
pthread_mutex_t acknowledgedMutex;
pthread_mutex_t sendAckMutex;
pthread_mutex_t energyPossiblyTakenMutex;
pthread_mutex_t energyPossiblyAddedMutex;
bool yodaSent = false;
bool zetSent = false;
std::vector<int> sendAck;

#define pretty_print(format, ...) fprintf(stderr, "%6d P%d %s " format, my_time, my_rank, my_rank < yoda_count ? "Yoda" : "Zet", ##__VA_ARGS__)

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

    while (true) {
        MPI_Recv(&received_message, sizeof(Msg), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        switch (received_message.tag) {
            case REQ:
                pthread_mutex_lock(&yodaSentMutex);

                if (!yodaSent) {
                    send(received_message.rank, ACK);
                    tick(received_message.time);
                    pthread_mutex_lock(&energyMutex);
                    energy--;
                    pthread_mutex_unlock(&energyMutex);
                } else {
                    if (received_message.time > my_time ||
                        (received_message.time == my_time && received_message.rank > my_rank)) {
                        pthread_mutex_lock(&acknowledgedMutex);
                        acknowledged += 1;
                        pthread_mutex_unlock(&acknowledgedMutex);

                        pthread_mutex_lock(&sendAckMutex);
                        sendAck.push_back(received_message.rank);
                        pthread_mutex_unlock(&sendAckMutex);

                        pthread_mutex_lock(&energyPossiblyTakenMutex);
                        energyPossiblyTaken++;
                        pthread_mutex_unlock(&energyPossiblyTakenMutex);
                    } else {
                        pthread_mutex_lock(&energyMutex);
                        energy--;
                        pthread_mutex_unlock(&energyMutex);
                    }

                }

                pthread_mutex_unlock(&yodaSentMutex);
                break;
            case ACK:
                pthread_mutex_lock(&acknowledgedMutex);
                acknowledged += 1;
                pthread_mutex_unlock(&acknowledgedMutex);
                break;
            case ACCEPT:
                pthread_mutex_lock(&energyMutex);
                energy = max_energy;
                pthread_mutex_unlock(&energyMutex);
                break;
        }
    }
}

void *zetCommunicationThread(void *ptr) {
    Msg received_message{};

    while (true) {
        MPI_Recv(&received_message, sizeof(Msg), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        switch (received_message.tag) {
            case REQ:
                pthread_mutex_lock(&zetSentMutex);
                if (!zetSent) {
                    send(received_message.rank, ACK);
                    tick(received_message.time);
                    pthread_mutex_lock(&energyMutex);
                    energy++;
                    pthread_mutex_unlock(&energyMutex);
                } else {
                    if (received_message.time > my_time ||
                        (received_message.time == my_time && received_message.rank > my_rank)) {
                        pthread_mutex_lock(&acknowledgedMutex);
                        acknowledged += 1;
                        pthread_mutex_unlock(&acknowledgedMutex);

                        pthread_mutex_lock(&sendAckMutex);
                        sendAck.push_back(received_message.rank);
                        pthread_mutex_unlock(&sendAckMutex);

                        pthread_mutex_lock(&energyPossiblyAddedMutex);
                        energyPossiblyAdded++;
                        pthread_mutex_unlock(&energyPossiblyAddedMutex);
                    } else {
                        pthread_mutex_lock(&energyMutex);
                        energy++;
                        pthread_mutex_unlock(&energyMutex);
                    }

                }

                pthread_mutex_unlock(&zetSentMutex);
                break;
            case ACK:
                pthread_mutex_lock(&acknowledgedMutex);
                acknowledged += 1;
                pthread_mutex_unlock(&acknowledgedMutex);
                break;
            case ACCEPT:
                pthread_mutex_lock(&energyMutex);
                energy = 0;
                pthread_mutex_unlock(&energyMutex);
                break;
        }
    }
}

void yoda() {
    while (true) {
        if (energy > 0) {
            pthread_mutex_lock(&yodaSentMutex);
            if (!yodaSent) {
                tick();
                pretty_print(CYA "Ubiegam się o dostęp" NLC);

                pthread_mutex_lock(&acknowledgedMutex);
                acknowledged = 0;
                pthread_mutex_unlock(&acknowledgedMutex);

                for (int i = 0; i < yoda_count; i++) {
                    if (i != my_rank) {
                        send(i, REQ);
                    }
                }
                yodaSent = true;
            }
            pthread_mutex_unlock(&yodaSentMutex);

            if ((acknowledged >= yoda_count - 1) && energy > 0 && yodaSent) {
                pthread_mutex_lock(&energyMutex);
                energy--;
                pretty_print(GRN "Pobieram energię zostało: %d" NLC, energy);
                if (energy == 0) {
                    pretty_print(RED "skończyła się energia, wysyłam request do Zetów" NLC);
                    for (int i = yoda_count; i < max_rank; i++) {
                        send(i, ACCEPT);
                    }
                }
                pthread_mutex_unlock(&energyMutex);

                int sleepTime = rand() % (5 - 1) + 1;
                tick();
                for (auto &element: sendAck) {
                    send(element, ACK);
                }

                pthread_mutex_lock(&energyMutex);
                energy -= energyPossiblyTaken;
                if (energy < 0) {
                    energy = 0;
                }
                pthread_mutex_unlock(&energyMutex);

                pthread_mutex_lock(&energyPossiblyTakenMutex);
                energyPossiblyTaken = 0;
                pthread_mutex_unlock(&energyPossiblyTakenMutex);

                pthread_mutex_lock(&sendAckMutex);
                sendAck.clear();
                pthread_mutex_unlock(&sendAckMutex);

                pthread_mutex_lock(&yodaSentMutex);
                yodaSent = false;
                pthread_mutex_unlock(&yodaSentMutex);
                pretty_print(YLW "śpie przez: %d sekund" NLC, sleepTime);
                sleep(sleepTime);
            }
        } else {
//            pretty_print(RED "Czekam na uzupełnienie" NLC);
            sleep(1);
        }
    }
}

void zet() {
    while (true) {
        if (energy < max_energy) {
            pthread_mutex_lock(&zetSentMutex);
            if (!zetSent) {
                tick();
                pretty_print(CYA "Ubiegam się o dostęp" NLC);

                pthread_mutex_lock(&acknowledgedMutex);
                acknowledged = 0;
                pthread_mutex_unlock(&acknowledgedMutex);

                for (int i = yoda_count; i < max_rank; i++) {
                    if (i != my_rank) {
                        send(i, REQ);
                    }
                }
                zetSent = true;
            }
            pthread_mutex_unlock(&zetSentMutex);

            if ((acknowledged >= zet_count - 1) && energy < max_energy && zetSent) {
                pthread_mutex_lock(&energyMutex);
                energy++;
                pretty_print(GRN "Uzupełniam energię jest: %d" NLC, energy);
                if (energy == max_energy) {
                    pretty_print(RED "Energia uzupełniona, wysyłam request do Yodów" NLC);
                    for (int i = 0; i < yoda_count; i++) {
                        send(i, ACCEPT);
                    }
                }
                pthread_mutex_unlock(&energyMutex);

                int sleepTime = rand() % (5 - 1) + 1;
                tick();
                for (auto &element: sendAck) {
                    send(element, ACK);
                }

                pthread_mutex_lock(&energyMutex);
                energy += energyPossiblyAdded;
                if (energy > max_energy) {
                    energy = max_energy;
                }
                pthread_mutex_unlock(&energyMutex);

                pthread_mutex_lock(&energyPossiblyAddedMutex);
                energyPossiblyAdded = 0;
                pthread_mutex_unlock(&energyPossiblyAddedMutex);

                pthread_mutex_lock(&sendAckMutex);
                sendAck.clear();
                pthread_mutex_unlock(&sendAckMutex);

                pthread_mutex_lock(&zetSentMutex);
                zetSent = false;
                pthread_mutex_unlock(&zetSentMutex);
                pretty_print(YLW "śpie przez: %d sekund" NLC, sleepTime);
                sleep(sleepTime);
            }
        } else {
            sleep(1);
        }
    }
}

int main(int argc, char **argv) {

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &max_rank);

    yoda_count = max_rank/PROPORTION;
    zet_count = max_rank - yoda_count;

    srand(my_rank * time(nullptr));

    pthread_t threads;

    timeMutex = PTHREAD_MUTEX_INITIALIZER;
    energyMutex = PTHREAD_MUTEX_INITIALIZER;
    acknowledgedMutex = PTHREAD_MUTEX_INITIALIZER;
    sendAckMutex = PTHREAD_MUTEX_INITIALIZER;

    if (my_rank < yoda_count) {
        yodaSentMutex = PTHREAD_MUTEX_INITIALIZER;
        energyPossiblyTakenMutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_create(&threads, nullptr, yodaCommunicationThread, nullptr);
        yoda();
    } else {
        zetSentMutex = PTHREAD_MUTEX_INITIALIZER;
        energyPossiblyAddedMutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_create(&threads, nullptr, zetCommunicationThread, nullptr);
        zet();
    }

    MPI_Finalize();
    return 0;
}