#ifndef MASHERROR_H
#define MASHERROR_H

#define SIZE_OF_DELIMITER_LINE 80

#define PROCESS_PIPE_ERROR 240
#define PROCESS_ALLOCATION_ERROR 241
#define PROCESS_WAIT_ERROR 242
#define PROCESS_EXECVP_ERROR 243
#define PROCESS_COMMAND_USAGE_ERROR 244
#define PROCESS_COMMAND_ERROR 245
#define PROCESS_FILE_DIRECTORY_ERROR 246
#define PROCESS_NO_COMMAND_WARNING 124

#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define RESET "\033[0m"

void process_pipe_exception(); // 240
void process_allocation_exception(); // 241
void process_wait_exception(); // 242
void process_execvp_exception(const char* command); // 243
void process_command_usage_exception(const char* command); // 244
void process_command_exception(const char* command); // 245
void process_file_directory_exception(); // 246

void process_no_command_warning(int order); // 124

#endif