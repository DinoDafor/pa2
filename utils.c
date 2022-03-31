#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "utils.h"
#include "common.h"

void set_default_state_live_processes(const int local_id, int* pProcesses, int process_count) {

    pProcesses[0] = 1;

    for (int process_id = 1; process_id < process_count; process_id ++) {
        if (local_id != process_id) pProcesses[process_id] = 0;
        else pProcesses[process_id] = 1;
    }

}


environment_t* init_environment(int process_num) {

    environment_t* env = malloc(sizeof(environment_t));

    env->processNum = process_num;

    pipe_t* table = calloc(process_num * process_num, sizeof(pipe_t));
    pipe_t** row = malloc(process_num * sizeof(pipe_t));

    for (int row_index = 0 ; row_index < process_num; row_index ++) {
        row[row_index] = table + process_num * row_index;
        for (int column_index = 0; column_index < process_num; column_index ++) {
            if (row_index != column_index) {
                pipe(row[row_index][column_index].fd);
                fcntl(row[row_index][column_index].fd[0], F_SETFL, O_NONBLOCK);
                fcntl(row[row_index][column_index].fd[1], F_SETFL, O_NONBLOCK);
            }
        }
    }

    env->pPipe = row;

    env->fdLog = open(events_log, O_APPEND | O_CREAT | O_RDWR);

    return env;
}


process_information_t* create_process(environment_t* environment, int local_id, int balance) {
    process_information_t* processInformation = malloc(sizeof(process_information_t));

    BalanceState  balanceState = {.s_balance = balance, .s_time = get_physical_time(), .s_balance_pending_in = 0};

    processInformation->environment = environment;
    processInformation->local_id = local_id;
    processInformation->get_signal_stop = 0;
    processInformation->balanceHistory = malloc(sizeof(BalanceHistory));
    processInformation->balanceHistory->s_history_len = 1;
    processInformation->balanceHistory->s_id = local_id;
    processInformation->balanceHistory->s_history[0] = balanceState;
    processInformation->allHistory = NULL;
    processInformation->get_message = 0;


    int* neighbor = calloc(environment->processNum, sizeof(int));
    set_default_state_live_processes(processInformation->local_id, neighbor, processInformation->environment->processNum);
    processInformation->liveProcesses = neighbor;

    return processInformation;
}

void close_unused_pipes(process_information_t* processInformation) {

    environment_t* environment = processInformation->environment;

    for (int row = 0; row < environment->processNum; row ++) {
        for (int column = 0; column < environment->processNum; column ++) {
            if (row != column && row != processInformation->local_id && column != processInformation->local_id) {
                close(environment->pPipe[row][column].fd[1]);
                close(environment->pPipe[row][column].fd[0]);
            } else if (row == processInformation->local_id) {
                close(environment->pPipe[row][column].fd[0]);
            } else if (column == processInformation->local_id) {
                close(environment->pPipe[row][column].fd[1]);
            }
        }
    }

}

MessageHeader create_header(const char* payload, MessageType type) {
    if (type == TRANSFER) {
        MessageHeader header = {
                .s_type = TRANSFER,
                .s_payload_len = sizeof(TransferOrder),
                .s_magic = MESSAGE_MAGIC,
                .s_local_time = get_physical_time()
        };
        return header;
    } else if (type == BALANCE_HISTORY) {
        MessageHeader  header = {
                .s_type = BALANCE_HISTORY,
                .s_payload_len = sizeof(BalanceHistory),
                .s_magic = MESSAGE_MAGIC,
                .s_local_time = get_physical_time()
        };
        return header;
    } else {
        MessageHeader header = {
                .s_local_time = get_physical_time(),
                .s_payload_len = payload != NULL ? strlen(payload) : 0,
                .s_magic = MESSAGE_MAGIC,
                .s_type = type
        };

        return header;
    }
}

Message* create_message(const char* payload, MessageType type) {

    MessageHeader  header = create_header(payload, type);

    Message* message = (Message*) malloc(MAX_MESSAGE_LEN);
    message->s_header = header;

    if (payload != NULL && BALANCE_HISTORY != type) {
        strncpy(message->s_payload, payload, strlen(payload));
    }

    return message;
}

char* print_output(const char* message_format, const process_information_t* processInformation, const TransferOrder* transferOrder, MessageType type) {
    char* payload = malloc(strlen(message_format));
    switch (type) {
        case STARTED:
            sprintf(payload, message_format, get_physical_time(), processInformation->local_id, getpid(), getppid(), processInformation->balanceHistory->s_history[processInformation->balanceHistory->s_history_len - 1].s_balance);
            break;
        case DONE:
            sprintf(payload, message_format, get_physical_time(), processInformation->local_id, processInformation->balanceHistory->s_history[processInformation->balanceHistory->s_history_len - 1].s_balance);
            break;
        case TRANSFER:
            if (transferOrder->s_src == processInformation->local_id)
                sprintf(payload,  message_format, get_physical_time(), processInformation->local_id, transferOrder->s_amount, transferOrder->s_dst);
            else
                sprintf(payload, message_format, get_physical_time(), processInformation->local_id, transferOrder->s_amount, transferOrder->s_src);
            break;
        default:
            sprintf(payload, message_format, get_physical_time(), processInformation->local_id);
    }

    write(processInformation->environment->fdLog, payload, strlen(payload));
    printf("%s", payload);
    return payload;

}

int get_live_process_count(int* processes, int process_count) {

    int working_processes = 0;

    for (int i = 1; i < process_count; i ++) {
        if (processes[i] == 1) working_processes ++;
    }

    return working_processes;
}

void update_balance_history(const TransferOrder* transferOrder, BalanceHistory* balanceHistory, int increase_mode) {

    int system_time = get_physical_time();

    while (balanceHistory->s_history_len < system_time) {
        BalanceState balanceState = {
                .s_balance_pending_in = 0,
                .s_time = balanceHistory->s_history_len,
                .s_balance = balanceHistory->s_history[balanceHistory->s_history_len - 1].s_balance
        };
        balanceHistory->s_history[balanceHistory->s_history_len] = balanceState;
        balanceHistory->s_history_len ++;
    }

    BalanceState balanceState = {
            .s_balance_pending_in = 0,
            .s_time = get_physical_time(),
            .s_balance = increase_mode == 1 ?
                    balanceHistory->s_history[balanceHistory->s_history_len - 1].s_balance + transferOrder->s_amount:
                    balanceHistory->s_history[balanceHistory->s_history_len - 1].s_balance - transferOrder->s_amount
    };

    balanceHistory->s_history[balanceHistory->s_history_len] = balanceState;
    balanceHistory->s_history_len ++;

}


void get_transfer_from_parent(const Message* message, process_information_t* processInformation) {

    TransferOrder* transferOrder = (TransferOrder*) message->s_payload;

    update_balance_history(transferOrder, processInformation->balanceHistory, 0);

    print_output(log_transfer_out_fmt, processInformation, transferOrder, TRANSFER);

    Message* msg = create_message(get_payload_from_transfer(processInformation->local_id, transferOrder->s_dst, transferOrder->s_amount), TRANSFER);

    send(processInformation, transferOrder->s_dst, msg);

}


void get_transfer_from_child(const Message* message, process_information_t* processInformation) {

    TransferOrder* transferOrder = (TransferOrder*) message->s_payload;

    update_balance_history(transferOrder, processInformation->balanceHistory, 1);

    print_output(log_transfer_in_fmt, processInformation, transferOrder, TRANSFER);

    Message* msg = create_message(NULL, ACK);

    send(processInformation, 0, msg);
}


void handle_messages(process_information_t* processInformation) {

    Message  message;
    int i = 0;
    while (i < processInformation->environment->processNum) {
        if (i != processInformation->local_id) {
            if (receive(processInformation, i, &message) == 0) {
                switch (message.s_header.s_type) {
                    case STOP:
                        processInformation->get_signal_stop = 1;
                        break;
                    case TRANSFER:
                        if (i == 0) {
                            get_transfer_from_parent(&message, processInformation);
                        } else {
                            get_transfer_from_child(&message, processInformation);
                        }
                        break;
                }
            }
        }
        i ++;
    }
}


char* get_payload_from_transfer(const int src, const int dst, const int balance) {

    TransferOrder* transferOrder = malloc(sizeof(TransferOrder));
    transferOrder->s_dst = dst;
    transferOrder->s_src = src;
    transferOrder->s_amount = balance;

    return (char*) transferOrder;
}

void update_all_history(const int from_local_id, const BalanceHistory* balanceHistory, AllHistory* allHistory) {
    int max_length = balanceHistory->s_history_len > allHistory->s_history_len ? balanceHistory->s_history_len : allHistory->s_history_len;
    allHistory->s_history_len = max_length;
    allHistory->s_history[from_local_id - 1] = *balanceHistory;


    for (int local_id = 0; local_id < from_local_id; local_id ++) {
        BalanceHistory old_balance_history = allHistory->s_history[local_id];


        while (old_balance_history.s_history_len < max_length) {
            BalanceState balanceState = {
                    .s_balance_pending_in = 0,
                    .s_time = old_balance_history.s_history_len,
                    .s_balance = old_balance_history.s_history[old_balance_history.s_history_len - 1].s_balance
            };
            old_balance_history.s_history[old_balance_history.s_history_len] = balanceState;
            old_balance_history.s_history_len ++;
        }
        allHistory->s_history[local_id] = old_balance_history;
    }
}

void receive_from_children(process_information_t* processInformation) {

    Message message;

    for (int local_id = 1; local_id < processInformation->environment->processNum; local_id ++) {
        if (receive(processInformation, local_id, &message) == 0) {
            if (message.s_header.s_type == STARTED) {
                processInformation->liveProcesses[local_id] = 1;
            } else if (message.s_header.s_type == DONE) {
                processInformation->liveProcesses[local_id] = 0;
            } else if (message.s_header.s_type == BALANCE_HISTORY) {
                update_all_history(local_id, (BalanceHistory*) message.s_payload, processInformation->allHistory);
                processInformation->get_message ++;
            }
        }
    }
}

void send_children(process_information_t* processInformation, Message* message) {
    for (int local_id = 1; local_id < processInformation->environment->processNum; local_id ++) {
        send(processInformation, local_id, message);
    }
}
