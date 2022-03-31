#ifndef PA1_GENERAL_STRUCTURES_H
#define PA1_GENERAL_STRUCTURES_H

#include "banking.h"

typedef enum {true, false} boolean;

struct pipes {
    int fd[2];
} typedef pipe_t;

struct environment {
    pipe_t** pPipe;
    int fdLog;
    int processNum;
} typedef environment_t;


struct process_information {
    environment_t* environment;
    int local_id;
    int* liveProcesses;
    int get_signal_stop;
    BalanceHistory* balanceHistory;
    AllHistory* allHistory;
    int get_message;
} typedef process_information_t;


#endif //PA1_GENERAL_STRUCTURES_H
