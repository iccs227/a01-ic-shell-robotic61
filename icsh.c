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
#include <fcntl.h>

#define MAX_CMD_BUFFER 255
#define MAX_ARGS       128 // for number of arguments (external command)

pid_t fg_pid = -1; // stores the PID of the foreground child (-1 if none)
int   last_status = 0;  // exit status of last foreground process

void handle_sigint(int sig) {
    if (fg_pid > 0) {
        // if a child is running, forward SIGINT
        printf("\n");
        kill(fg_pid, SIGINT);
    }
    else {
        // otherwise just reprint prompt
        printf("\nicsh $ ");
        fflush(stdout);
    }
}

void handle_sigtstp(int sig) {
    if (fg_pid > 0) {
        // if a child is running, forward SIGTSTP
        printf("\n");
        kill(fg_pid, SIGTSTP);
    }
    else {
        // otherwise just reprint prompt
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
        // repeat last command
        if (strlen(last_cmd) == 0) {
            // no last command
            return 0;
        }
        printf("%s\n", last_cmd);
        strcpy(buffer, last_cmd);
    }
    else if (strlen(buffer) > 0) {
        // save new command
        strcpy(last_cmd, buffer);
    }
    return 1;
}

void runCmd(char *cmd, char last_cmd[], char buffer[]) {
    // parse tokens and I/O redirection 
    char *tokens[MAX_ARGS];
    char *infile = NULL;
    char *outfile = NULL;
    int   ntok = 0;

    // first token is the command
    tokens[ntok] = cmd;
    ntok = ntok + 1;

    char *t = strtok(NULL, " ");
    while (t != NULL && ntok < MAX_ARGS - 1) {
        if (strcmp(t, "<") == 0) {
            // input redirection
            t = strtok(NULL, " ");
            if (t != NULL) {
                infile = t;
            }
        }
        else if (strcmp(t, ">") == 0) {
            // output redirection
            t = strtok(NULL, " ");
            if (t != NULL) {
                outfile = t;
            }
        }
        else {
            tokens[ntok] = t;
            ntok = ntok + 1;
        }
        t = strtok(NULL, " ");
    }
    tokens[ntok] = NULL;

    // built-in commands with parent-level redirection
    int is_echo = (strcmp(cmd, "echo") == 0);
    int is_exit = (strcmp(cmd, "exit") == 0);

    if (is_echo == 1 || is_exit == 1) {
        int saved_in  = -1;
        int saved_out = -1;

        // redirect input if requested
        if (infile != NULL) {
            saved_in = dup(STDIN_FILENO);
            int fd = open(infile, O_RDONLY);
            if (fd < 0) {
                perror("open input");
                goto restore;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        // redirect output if requested
        if (outfile != NULL) {
            saved_out = dup(STDOUT_FILENO);
            int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open output");
                goto restore;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // handle echo
        if (is_echo == 1) {
            char **args = tokens + 1;
            if (args[0] != NULL && strcmp(args[0], "$?") == 0) {
                // print last status
                printf("%d\n", last_status);
            }
            else {
                int i = 0;
                while (args[i] != NULL) {
                    printf("%s", args[i]);
                    if (args[i + 1] != NULL) {
                        printf(" ");
                    }
                    i = i + 1;
                }
                printf("\n");
            }
        }
        else {
            // handle exit
            int code = 0;
            if (tokens[1] != NULL) {
                code = atoi(tokens[1]) & 0xFF;
            }
            printf("bye\n");
            exit(code);
        }

    restore:
        // restore fds
        if (saved_out != -1) {
            dup2(saved_out, STDOUT_FILENO);
            close(saved_out);
        }
        if (saved_in  != -1) {
            dup2(saved_in, STDIN_FILENO);
            close(saved_in);
        }
        return;
    }

    // external command: fork + child-level redirection
    pid_t pid = fork();
    if (pid < 0) {
        perror("failed to fork");
        last_status = 1;
        return;
    }
    if (pid == 0) {
        // child: apply redirection if needed
        if (infile != NULL) {
            int fd = open(infile, O_RDONLY);
            if (fd < 0) {
                perror("open input");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (outfile != NULL) {
            int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open output");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        execvp(tokens[0], tokens);
        perror("failed to execute");
        exit(1);
    }
    else {
        // parent: wait for child
        fg_pid = pid;
        int status;
        waitpid(pid, &status, WUNTRACED);
        fg_pid = -1;
        if (WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        }
        else {
            last_status = 1;
        }
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
        char *cmd = strtok(buffer, " ");
        if (cmd == NULL) continue;
        runCmd(cmd, last_cmd, buffer);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT,  handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    if (argc == 2) {
        scriptMode(argv[1]);
    } else {
        interactiveMode();
    }
    return 0;
}
