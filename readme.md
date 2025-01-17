# Robert Thurston - Simulated Parallel Processing

## Overview
This project was an assignment for a class I took, Computer Systems Principles (Comp 230) at UMass Amherst. I've included the file for the project documentation in this repository: `Simulated Parallel Processing.pdf`. I decided to upload this project because I thought it was super cool.

This project demonstrates a multi-core task management system in C, utilizing pipes, signals, and processes to distribute tasks among multiple cores. The main process then aggregates the results from each core. The project is designed to use three separate cores, but it can be easily adjusted to handle up to 31 processes by modifying the `NUM_CORE` variable. The program outputs the number of tasks completed by each core process and the sum of the task IDs.

## Project Requirments
### User Processes are Correctly Created as Stated in the Requirement (4 pts)
(Lines 277 to 305) I created the core processes using the `fork` system call. To detect any errors, I initialized `errno` to 0 before the system call. The value returned by `fork` is 0 for child/core processes, so I used a conditional to branch the control flow. Each core generates its own seed (for random sleep times), closes any unused pipes, and then calls the `runCore` function, which serves as the core’s main function.

### Pipes are Reaped (3 pts)
(Line 352) I implemented a function called `waitForCore` (Lines 170 to 177) that accepts a process ID (pid) of a core about to terminate. The function calls `waitpid` and checks for errors. If no errors occur, the main process successfully reaps the core process.

### Pipes are Created to Support Two-Way Communication Correctly (3 pts)
I defined two 2D arrays (`mainToCores` and `coresToMain`, located at Lines 259 and 260) to support the pipes for communication. In a loop, I created pipes for each `mainToCores` and `coresToMain` (Lines 265 and 266) using the `createPipe` function (Lines 87 to 97). This function calls `pipe`, handles errors, and creates the new pipes with the provided file descriptors. It's important to note that all pipes are created in the main process before forking the core processes.

### The Corresponding Ends of Pipes are Correctly Closed on Both the Parent and Child Process Ends (8 pts)
Closing pipes was one of the more complex parts of this assignment due to the variety of potential implementations. In the main process, I closed the unused pipe ends using a simple for-loop. I closed the read-end for each pipe in `mainToCores` and the write-end for each pipe in `coresToMain` (Lines 321 and 322). To simplify this, I created a `closePipeEnd` function (Lines 185 to 187), which calls `closeFd` (Lines 71 to 80), handling errors and closing the specified file descriptor.

For the core processes, since all pipes are created in the main process before forking, each core gets a copy. For an arbitrary core `i`, I first closed the read-end of `mainToCores[i]` and the write-end of coresToMain[i] (Lines 293 and 294) using `closePipeEnd`. Then, in a second for-loop, I closed all unused pipes while skipping the current core’s pipes (Lines 298 to 302). I created a function, `closePipe` (Lines 192 to 195), to close both ends of a pipe, as the core process does not need either.

### All Pipes are Correctly Closed in the Clean-Up Stage (4 pts)
The cleanup stage begins once all tasks have been assigned, and the main process waits for all cores to finish. I handled closing pipes in two ways:
- When a core finishes its final task, the main process reads the result as normal and then closes the read-end of the core-to-main pipe (Line 378).
- When a core is idle (ready to terminate), the main process closes the write-end of the main-to-core pipe (Line 351), signaling that no more tasks will be assigned. This allows the core to close both the read-end of the main-to-core pipe and the write-end of the core-to-main pipe before terminating (Lines 219 and 220).

### Correctly Calls the Write Function to Send Data to the Pipe (4 pts)
I created a custom function, `writeToPipe` (Lines 143 to 153), which takes two parameters: the file descriptor and the task ID to write. The function calls `write` and writes the value to the file, handling any errors with `errno`. This function is used when the main process assigns a task to a core (Line 360) and when a core sends the task ID back to the main process upon completion (Line 232).

###  Correctly Calls the Read Function to Read Data from the Pipe (4 pts)
I implemented the `readFromPipe` function (Lines 125 to 135), which takes a file descriptor and a buffer to store the `read` value. The function calls read and stores the result in the buffer, handling errors with `errno`. On successful execution, it returns the buffer value. The main process calls this function whenever there is an unread result from a core (Line 370). I initially considered calling this function in the core process as well, but because of the special case when `read` returns 0 (indicating no more data), I handled reading differently for the core processes (Lines 208 to 223). The core process checks if the value read is 0, which means it can safely terminate.

### Choose Appropriate Signals to Register (3 pts)
(Line 236) I used `SIGRTMIN` and added the core number to create custom signals for each core. For example, Core `i` sends `SIGRTMIN + i` to the main process to notify it that the task is completed. I created the `sendSignalToMain` function (Lines 158 to 165), which uses `kill` to send the signal, with error handling in place.

### Correctly use Sigaction to Register Signal Handlers (3 pts)
I created a helper function, `registerSignalHandler` (Lines 102 to 117), which sets up the sigaction struct. The handler function is `handleResult` (Lines 45 to 49), which is invoked when the main process receives the `SIGRTMIN + i` signal, where `i` is the core number.

### Signal Handler Should Be Correctly Implemented. Its Operation Should Keep Atomic (6 pts)
(Lines 45 to 49) The signal handler retrieves the core ID by subtracting `SIGRTMIN` from the parameter `sig`. Using the core ID, I set the volatile global variable `msgFromCore[id]` to 1, indicating that the core has written the result to the pipe.

### Use the Proper Type of Global Variable as the Indicator of New Results (2 pts)
(Line 23) As specified, the global variable `msgFromCore` is declared as a `volatile sig_atomic_t`, which is the required type for handling signals safely.

### Signal Should be Properly Sent Out When a Message is Sent Out (2 pts)
(Lines 232 to 237) The only instance where a signal is sent is when a core finishes a task. After writing the task ID to the core-to-main pipe, the core immediately sends a signal to the main process to notify it that new data is available.

### The Core Subprocess Can Detect the Close of Pipes and Quit the Status of the Wait (3 pts)
Core processes wait for tasks to be assigned or for the main process to close the pipe. I achieved this by calling `read` (Line 208), which blocks until data is available. If no data is available (indicated by `read` returning 0), the core detects that the write-end of the pipe is closed by the main process (Line 351), and it can safely terminate.

### Error Handling on All the Functions and System Calls Including `read`/`write`/`fork`/`pipe`... (4 pts)
I created helper functions for most system calls used in the program, except for `fork` (Lines 277 to 283) as its behavior involves process cloning and doesn't require complex handling. For every system call, I set `errno` to 0 before the call. If an error occurs, `errno` is updated; if it remains unchanged, the system call is successful.

### Implement the Command Line Arguments to Allow User to Control the Number of Total Tasks (4 pts)
Command-line arguments are passed through the `argv` array. The arguments for the total number of tasks and the maximum process time are parsed into a `long` using the `convertToLong` function (Lines 57 to 66), which calls `strtol`. After conversion, I validate that the values are positive numbers and handle errors gracefully when non-numeric input is provided.

### Do Not Cause Memory Leakage. (3 pts)
I made a diligent effort to avoid memory leaks by ensuring that all allocated resources are properly freed. I close all pipes and reap all core processes to prevent memory issues. You can verify the absence of memory leaks by running the following command: `valgrind --leak-check=full ./main a b` (where `a` and `b` are the required arguments).
