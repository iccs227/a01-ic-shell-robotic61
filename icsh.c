/* ICCS227: Project 1: icsh
 * Name: Ratchapong Tiebsornchai
 * StudentID: 6380164
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_CMD_BUFFER 255

void runCmd(char *cmd, char last_cmd[], char buffer[]);
void scriptMode(const char *filename);
// 	A pointer to read-only string data.
void interactiveMode();
void prompt();
int handleHistory(char buffer[], char last_cmd[]);

int main(int argc, char *argv[]) {
    if (argc == 2) {
        scriptMode(argv[1]);
    } else {
        interactiveMode();
    }
    return 0;
}

void prompt() {
    printf("icsh $ ");
    fflush(stdout);
}

int handleHistory(char buffer[], char last_cmd[]) {
    if (strcmp(buffer, "!!") == 0) {
        if (strlen(last_cmd) == 0) {
            return 0; // no last command
        }
        printf("%s\n", last_cmd);
        strcpy(buffer, last_cmd);
    } else if (strlen(buffer) > 0) {
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

    printf("bad command\n");
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

        char *cmd = strtok(buffer, " ");
        if (cmd == NULL) continue;

        runCmd(cmd, last_cmd, buffer);
    }
}