#include "banking.h"
#include "utils.h"
#include "general_structures.h"

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {

    process_information_t* processInformation = parent_data;

    Message* message = create_message(get_payload_from_transfer(src, dst, amount),TRANSFER);

    send(processInformation, src, message);

    do {
        receive(processInformation, dst, message);
    } while (message->s_header.s_type != ACK);
}
