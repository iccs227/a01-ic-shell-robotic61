/* ICCS227: Project 1: icsh
 * Name: Ratchapong Tiebsornchai
 * StudentID: 6380164
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CMD_BUFFER 255
#define MAX_ARGS 128 // for number of arguments(external command)

pid_t fg_pid = -1; // stores the PID of the child process(-1 is never a valid PID 
//meaning no child process now)
int last_status = 0;
// for built-in commands we do not update last_statusm so it stays at 0.
// only the child process will update the last_status.

void handle_sigint(int sig) {
    if (fg_pid > 0) {
        printf("\n");
        kill(fg_pid, SIGINT);
    }
    else { // only print prompt if no child is running.
        printf("\nicsh $ ");
        fflush(stdout);
        // After the signal handler runs, it resumes exactly where it was interrupted.
        // so continues at fgets().
    }
    // if signal sent and there is no child process we do nothing(go back to prompt).
}

void handle_sigtstp(int sig) {
    if (fg_pid > 0) {
        printf("\n");
        kill(fg_pid, SIGTSTP);
    }
    else { // only print prompt if no child is running.
        printf("\nicsh $ ");
        fflush(stdout);
    }
}

void prompt() {
    printf("icsh $ ");
    fflush(stdout);
}

int handleHistory(char buffer[], char last_cmd[]) {
    if (strcmp(buffer, "!!") == 0) {
        if (strlen(last_cmd) == 0) {
            return 0; // no last command
			// returns here are used for if statement in the program.
        }
        printf("%s\n", last_cmd);
        strcpy(buffer, last_cmd);
    } 
    else if (strlen(buffer) > 0) {
        strcpy(last_cmd, buffer);
    }
    return 1;
}

void runCmd(char *cmd, char last_cmd[], char buffer[]) {
    if (strcmp(cmd, "echo") == 0) {
        char *next = strtok(NULL, " ");

        if (next && strcmp(next, "$?") == 0) {
            printf("%d\n", last_status);
            return; // exits the function early(does not return anything)
        }

        while (next != NULL) {
            printf("%s ", next);
            next = strtok(NULL, " ");
        }
        printf("\n");
        return;
    }

    if (strcmp(cmd, "exit") == 0) {
        char *next = strtok(NULL, " ");
        int exit_code = 0;
        if (next != NULL) {
            exit_code = atoi(next) & 0xFF;
        }
        printf("bye\n");
        exit(exit_code);
    }

    // External command(replaced the bad command earlier)
    char *args[MAX_ARGS]; // array of strings to keep argument lists, used to run execvp
    int i = 0;
    char *token = cmd; //from the interactive/script mode function
    while(token != NULL && i < MAX_ARGS -1) {
        args[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    args[i] = NULL; //use this to show NULL terminaation for the execvp
    // execvp only looks until it sees NULL.
    pid_t pid = fork();

    if (pid == 0) {
        execvp(args[0], args);
        // after execvp succeeds, the current process is gone.
        // so no code after this will run unless execvp fails.
        perror("failed to execute");
        // prints the most recent failed error
        exit(1);
        // exit with error
    }

    else if (pid > 0) {
        fg_pid = pid;
        // set 
        int status;
        // stores an integer of how the child process terminates.
        waitpid(pid, &status, WUNTRACED); // set instead of default to WUNTRACED.
        // parent waits for child to either exit normally or get suspended.
        // now the child stays suspended and parent gain control of the terminal.
        // so prompt can return right away and dont have to wait for the 
        // SUSPENDED child process.
        // wait for the child process to finish to continue.
        // use 0 for default behavior.

        fg_pid = -1; // PREPARE TO CHANGE THIS to resume the child process.

        // resets the fg_pid back to -1 after the child finishes(no more child process)

        if (WIFEXITED(status)) {
            // true if the child exited normally
            last_status = WEXITSTATUS(status);
            // returns the exit code of the child
        }

        else {
            last_status = 1;
        }

    }

        else {
            // for the case where fork returns -1, meaning the process creation failed.
            perror("failed to fork");
            last_status = 1;
            // 1 means error.
        }

}

void scriptMode(const char *filename) {
    char buffer[MAX_CMD_BUFFER];
    char last_cmd[MAX_CMD_BUFFER] = "";
    FILE *input = fopen(filename, "r");

    if (input == NULL) {
        printf("Could not open the file.\n");
        return;
    }

    while (fgets(buffer, MAX_CMD_BUFFER, input) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        if (!handleHistory(buffer, last_cmd)) continue;

        char *cmd = strtok(buffer, " ");
        if (cmd == NULL) continue;

        runCmd(cmd, last_cmd, buffer);
    }

    fclose(input);
}

void interactiveMode() {
    char buffer[MAX_CMD_BUFFER];
    char last_cmd[MAX_CMD_BUFFER] = "";

    printf("Starting IC shell\n");

    while (1) {
        prompt();
        if (fgets(buffer, MAX_CMD_BUFFER, stdin) == NULL) {
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';
        if (!handleHistory(buffer, last_cmd)) continue;
		// handleHistory runs first then check the condtion.
		// continue, for the case of !!, but no last command. (0! = 1 = true) back to prompt.

        char *cmd = strtok(buffer, " ");
        if (cmd == NULL) continue;

        runCmd(cmd, last_cmd, buffer);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    // we put signal handler here cuz we want to set up the signal handlers asap.
    // signal handler is applied to both script and interactive mode.
    if (argc == 2) {
        scriptMode(argv[1]);
    } else {
        interactiveMode();
    }
    return 0;
    // program exits after script mode (return 0 = exit(0))
    // thats why the scriptmode exits even without exit command in the file.
}
