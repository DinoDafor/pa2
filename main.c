#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include "general_structures.h"
#include "utils.h"
#include "stdlib.h"
#include "child_process.h"

#define SKIP_FIRST_ARGUMENTS 2


void work_stage_main_process(process_information_t* processInformation) {

    close_unused_pipes(processInformation);

    processInformation->allHistory = malloc(sizeof(AllHistory));
    processInformation->allHistory->s_history_len = 0;

    bank_robbery(processInformation, processInformation->environment->processNum - 1);

    Message* message = create_message(NULL, STOP);

    send_children(processInformation, message);

    while (waitpid(-1, NULL, 0 ) != -1);

    while (processInformation->get_message != processInformation->environment->processNum - 1) {
        receive_from_children(processInformation);
    }

    processInformation->allHistory->s_history_len = processInformation->environment->processNum - 1;
    print_history(processInformation->allHistory);

}

int main(int argc, char* argv[]) {
    int process_count;
    sscanf(argv[2], "%d", &process_count);

    environment_t* environment = init_environment(process_count + 1);

    __pid_t process;
    for (int process_id = 1; process_id < environment->processNum; process_id ++) {
        if ((process = fork()) == 0) {
            process_information_t* processInformation = create_process(environment, process_id, atoi(argv[process_id + SKIP_FIRST_ARGUMENTS]));
            work_stage_child_process(processInformation);
        }
    }

    process_information_t* processInformation = create_process(environment, 0, 0);

    work_stage_main_process(processInformation);

    close(processInformation->environment->fdLog);


    return 0;
}
