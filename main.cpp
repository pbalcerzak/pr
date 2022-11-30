#pragma once

#include <mpi.h>
#include <iostream>
#include <vector>
#include <unistd.h>


#define RED "\033[41m"
#define GRN "\033[42m"
#define YLW "\033[43m"
#define NLC "\033[0m\n"

using namespace std;

enum Tag { REQ, ACK };
struct Msg { Tag tag; int time; int energy; int rank;};
enum Type { Y, Z };

Msg received_message;
int energy = 10;
int max_rank;
int my_rank;
int my_time = 0;
Type my_type;

template <typename T>
void append(vector<T> &xs, T x) {
    xs.push_back(x);
}

int accepted = 0;

#define pretty_print(format, ...) fprintf(stderr, "%6d P%d " format, my_time, my_rank, ##__VA_ARGS__)

void tick(int new_time = 0) {
    my_time = max(my_time, new_time) + 1;
}

void send(int dst, Tag tag) {
    Msg msg {.tag = tag, .time = my_time, .energy = energy, .rank = my_rank};
    MPI_Send(&msg, sizeof(Msg), MPI_BYTE, dst, 0, MPI_COMM_WORLD);
}

bool wait_for_response(int source, Msg *received_msg)
{
    MPI_Recv(received_msg, sizeof(Msg), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (received_msg->tag == REQ) {
        pretty_print(GRN "REQ time: %d" NLC, received_msg->time);
        if (received_msg->time > my_time || (received_msg->time == my_time && received_msg->rank > my_rank)) {
            return true;
        }
    }

    return false;
}

void wait_for_accept(int source, Msg *received_msg)
{
    MPI_Recv(received_msg, sizeof(Msg), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (received_msg->tag == ACK) {
        tick();
        pretty_print(GRN "Received Accept time: %d" NLC, received_msg->time);
        energy = received_msg->energy;
    }
}

void request_access(bool add)
{
    accepted = 0;
    vector<int> deffered[max_rank-1];

    tick();
    for (int rank = 0; rank < max_rank; rank++) {
        if (rank != my_rank) {
            send(rank, REQ);
        }
    }

    for (int rank = 0; rank < max_rank; rank++) {
        if (rank != my_rank) {
            bool accept = wait_for_response(rank, &received_message);
            if (accept) {
                accepted++;
                append(deffered[1], rank);
            }
        }
    }
    for(int i: deffered[1]) {
        pretty_print(YLW "Deffered: %d" NLC, i);
    }

    for (int rank = 0; rank < max_rank; rank++) {
        if (rank != my_rank) {
            bool next = false;
            for(int i: deffered[1]) {
                if (i == rank) {
                    next = true;
                    break;
                }
            }
            if (next) {
                continue;
            }
            wait_for_accept(rank, &received_message);
            accepted++;
        }
    }


    pretty_print(RED "Accepted: %d" NLC, accepted);

    if (accepted == max_rank-1) {
        tick();
        sleep(1);
        add ? energy++ : energy--;
        pretty_print(RED "Energy: %d" NLC, energy);
    }

    for(int i: deffered[1])
        send(i, ACK);
}



int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &max_rank);

    my_type = Type(my_rank%2);

    for(int i = 0; i < 50; i++) {
        switch (my_type) {
            case Y:
                request_access(true);
                break;
            case Z:
                request_access(false);
                break;

        }
    }

    MPI_Finalize();
    return 0;
}



/* Energia 10 4 procesy
 * Wchodzi proces 1, stwierdza że może wchodzić bo jest lepszy od wszystkich requestów energia -4 6
 * Wchodzi proces 2, wie że jest lepszy od 2 procesów i gorszy od jednego a starczy energii (10) więc energia -4 6
 * Wchodzi proces 3, wie że jest lepszy od 1 procesu i gorszy od dwóch a starczy energii (10) więc energia -4 6
 * Wchodzi proces 4, wie że jest najgorszy ale starczy energii więc -4 6
 * Wchodzi proces 1, Wysyła wszystkim request, czeka -4 2
 * Wchodzi proces 2, Wysyła wszystkim request, czeka -4 2
 * Wchodzi proces 3, Wysyła wszystkim request, czeka -4 2
 * Wchodzi proces 4, Wysyła wszystkim request, czeka -4 2
 * Wchodzi proces 1, Wysyła wszystkim request, czeka -2 0
 * Wchodzi proces 2, Wysyła wszystkim request, czeka -2 0
 * Wchodzi proces 3, Wysyła wszystkim request, czeka -2 0
 *
 *
 *
 * */