/* ICCS227: Project 1: icsh
 * Name: Ratchapong Tiebsornchai
 * StudentID: 6380164
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CMD_BUFFER 255
#define MAX_ARGS 128 // for number of arguments(external command)

void runCmd(char *cmd, char last_cmd[], char buffer[]);
void scriptMode(const char *filename);
// 	A pointer to read-only string data.
void interactiveMode();
void prompt();
int handleHistory(char buffer[], char last_cmd[]);
// declare the functions on top so main can see.

int main(int argc, char *argv[]) {
    if (argc == 2) {
        scriptMode(argv[1]);
    } else {
        interactiveMode();
    }
    return 0;
    // program exits after script mode (return 0 = exit(0))
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
        int status;
        // stores the exit status of the child process.
        waitpid(pid, &status, 0);
        // wait for the child process to finish to continue.
        // use 0 for default behavior.
    }

    else {
        perror("failed to fork");
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
