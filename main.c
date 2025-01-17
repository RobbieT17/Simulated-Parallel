#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// Total Number of Cores (MAX: 31)
#define NUM_CORES 3

// FDS
#define READ 0
#define WRITE 1

// Core Status
#define IDLE 0
#define ACTIVE 1
#define TERMINATED 2

// Global Variables
volatile sig_atomic_t msgFromCore[NUM_CORES]; // Signal handle updates this to notify MAIN when a core has finished a task

// *** Helper Function from Instructors *** //
void no_interrupt_sleep(int sec) {
    // * advanced sleep which will not be interfered by signals
    struct timespec req, rem;
    req.tv_sec = sec; // The time to sleep in seconds
    req.tv_nsec = 0; // Additional time to sleep in nanoseconds
    while(nanosleep(&req, &rem) == -1) {
        if(errno == EINTR) {
            req = rem;
        }
    }
}

// *** SIGNAL HANDLER FUNCTION *** //

/** Handles signals from core processes indicating task completion.
 * MAIN updates msgFromCore for the core that sent the signal.
 * 
 * @param sig intercepted signal
 */
void handleResult(int sig) {
    int id = sig - SIGRTMIN; // Identifies core
    msgFromCore[id] = 1;
    printf("(MAIN) recieved signal SIGRTMIN + %d from core %d\n", id, id);    
}

// *** SYSTEM CALL FUNCTIONS *** //

/** Converts a string to a long
 * @param string a string to convert
 * @returns converted long value
 */
long convertToLong(char* string) {
    errno = 0;
    long value = strtol(string, NULL, 10);

    if (errno != 0) { // Some error occured
        printf("(PID: %d) failed to convert to long\n", getpid());
        exit(EXIT_FAILURE);
    }
    return value;
}

/** Closes a file descriptor
 * @param fd file descriptor to close
 */
void closeFd(int fd) {
    errno = 0;

    close(fd);
    if (errno != 0) {
        printf("(PID: %d) failed to close fd\n", getpid());
        exit(EXIT_FAILURE);
    }
    printf("(PID: %d) closed fd %d\n", getpid(), fd);
}

/** Creates a pipe
 * @param fds file descriptors, read and write ends for the pipe
 * @param string indicates the direction of the pipe (main to core/core to main)
 * @param i the core number
 */
void createPipe(int fds[], char* string, int i) {
    errno = 0;

    pipe(fds);
    if (errno != 0) { // An error occured
        printf("(PID: %d) failed to create pipe\n", getpid());
        exit(EXIT_FAILURE);
    }
    // Prints out on success
    printf("(PID %d) created pipe %s %i (read: %d, write: %d)\n", getpid(), string, i, fds[0], fds[1]);
}

/** Registers a custom signal handler
 * @param i a number added to SIGRTMIN (gives each core a its own signal)
 */
void registerSignalHandler(int i) {
    // Signals
    struct sigaction sa;
    sa.sa_handler = handleResult;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);

    errno = 0;
    sigaction(SIGRTMIN + i, &sa, NULL); 
    if (errno != 0) { // An error occured
        printf("(PID %d) failed to create signal handler\n", getpid());
        exit(EXIT_FAILURE);
    }
    // Prints out on success
    printf("(PID %d) created signal handler for SIGRTMIN + %d\n", getpid(), i);
}

/** Calls read, and checks for any errors
 * @param file file descriptor for pipe read-end
 * @param buffer stores the information being read from the pipe
 * 
 * @returns the value read from the pipe
 */
int readFromPipe(int input, int buffer) {
    errno = 0;
    read(input, &buffer, sizeof(buffer));

    if (errno != 0) { // Some error occured
        printf("(PID: %d) read call failed <fd: %d>\n", getpid(), input);
        exit(EXIT_FAILURE);
    }

    return buffer;
}

/** Calls write, and checks for any errors
 * @param output file descriptor for pipe write-end
 * @param buffer holds the infomation being written into the pipe
 * 
 * @returns the total number of bytes written
 */
int writeToPipe(int output, int buffer) {
    errno = 0;

    int result = write(output, &buffer, sizeof(buffer));
    if (errno != 0) { // Some error occured
        printf("(PID: %d) write call failed\n", getpid());
        exit(EXIT_FAILURE);
    }

    return result;
}

/** Sends a signal to the MAIN process
 * @param sig signal to send to main
 */
void sendSignalToMain(int sig) {
    errno = 0;
    kill(getppid(), sig);
    if (errno != 0) {
        printf("(PID: %d) failed to send signal to %d\n", getpid(), getppid());
        exit(EXIT_FAILURE);
    }
}

/** Waits for process to terminate
 * @param pid process id to wait for
 */
void waitForCore(pid_t pid) {
    errno = 0;
    waitpid(pid, NULL, 0);
    if (errno != 0) { // Some error occured
        printf("(PID: %d) failed to wait\n", getpid());
        exit(EXIT_FAILURE);
    }
}

// *** END OF SYSTEM CALL FUNCTIONS *** ///

 /** Closes the read/write end of a pipe 
 * @param fds file descriptors for read and write end of a pipe
 * @param n selected file descriptor to close (0: Read, 1: Write)
 */
void closePipeEnd(int fds[], int n) {
    closeFd(fds[n]);
}

/** Closes pipe (both read and write ends)
 * @param fds file descriptors for read and write end of a pipe
 */
void closePipe(int fds[]) {
    closePipeEnd(fds, READ);
    closePipeEnd(fds, WRITE);
}

/** Core process waits for a task, once assigned reads the tasks id, waits for a random amount of second
 * @param input file descriptor for pipe read-end
 * @param output file descriptor for pipe write-end
 * @param maxTime max random time to sleep
 * @param core the core number
 * 
 * The process terminates after this function is done. 
 */
void runCore(int input, int output, int maxTime, int core) {
    while (1) {
        int task; 
        int result = read(input, &task, sizeof(task)); // Will unblock when something is written to it (or nothing is writing to the pipe)

        // Reads task from mainToCore pipe
        errno = 0;
        if (errno != 0) { // Failed to read
            closeFd(input);
            closeFd(output);
            printf("(CORE %d) failed to read\n", core);
            exit(EXIT_FAILURE);
        }
        if (result == 0) { // No more tasks
            closeFd(input);
            closeFd(output);
            printf("(CORE %d) terminated\n", core);
            exit(EXIT_SUCCESS);
        }

        // Executes task
        int sleepTime = rand() % maxTime + 1; // Sleeps for a random amount of time 

        printf("(CORE %d) started task %d <read from fd %d> [%d seconds]\n", core, task, input, sleepTime);
        no_interrupt_sleep(sleepTime);

        // Writes task id to coreToMain pipe
        writeToPipe(output, task);
        printf("(CORE %d) completed task %d <wrote to fd %d>\n", core, task, output);

        // Sends a signal to MAIN 
        sendSignalToMain(SIGRTMIN + core);
        printf("(CORE %d) sent signal SIGRTMIN + %d to MAIN\n", core, core);
    }  
}

int main(int argc, char* argv[]) {
    // Checks for invalid number of program arguments
    if (argc != 3) {
        printf("Invalid number of program args\n");
        exit(EXIT_FAILURE);
    }

    long taskCount = convertToLong(argv[1]); // Number of tasks
    long maxProcessTime = convertToLong(argv[2]); // Max time a process may take to complete a task
    
    // Checks for invalid arguments
    if (taskCount <= 0 || maxProcessTime <= 0) {
        printf("Program arguments must be positive\n");
        exit(EXIT_FAILURE);
    }

    printf("(Main Process) PID: %d\n", getpid());

    int mainToCores[NUM_CORES][2]; // Main to Core pipes
    int coresToMain[NUM_CORES][2]; // Core to Main pipes

    // Create Pipes and Signals
    for (int i = 0; i < NUM_CORES; i++) {
        // Pipes
        createPipe(mainToCores[i], "Main to Core", i); // Main to Core i
        createPipe(coresToMain[i], "Core to Main", i); // Core i to Main

        // Signals
        registerSignalHandler(i);
    }

    // PIDs for Cores
    pid_t pids[NUM_CORES];

    // Creates Core Processes
    for (int i = 0; i < NUM_CORES; i++) {
        pids[i] = fork();

        errno = 0;
        if (errno != 0) {
            printf("(MAIN) failed to fork\n");
            exit(EXIT_FAILURE);
        } 
        else if (!pids[i]) {
            // *** CORE PROCESS BEGINS *** //
            printf("(Core Process %d) PID: %d\n", i, getpid());

            // Generates random seed for randomized sleep times
            unsigned int seed = time(NULL) * rand() * getpid();
            srand(seed);

            // Close unused pipe ends
            closePipeEnd(mainToCores[i], WRITE); // Core reads input from Main
            closePipeEnd(coresToMain[i], READ); // Core writes output to Main

            // Fully Close Unused Pipes
            for (int j = 0; j < NUM_CORES; j++) {
                if (j == i) { // Skips closing current Core i pipes
                    continue;
                }
                closePipe(mainToCores[j]);
                closePipe(coresToMain[j]);
            }
            
            runCore(mainToCores[i][READ], coresToMain[i][WRITE], maxProcessTime, i); // Completes tasks assigned from main
            // *** CORE STOPS EXECUTION  *** //
        }
        // *** Parent Process Continues *** //     
    }

    //*** ONLY MAIN PROCESS ONLY ***//

    // Current status of cores (0: Idle, 1: Active, 2: Terminated)
    int coresStatus[NUM_CORES]; 

    // Number of completed tasks for each core
    int counters[NUM_CORES]; 

    // Closes unused pipe ends and initializes counters
    for (int i = 0; i < NUM_CORES; i++) {
        closePipeEnd(mainToCores[i], READ);
        closePipeEnd(coresToMain[i], WRITE);

        coresStatus[i] = IDLE; // Initializes core i status to IDLE
        counters[i] = 0; // Initializes task counter of core i to 0 
    }

    // Stalls for a second (Ensures that all cores finish setup before assigning tasks)
    sleep(1);

    // Assigns tasks to cores until no more tasks. 
    // Then waits for all children to finish before giving out a result.

    int sum = 0; // Summation of results from all cores
    int task = 0; // Current task number
    int noTasks = 0; // Set to 1 (TRUE) once all tasks have been assigned
    int terminatedCores = 0; // Number of terminate cores
    
    // Loops until all tasks have been assigned and cores finish
    while (1) {
        for (int i = 0; i < NUM_CORES; i++) { 
            // Checks if there are no more tasks to assign
            if (!noTasks && task >= taskCount) { 
                printf("(MAIN) no more tasks, waiting for cores to finish...\n");
                noTasks = 1;
            }

            int coreStatus = coresStatus[i];
            if (coreStatus == IDLE) { 
                if (noTasks) { // No more tasks, close pipe write end and wait for core termination
                    closePipeEnd(mainToCores[i], WRITE);
                    waitForCore(pids[i]);
                    coresStatus[i] = TERMINATED;
                    terminatedCores++;
                    printf("(MAIN) reaped core %d\n", i);
                    continue;
                }

                // Assigns new task to core process and sets its status to ACTIVE
                writeToPipe(mainToCores[i][WRITE], task);
                coresStatus[i] = ACTIVE;
                printf("(MAIN) assigned task %d to core %d <writes to fd %d>\n", task++, i, mainToCores[i][WRITE]);       
            }
            else if (coreStatus == ACTIVE) {
                if (!msgFromCore[i]) { // Core is still running, skip!
                    continue;
                }

                // Reads result from core, resets core status to IDLE, and aggregates result
                int result = readFromPipe(coresToMain[i][READ], result);
                msgFromCore[i] = 0;
                coresStatus[i] = IDLE;
                counters[i]++;
                sum += result;
                printf("(MAIN) recieved task %d from core %d <read from fd %d>\n", result, i, coresToMain[i][READ]);

                if (noTasks) { // Close read end, no more tasks
                    closePipeEnd(coresToMain[i], READ);
                }
            }
        }

        // Breaks loop once ALL cores finish
        if (terminatedCores == NUM_CORES) {
            break;
        }
    }

    // *** POST-PROCESSING *** //
    printf("All Done!\n");
    
    // Prints out the number of tasks each core completed
    for (int i = 0; i < NUM_CORES; i++) {
        printf("Core %d: completed %d task(s)\n", i, counters[i]);
    }

    // Summation of task IDs
    printf("Task Sum: %d\n", sum);

    return 0;
}