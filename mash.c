/**
 * @file mash.c
 * @author Minzhi Qu (quminzhi@gmail.com)
 * @brief MASH Shell will 'mash' three Linux command requests together and run them against the
 * same input file in paraller. 
 * @version 0.1
 * @date 2021-11-10
 * 
 * @copyright Copyright (c) 2021
 */

// EXTRA CREDIT FEATURES: EC1, EC2, EC3, EC4 implemented

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include "mash.h"
#include "masherror.h"

/**
 * @brief MsgCollector
 * 
 * @param message: pipe message
 * 
 * The task UI process will do:
 * 1. prompt input from user.
 * 2. put number of commands into pipe.
 * 3. put commands into pipe in form of {len(cmd), cmd}. 
 */
STATUS MsgCollector(IN int* message) {
    // TODO: read 3 commands and a file from user
    char userInput[USER_INPUT_MAX_SIZE];
    int len;
    int i = 1;
    close(message[0]); // close read
    // TODO: write commands to message
    // message: {int, len(cmd1), cmd1, len(cmd2), cmd2, len(cmd3), cmd3, len(file), file}
    int numberOfCommands = 4;
    write(message[1], &numberOfCommands, sizeof(numberOfCommands));
    while (i < 4) {
        printf("mash-%d> ", i);
        // fgets will append a new line character '\n' to string
        fgets(userInput, USER_INPUT_MAX_SIZE, stdin);
        // fix fgets problem: replace '\n' with '\0' 
        userInput[strcspn(userInput, "\n")] = 0;
        len = strlen(userInput);
        write(message[1], &len, sizeof(len));
        write(message[1], userInput, strlen(userInput) + 1);
        i++;
    }
    printf("file> ");
    fgets(userInput, USER_INPUT_MAX_SIZE, stdin);
    userInput[strcspn(userInput, "\n")] = 0; 
    len = strlen(userInput);
    write(message[1], &len, sizeof(len));
    write(message[1], userInput, strlen(userInput) + 1);
    close(message[1]);

    return 0;
}

/**
 * @brief MessageParser
 * 
 * @param message: pipe message
 * @param numberOfEntries: the number of entries user input
 * @param commands_out: pass command strings to main process
 * 
 * The function will parse message in pipe and return command strings to main process.
 */
STATUS MessageParser(IN int* message, IN int* numberOfEntries, OUT char*** commands_out) {
    close(message[1]);
    const char buffer[MESSAGE_MAX_SIZE];
    read(message[0], (void*)buffer, sizeof(buffer));
    // message: {int, len(cmd1), cmd1, len(cmd2), cmd2, len(cmd3), cmd3, len(file), file}
    char* p = (char*)buffer; // pointer used to scan buffer
    
    int numberOfCommands;
    int len;
    memcpy(&numberOfCommands, buffer, sizeof(numberOfCommands));
    char** commands = malloc(sizeof(char*) * numberOfCommands);
    p += sizeof(numberOfCommands);
    for (int i = 0; i < numberOfCommands; i++) {
        memcpy(&len, p, sizeof(len));
        p += sizeof(len);
        commands[i] = malloc(len + 1); // '\0'
        strcpy(commands[i], p);
        p += len + 1;
    }
    close(message[0]);

    *numberOfEntries = numberOfCommands;
    *commands_out = commands;

    return 0;
}

/**
 * @brief WaitStatusChecker
 * 
 * @param wstatus: wstatus
 * 
 * The function will report exit code of command in exec() in child process.
 */
STATUS WaitStatusChecker(IN int wstatus) {
        if (WIFEXITED(wstatus)) {
        int statusCode = WEXITSTATUS(wstatus);
        if (statusCode != 0) {
            printf("Process %d: failure with exit code %d\n", getppid(), statusCode);
        }
    }

    return 0;
}

/**
 * @brief CommandParserWithoutFile
 * 
 * @param commands: a string of command including parameters 
 * @param args: a string array of command and command args
 * @param size_o: size of args
 * 
 * The function will extract command and parameters from string and pass it back with args.
 */
STATUS CommandParserWithoutFile(IN const char* commands, OUT char*** args, OUT int* size_o) {
    char* newCommands = strdup(commands);
    int maxSize = COMMAND_MAX_SIZE;
    char** argList = malloc(sizeof(char*) * maxSize);
    memset(argList, 0, sizeof(char*) * maxSize);
    int size = 0;
    
    char* token = strtok(newCommands, " ");
    while (token != nullptr) {
        argList[size++] = token;
        token = strtok(nullptr, " ");
    }
    
    if (DEBUG) {
        printf("Child Process %d: calling CommandParserWithoutFile:\n", getpid());
        printf("args: ");
        for (int i = 0; i < size; i++) {
            printf("%s ", argList[i]);
        }
        printf("\n\n");
    }

    *args = argList;
    *size_o = size;

    return 0;
}

/**
 * @brief CommandParserWithFile
 * 
 * @param commands: a string of command including parameters
 * @param file: file string
 * @param args: a string array of command and command args
 * @param size_o: size of args
 * 
 * The function will extract command and parameters from string and pass it back with args.
 */
STATUS CommandParserWithFile(IN const char* commands, IN const char* file, OUT char*** args, OUT int* size_o) {
    char* newCommands = strdup(commands);
    char* newFile = strdup(file);
    int maxSize = COMMAND_MAX_SIZE;
    char** argList = malloc(sizeof(char*) * maxSize);
    memset(argList, 0, sizeof(char*) * maxSize);
    int size = 0;
    
    char* token = strtok(newCommands, " ");
    while (token != nullptr) {
        argList[size++] = token;
        token = strtok(nullptr, " ");
    }
    argList[size++] = newFile;

    if (DEBUG) {
        printf("Child Process %d: calling CommandParserWithFile:\n", getpid());
        printf("args: ");
        for (int i = 0; i < size; i++) {
            printf("%s ", argList[i]);
        }
        printf("\n\n");
    }

    *args = argList;
    *size_o = size;

    return 0;
}

/**
 * @brief CommandParser
 * 
 * A router to two kinds of command parser, with or without file.
 */
STATUS CommandParser(IN const char* commands, IN const char* file, IN char*** args, OUT int* size_o) {
    if (strlen(file) == 0) {
        CommandParserWithoutFile(commands, args, size_o);
    }
    else {
        CommandParserWithFile(commands, file, args, size_o);
    }

    return 0;
}

/**
 * @brief isCommandWithTarget
 * 
 * @param command: command to test
 * @return int: true if it is
 */
int isCommandWithTarget(const char* command) {
    for (int i = 0; i < SIZE_OF_TARGET_COMMAND; i++) {
        if (strcmp(command, target_commands[i]) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief printChildrenProcess
 * 
 * @param processID 
 * @param statusCode
 * 
 * The function will print child process information according to status.
 */
void printChildrenProcess(int processID, int statusCode) {
    if (statusCode == 0) {
        printf("%s%d(%d) %s", KGRN, processID, statusCode, RESET);
    }
    else if (statusCode == PROCESS_NO_COMMAND_WARNING) {
        printf("%s%d(%d) %s", KBLU, processID, statusCode, RESET);
    }
    else {
        printf("%s%d(%d) %s", KRED, processID, statusCode, RESET);
    }
} 

/**
 * @brief Worker
 * 
 * @param command: raw command string received from main process 
 * @param file: target file, nullptr if empty
 * @param time_pipe: used for tranfer time information between main process and child process
 * 
 * The function is responsible for executing given command and cache result.
 * 1. parse given command.
 * 2. redirect output.
 * 3. create a new process and execute command with execvp.
 */
STATUS Worker(IN const char* command, IN const char* file, IN int order) {
    // @args: argument list. A string of command, its arguments, and potential target file. 
    char** args;
    // @size: size of 'args', do NOT contain NULL at the end for execvp(). 
    int size; 

    // TODO: redirect stdout and stderr to cache
    if (!DEBUG) {
        char cacheName[CACHE_NAME_SIZE];
        sprintf(cacheName, "job_%d_cache", getpid());
        int descriptor = open(cacheName, O_WRONLY | O_CREAT, 0644);
        if (descriptor == -1) {
            printf("Error: failed to create a file.\n");
            exit(279);
        }
        dup2(descriptor, STDOUT_FILENO);
        dup2(descriptor, STDERR_FILENO);
        close(descriptor);
    }

    if (strlen(command) == 0) {
        process_no_command_warning(order); // print header by itself
    }

    // TODO: parse command string
    CommandParser(command, file, &args, &size);
    
    // TODO: print head summary
    int printPID = fork();
    if (printPID == -1) {
        process_allocation_exception();
    }    
    if (printPID == 0) {
        printf("-----CMD %d: %s", order, command);
        int paddingSize = SIZE_OF_DELIMITER_LINE - 12 - strlen(command);
        for (int i = 0; i < paddingSize; i++) {
            printf("-");
        }
        printf("\n");
    }
    else {
        int print_res = wait(nullptr);
        if (print_res == -1) {
            process_wait_exception();
        }

        // TODO: check valid of command and arguments
        if ((strlen(file) == 0) && (isCommandWithTarget(args[0]) && (access(args[size-1], F_OK) != 0))) {
            // if target file is not found in file or the last argument, then PROCESS_COMMAND_ERROR
            process_command_exception(args[0]);
        }

        // TODO: create a new process to execute command
        args[size++] = nullptr; // add nullptr for execvp()
        struct timeval start_t, end_t;
        gettimeofday(&start_t, 0x0);

        int execPID = fork();
        if (execPID == -1) {
            process_allocation_exception();
        }
        if (execPID == 0) {
            int status = execvp(args[0], args);
            exit(status);
        }
        else {
            // TODO: catch exit status of child process
            int execWstatus;
            int execRes = waitpid(execPID, &execWstatus, 0);
            if (execRes == -1) {
                process_wait_exception(args[0]);
            }

            gettimeofday(&end_t, 0x0);
            // runtime_t in microseconds
            int runtime_t = (end_t.tv_sec - start_t.tv_sec) * 1000000 + (end_t.tv_usec - start_t.tv_usec);
            double runtime = (double)runtime_t / 1000;

            if (WIFEXITED(execWstatus)) {
                // @statusCode: indicate process status: 
                //      0 for normal exit, 255 and 2 for invalid use of command
                int statusCode = WEXITSTATUS(execWstatus);
                if (DEBUG) {
                    printf("Received status code: %d\n", statusCode);
                }
                if (statusCode == 255) {
                    process_execvp_exception(args[0]);
                } 
                else if (statusCode == 1) {
                    process_file_directory_exception();
                }
                else if (statusCode == 2) {
                    process_command_usage_exception(args[0]);
                }
                else {
                    // TODO: output command summary to cachefile -- summary
                    printf("%s[Success]: result took: %.0fms%s\n", KGRN, runtime, RESET);
                }
            }

            // TODO: free dynamic memory
            if (args != nullptr) {
                free(args);
            }
        }
    }

    return 0; 
}

/**
 * @brief Reporter
 * 
 * @param jobQueue: job with process id in order
 * @param statusQueue: status code responding to jobQueue
 * @param runtimeMain: run time for main process
 * @param file: target file
 * @return STATUS: 0 for success
 * 
 * The function will generate summary report.
 * 1. detailed report is generated by a new process.
 * 2. summary is written to stdout.
 */
STATUS Reporter(int* jobQueue, int* statusQueue, double runtimeMain, char* file) {
    // TODO: find cache file 'job_{res_job1}_cache' and output cache file in order
    int detailID = fork();
    if (detailID == -1) {
        process_allocation_exception();
    }
    if (detailID == 0) {
        char cacheName1[CACHE_NAME_SIZE];
        char cacheName2[CACHE_NAME_SIZE];
        char cacheName3[CACHE_NAME_SIZE];
        sprintf(cacheName1, "job_%d_cache", jobQueue[0]);
        sprintf(cacheName2, "job_%d_cache", jobQueue[1]);
        sprintf(cacheName3, "job_%d_cache", jobQueue[2]);
        if ((access(cacheName1, F_OK) == 0) && (access(cacheName2, F_OK) == 0) && (access(cacheName1, F_OK) == 0)){
            int status = execlp("cat", "cat", cacheName1, cacheName2, cacheName3, (char*)NULL);
            if (status == -1) {
                process_execvp_exception("cat");
            }
        }
        else {
            exit(-1);
        }
    }
    else {
        int wstatus = 0;
        int res_detail = wait(&wstatus);
        if (res_detail == -1) {
            process_wait_exception();
        }
        if (WIFEXITED(wstatus)) {
            int statusCode = WEXITSTATUS(wstatus);
            if (statusCode == PROCESS_EXECVP_ERROR) {
                exit(PROCESS_EXECVP_ERROR);
            }
        }
        // TODO: output summary
        for (int i = 0; i < SIZE_OF_DELIMITER_LINE; i++) {
            printf("-");
        }
        printf("\n");

        int success = 0;
        int warning = 0;
        for (int i = 0; i < NUM_OF_JOBS; i++) {
            if (statusQueue[i] == 0) {
                success++;
            }
            if (statusQueue[i] == PROCESS_NO_COMMAND_WARNING) {
                warning++;
            }
        }
        
        printf("Summary: success: %d, warning: %d, failure: %d", 
               success, warning, 3 - success - warning);
        if (strlen(file) == 0) {
            printf("\t  Target file: <blank>\n");
        }
        else {
            printf("\t  Target file: %s\n", file);
        }
        
        printf("Children process IDs (status code): ");
        for (int i = 0; i < NUM_OF_JOBS; i++) {
            printChildrenProcess(jobQueue[i], statusQueue[i]);
        }

        printf("\n");
        printf("Total elapsed time: %.0fms\n", runtimeMain);
    }
    
    return 0;
}

/**
 * @brief Cleaner
 * 
 * @param jobQueue: jobs with process ID in order 
 * @return STATUS: 0 for success
 * 
 * The function will clean cache file generated by worker processes
 */
STATUS Cleaner(int* jobQueue) {
    int cacheID = fork();
    if (cacheID == -1) {
        process_allocation_exception();
    }
    if (cacheID == 0) {
        char cacheName1[CACHE_NAME_SIZE];
        char cacheName2[CACHE_NAME_SIZE];
        char cacheName3[CACHE_NAME_SIZE];
        sprintf(cacheName1, "job_%d_cache", jobQueue[0]);
        sprintf(cacheName2, "job_%d_cache", jobQueue[1]);
        sprintf(cacheName3, "job_%d_cache", jobQueue[2]);
        int status = execlp("rm", "rm", "-rf", cacheName1, cacheName2, cacheName3, nullptr);
        if (status == -1) {
            process_execvp_exception("rm");
        }
    }
    else {
        int wstatus = 0;
        int res_cache = wait(&wstatus);
        if (res_cache == -1) {
            process_wait_exception();
        }
        if (WIFEXITED(wstatus)) {
            int statusCode = WEXITSTATUS(wstatus);
            if (statusCode == PROCESS_EXECVP_ERROR) {
                exit(PROCESS_EXECVP_ERROR);
            }
        }
    }

    return 0;
}


void WaitStatusParser(int pid, int wstatus, int* jobQueue, int* statusQueue) {
    int order = 0;
    for (int i = 0; i < NUM_OF_JOBS; i++) {
        if (pid == jobQueue[i]) {
            order = i;
            break;
        }
    }

    if (order == 0) {
        printf("First process [pid: %d] is finished...\n", pid);
    }
    else if (order == 1) {
        printf("Second process [pid: %d] is finished...\n", pid);
    }
    else {
        printf("Third process [pid: %d] is finished...\n", pid);
    }

    if (WIFEXITED(wstatus)) {
        int statusCode = WEXITSTATUS(wstatus);
        statusQueue[order] = statusCode;
    }
}

int main(int argc, char* argv[]) {
    int message[2]; // 0 for read, 1 for write
    if (pipe(message) == -1) {
        process_pipe_exception();
    }
    int uiPID = fork();
    if (uiPID == -1) {
        process_allocation_exception();
    }
    if (uiPID == 0) {
        // TODO: UI Process
        MsgCollector(message);
    }
    else {
        // TODO: Main Process
        int res = wait(nullptr);
        if (res != uiPID) {
            process_wait_exception();
        }

        // TODO: parse message from pipe
        int numberOfEntries = 0; 
        char** commands = nullptr;
        MessageParser(message, &numberOfEntries, &commands);
        char* file = commands[numberOfEntries-1];

        // TODO: Dispatch tasks to children processes
        int job1PID = fork();
        if (job1PID == -1) {
            process_allocation_exception();
        }
        if (job1PID == 0) {
            // TODO: Job1 Process
            Worker(commands[0], file, 1);
        }
        else {
            int job2PID = fork();
            if (job2PID == -1) {
                process_allocation_exception();
            }
            if (job2PID == 0) {
                // TODO: Job2 Process
                Worker(commands[1], file, 2);
            }
            else {
                int job3PID = fork();
                if (job3PID == -1) {
                    process_allocation_exception();
                }
                if (job3PID == 0) {
                    // TODO: Job3 Process
                    Worker(commands[2], file, 3);
                }
                else {
                    // TODO: Main Process:
                    struct timeval start_main, end_main;
                    gettimeofday(&start_main, 0x0);
                    
                    int jobQueue[NUM_OF_JOBS] = {job1PID, job2PID, job3PID};
                    int statusQueue[NUM_OF_JOBS] = {0, 0, 0};
                    int wstatus;
                    int res;
                    printf("\n"); 
                    res = wait(&wstatus);
                    WaitStatusParser(res, wstatus, jobQueue, statusQueue);
                    res = wait(&wstatus);
                    WaitStatusParser(res, wstatus, jobQueue, statusQueue);
                    res = wait(&wstatus);
                    WaitStatusParser(res, wstatus, jobQueue, statusQueue);
                    
                    gettimeofday(&end_main, 0x0);
                    int runtime_main = (end_main.tv_sec - start_main.tv_sec) * 1000000 + (end_main.tv_usec - start_main.tv_usec);
                    double runtimeMain = (double)runtime_main / 1000;

                    if (DEBUG) {
                        printf("Main Process: after waiting three working process done:\n");
                        printf("job1 id = %d, status code = %d\n", jobQueue[0], statusQueue[0]);
                        printf("res_job2 = %d, status code = %d\n", jobQueue[1], statusQueue[1]);
                        printf("res_job3 = %d, status code = %d\n", jobQueue[2], statusQueue[2]);
                        printf("\n");
                    }

                    int reporterPID = fork();
                    if (reporterPID == -1) {
                        process_allocation_exception();
                    }
                    if (reporterPID == 0) {
                        Reporter(jobQueue, statusQueue, runtimeMain, file);
                    }
                    else {
                        int output_res = wait(&wstatus);
                        if (output_res == -1) {
                            process_wait_exception();
                        }

                        int cleanerPID = fork();
                        if (cleanerPID == -1) {
                            process_allocation_exception();
                        }
                        if (cleanerPID == 0) {
                            Cleaner(jobQueue);
                        }
                        else {
                            // TODO: delete dynamic memory
                            for (int i = 0; i < numberOfEntries; i++) {
                                if (commands[i] != nullptr) {
                                    free(commands[i]);
                                    commands[i] = nullptr;
                                }
                            }
                            if (commands != nullptr) {
                                free(commands);
                            }
                        }                        
                    }                    
                }
            }
        }
    }

    return 0;
}
