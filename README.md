# MASH Shell

## Design

MASH shell is a multi-process shell, where main process, as a manager, is responsible for distributing tasks to child processes.

MASH shell is gonna achieve to execute three commands prompted from users concurrently.

The basic structure is shown below.

```shell
# Main Process 
#     ||          
#     ||  dispatch       
#     ||------------->       UI process
#   * ||<------------  (parse input from stdin)
#     ||   message
#     ||
#     || MessageParser()
#     ||                                          _______
#     ||-----> job process 1 ---> exec process---|       |
#     ||                               |         | cache |
#   W || <--------signal---------------|         |       |
#   A ||                                         | cache |
#   I ||-----> job process 2 ---> exec process---|       |
#   T ||                                         | files |
#     ||-----> job process 3 ---> exec process---|       |
#     ||                               |         |-------|
#   * || <--------------signal---------|           | |
#     ||                                           | |
#     ||                              read         | |
#     ||-----> reporter process <---------<--------| |
#     ||        (output info)                        |
#   * || <---signal--|                               |
#     ||                                             |
#     ||                              delete         |
#     ||-----> cleaner process ----->----->----->----|
#     ||             |
#   * || <---signal--|
#     ||
#     || CleanDynamicBuffer()
#     ||
#    EXIT
```

## Usage and Error Code

### Usage

There are two ways to use MASH.

- Three independent commands are allowed to execute concurrently. In this case, leave file field to be blank.

```shell
# Input example 1
$ ./mash
mash-1> ls .
mash-2> echo hello
mash-3> pwd
file>
```

```shell
# Output
First process [pid: 1343] is finished...
Second process [pid: 1344] is finished...
Third process [pid: 1345] is finished...
-----CMD 1: ls .----------------------------------------------------------------
Makefile
README.md
mash
mash.c
mash.h
mash.o
masherror.c
masherror.h
masherror.o
[Success]: result took: 7ms
-----CMD 2: echo hello----------------------------------------------------------
hello
[Success]: result took: 10ms
-----CMD 3: pwd-----------------------------------------------------------------
/Users/minzhiqu/Desktop/Study/OS/MASH
[Success]: result took: 11ms
--------------------------------------------------------------------------------
Summary: success: 3, warning: 0, failure: 0	  Target file: <blank>
Children process IDs (status code): 1343(0) 1344(0) 1345(0) 
Total elapsed time: 13ms
```

- If three commands share common target file, then input filename after `file>`.

```shell
# Input example 2
mash-1> grep -c the
mash-2> wc -l
mash-3> ls -l
file> README.md
```

```shell
# Output
First process [pid: 1462] is finished...
Second process [pid: 1463] is finished...
Third process [pid: 1464] is finished...
-----CMD 1: grep -c the---------------------------------------------------------
4
[Success]: result took: 6ms
-----CMD 2: wc -l---------------------------------------------------------------
     580 README.md
[Success]: result took: 5ms
-----CMD 3: ls -l---------------------------------------------------------------
-rw-r--r--@ 1 minzhiqu  staff  18922 Nov 14 08:17 README.md
[Success]: result took: 10ms
--------------------------------------------------------------------------------
Summary: success: 3, warning: 0, failure: 0	  Target file: README.md
Children process IDs (status code): 1462(0) 1463(0) 1464(0) 
Total elapsed time: 13ms
```

### Error Code

- `PROCESS_PIPE_ERROR 240`: fail to create pipe for process communication.
- `PROCESS_ALLOCATION_ERROR 241`: fail to create a new process.
- `PROCESS_WAIT_ERROR 242`: fail to wait specific process to finish.
- `PROCESS_EXECVP_ERROR 243`: fail to execute command user provides.
- `PROCESS_COMMAND_USAGE_ERROR 244`: valid command but wrong usage.
- `PROCESS_COMMAND_ERROR 245`: fail to provide target file for specific commands.
- `PROCESS_FILE_DIRECTORY_ERROR 246`: fail to open given target file or directory.
- `PROCESS_NO_COMMAND_WARNING 124`: no command detect on task process.

```shell
# Input example 3
mash-1> ping -c 3 google.com
mash-2> this is a wrong command
mash-3> 
file> 

# Output
Third process [pid: 3018] is finished...
Second process [pid: 3017] is finished...
First process [pid: 3016] is finished...
-----CMD 1: ping -c 3 google.com------------------------------------------------
PING google.com (142.251.33.110): 56 data bytes
64 bytes from 142.251.33.110: icmp_seq=0 ttl=58 time=10.842 ms
64 bytes from 142.251.33.110: icmp_seq=1 ttl=58 time=19.008 ms
64 bytes from 142.251.33.110: icmp_seq=2 ttl=58 time=42.352 ms

--- google.com ping statistics ---
3 packets transmitted, 3 packets received, 0.0% packet loss
round-trip min/avg/max/stddev = 10.842/24.067/42.352/13.352 ms
[Success]: result took: 2066ms
-----CMD 2: this is a wrong command---------------------------------------------

[Failure]: command 'this' is not found or can not be executed properly.
-----CMD 3: <blank>-------------------------------------------------------------

[Warning]: no input from command line.
--------------------------------------------------------------------------------
Summary: success: 1, warning: 1, failure: 1	  Target file: <blank>
Children process IDs (status code): 3016(0) 3017(243) 3018(124) 
Total elapsed time: 2070ms
```


## Implementation

### Main Process

```c
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
 }
```

### UI Process

UI Process handles tasks those need interact with users. Its main jobs are:

- prompt commands from user.
- package message and put it into message pipe.

```c
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
```

### Message Parser

`MessageParser` is used to parse message from message pipe. It is a decode process which is symmetric to package process in UI process.

```c
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
```

### Job Process

We will generate three job processes and each will handle one command given by main process in parallel.

- redirect output to cache file.
- parse command string with `CommandParser`.
- generate output info.
- create a new process to execute command.
- route back status code of command execution.

```c
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
```

### Reporter Process

Reporter process will read output from cache files and output them in stdout, and then print summary report.

```c
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
```

### Cleaner Process

Cleaner process will clean cache files created by job processes and free dynamic memory as well.

```c
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
```
