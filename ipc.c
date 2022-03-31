#include <unistd.h>
#include "ipc.h"
#include "general_structures.h"
#include "malloc.h"

int send(void * self, local_id dst, const Message * msg) {
    process_information_t* processInformation = (process_information_t*) self;
    pipe_t** table = processInformation->environment->pPipe;
    return write(
            table[processInformation->local_id][dst].fd[1],
            msg,
            msg->s_header.s_payload_len + MAX_MESSAGE_LEN - MAX_PAYLOAD_LEN) >= 0 ?
            0 : 1;
}


int send_multicast(void * self, const Message * msg) {

    process_information_t* processInformation = (process_information_t*) self;

    int isError = 1;
    int send_messages = 0;

    for (int local_id = 0; local_id < processInformation->environment->processNum; local_id ++) {
        if (processInformation->local_id != local_id) {
            if (send(processInformation, local_id, msg) == 0) {
                send_messages ++;
            }
        }
    }
    if (send_messages == processInformation->environment->processNum - 1)
        isError = 0;

    return isError;

}


int receive(void * self, local_id from, Message * msg) {

    process_information_t* processInformation = (process_information_t*) self;
    MessageHeader* header = malloc(MAX_MESSAGE_LEN - MAX_PAYLOAD_LEN);
    int fd = processInformation->environment->pPipe[from][processInformation->local_id].fd[0];
    if (read(fd, header, MAX_MESSAGE_LEN - MAX_PAYLOAD_LEN) > 0) {
        msg->s_header = *header;
        if (read(fd, msg->s_payload, header->s_payload_len) >= 0) return 0;
        return 1;
    }
    return 1;
}
