#include "masherror.h"
#include <stdio.h>
#include <stdlib.h>

void process_pipe_exception() {
    printf("Error: failed to initialize a pipe communication.\n");
    exit(PROCESS_PIPE_ERROR);
}

void process_allocation_exception() {
    printf("Error: failed to allocate a new process.\n");
    exit(PROCESS_ALLOCATION_ERROR);
}

void process_wait_exception() {
    printf("Error: failed to wait child process to be done.\n");
    exit(PROCESS_WAIT_ERROR);
}

void process_execvp_exception(const char* command) {
    printf("\n%s[Failure]: command '%s' is not found or can not be executed properly.\n%s", KRED, command, RESET);
    exit(PROCESS_EXECVP_ERROR);
}

void process_command_usage_exception(const char* command) {
    printf("\n%s[Failure]: wrong usage of command '%s'.\n%s", KRED, command, RESET);
    exit(PROCESS_COMMAND_USAGE_ERROR);
}

void process_command_exception(const char* command) {
    printf("\n%s[Failure]: '%s' needs target file or target file can not be opened.\n%s", KRED, command, RESET);
    exit(PROCESS_COMMAND_ERROR);
}

void process_file_directory_exception() {
    printf("\n%s[Failure]: no such file or directory.\n%s", KRED, RESET);
    exit(PROCESS_FILE_DIRECTORY_ERROR);
}

void process_no_command_warning(int order) {
    printf("-----CMD %d: <blank>", order);
    for (int i = 0; i < SIZE_OF_DELIMITER_LINE - 19; i++) {
        printf("-");
    }
    printf("\n");
    printf("\n");
    printf("%s[Warning]: no input from command line.\n%s", KBLU, RESET);
    exit(PROCESS_NO_COMMAND_WARNING);
}
