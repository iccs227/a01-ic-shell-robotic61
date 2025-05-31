// icsh.c
// Milestone 6: Background jobs & job control
// I wrote this shell so I can run commands, send them to background, and control jobs.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>    // I need this to check errors in handle_sigchld()

#define MAX_CMD_BUFFER 255
#define MAX_ARGS       128
#define MAX_JOBS       64

// ────────────────────────────────────────────────────────────────────────────
// Global state (from Milestone 5)

// I keep track of the PID of the process currently running in foreground.
// If no foreground child is running, fg_pid is -1.
pid_t fg_pid = -1;

// I store the exit status of the last foreground process here.
int last_status = 0;

// ────────────────────────────────────────────────────────────────────────────
// Milestone 6: job table + SIGCHLD handler + built-in job control

typedef enum { JOB_RUNNING, JOB_STOPPED } job_state_t;

// I define a struct job_t to store each background job’s ID, PID, state, and command line.
typedef struct {
    int          id;               // my job ID (1, 2, 3, …)
    pid_t        pid;              // child PID
    job_state_t  state;            // either JOB_RUNNING or JOB_STOPPED
    char         cmdline[MAX_CMD_BUFFER];  // the full command line I launched
} job_t;

// job_list[] holds all current background jobs. num_jobs is how many are active.
// next_job_id is what I assign next time I start a new job.
static job_t job_list[MAX_JOBS];
static int   num_jobs = 0;
static int   next_job_id = 1;

// just_handled_stop tells interactiveMode() not to print a duplicate prompt
// if runCmd() already printed one when a job was stopped.
static int just_handled_stop = 0;


// I scan job_list[] to find a job whose pid matches. I return its index or -1.
static int find_job_by_pid(pid_t pid) {
    for (int i = 0; i < num_jobs; i++) {
        if (job_list[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

// I scan job_list[] to find a job whose id matches. I return its index or -1.
static int find_job_by_id(int jid) {
    for (int i = 0; i < num_jobs; i++) {
        if (job_list[i].id == jid) {
            return i;
        }
    }
    return -1;
}

// When a job finishes or is removed, I want to delete it from job_list.
// I shift everything after idx down one.
static void remove_job_by_index(int idx) {
    if (idx < 0 || idx >= num_jobs) return;
    for (int i = idx; i < num_jobs - 1; i++) {
        job_list[i] = job_list[i + 1];
    }
    num_jobs--;
}

// Whenever a background job changes state (Done, Stopped, Continued),
// I print its status over the prompt (using "\r"), then reprint "icsh $ ".
static void report_job_status(job_t *j, int status) {
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        // the job terminated normally or was killed by a signal
        printf("\r[%d]  Done        %s\n", j->id, j->cmdline);
        printf("icsh $ ");
        fflush(stdout);
    }
    else if (WIFSTOPPED(status)) {
        // the job was stopped by SIGTSTP
        printf("\r[%d]  Stopped     %s\n", j->id, j->cmdline);
        printf("icsh $ ");
        fflush(stdout);
    }
    else if (WIFCONTINUED(status)) {
        // the job was resumed by SIGCONT
        printf("\r[%d]  Continued   %s\n", j->id, j->cmdline);
        printf("icsh $ ");
        fflush(stdout);
    }
}

// This is my SIGCHLD handler. Whenever any child (background or stopped) changes state,
// I loop on waitpid(..., WNOHANG|WUNTRACED|WCONTINUED) to reap all changes.
// If I find a job in job_list[], I update or remove it and call report_job_status().
static void handle_sigchld(int sig) {
    int saved_errno = errno;
    pid_t pid;
    int status;

    // keep collecting children that changed state
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int idx = find_job_by_pid(pid);
        if (idx >= 0) {
            job_t *j = &job_list[idx];

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // job finished: print "Done" and remove it from the table
                report_job_status(j, status);
                remove_job_by_index(idx);
            }
            else if (WIFSTOPPED(status)) {
                // job was stopped: mark as stopped, then print "Stopped"
                j->state = JOB_STOPPED;
                report_job_status(j, status);
            }
            else if (WIFCONTINUED(status)) {
                // job was resumed: mark as running, then print "Continued"
                j->state = JOB_RUNNING;
                report_job_status(j, status);
            }
        }
        // if pid isn’t in job_list, it was a foreground child; ignore here
    }

    errno = saved_errno;
}

// When I launch a new background job, I add it to job_list[] and print its job ID and PID.
// If I already have MAX_JOBS, I print an error and skip it.
static void add_job(pid_t pid, const char *cmdline) {
    if (num_jobs >= MAX_JOBS) {
        fprintf(stderr, "icsh: too many background jobs (max %d)\n", MAX_JOBS);
        return;
    }
    job_t *j = &job_list[num_jobs++];
    j->id    = next_job_id++;
    j->pid   = pid;
    j->state = JOB_RUNNING;
    strncpy(j->cmdline, cmdline, MAX_CMD_BUFFER - 1);
    j->cmdline[MAX_CMD_BUFFER - 1] = '\0';

    // show "[jobID] pid"
    printf("[%d] %d\n", j->id, j->pid);
    fflush(stdout);
}

// Built-in "jobs": I just loop through job_list[], and for each job I print its ID,
// then "Running" or "Stopped", then the saved cmdline plus "&".
static void builtin_jobs(void) {
    for (int i = 0; i < num_jobs; i++) {
        job_t *j = &job_list[i];
        const char *st = (j->state == JOB_RUNNING ? "Running" : "Stopped");
        printf("[%d] %s %s &\n", j->id, st, j->cmdline);
    }
}

// Built-in "fg": bring a stopped or background job to the foreground.
// If arg is NULL, I pick the most recent job. Otherwise I parse the job ID (allowing a leading '%').
// If that job was stopped, I send SIGCONT. Then I remove it from job_list and wait for it.
// If it stops again (WIFSTOPPED), I re-add it as a stopped job. Otherwise I store its exit code.
static void builtin_fg(char *arg) {
    int jid;
    if (arg == NULL) {
        // no argument → pick most recent job
        if (num_jobs == 0) {
            fprintf(stderr, "fg: no current job\n");
            return;
        }
        jid = job_list[num_jobs - 1].id;
    }
    else {
        if (arg[0] == '%') arg++;
        jid = atoi(arg);
        if (jid == 0) {
            fprintf(stderr, "fg: invalid job ID %s\n", arg);
            return;
        }
    }
    int idx = find_job_by_id(jid);
    if (idx < 0) {
        fprintf(stderr, "fg: no such job %d\n", jid);
        return;
    }
    job_t j = job_list[idx];

    // if it was stopped, resume it
    if (j.state == JOB_STOPPED) {
        if (kill(j.pid, SIGCONT) < 0) {
            perror("fg: kill (SIGCONT)");
            return;
        }
    }

    // remove it from job_list since it's going to run in foreground
    remove_job_by_index(idx);

    // print the command before blocking (like "sleep 20")
    printf("%s\n", j.cmdline);
    fflush(stdout);

    // wait for it in foreground (allow catching Stop or exit)
    fg_pid = j.pid;
    int status;
    if (waitpid(j.pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
    }

    // if it got stopped again, re-add as a stopped job
    if (WIFSTOPPED(status)) {
        if (num_jobs < MAX_JOBS) {
            job_t *newj = &job_list[num_jobs++];
            newj->id    = next_job_id++;
            newj->pid   = j.pid;
            newj->state = JOB_STOPPED;
            strncpy(newj->cmdline, j.cmdline, MAX_CMD_BUFFER - 1);
            newj->cmdline[MAX_CMD_BUFFER - 1] = '\0';
            printf("\r[%d]  Stopped     %s\n", newj->id, newj->cmdline);
            printf("icsh $ ");
            fflush(stdout);
            just_handled_stop = 1;
        } else {
            fprintf(stderr, "icsh: cannot re-add job %d (table is full)\n", j.id);
        }
    }
    else {
        // otherwise record its exit status
        if (WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        } else {
            last_status = 1;
        }
    }

    fg_pid = -1;
}

// Built-in "bg": resume a stopped job in the background.
// If arg is NULL, pick most recent job. Otherwise parse job ID (allow leading '%').
// If the job is already running, I print an error. Otherwise I send SIGCONT,
// set state to JOB_RUNNING, and print "[id] cmdline &".
static void builtin_bg(char *arg) {
    int jid;
    if (arg == NULL) {
        if (num_jobs == 0) {
            fprintf(stderr, "bg: no current job\n");
            return;
        }
        jid = job_list[num_jobs - 1].id;
    }
    else {
        if (arg[0] == '%') arg++;
        jid = atoi(arg);
        if (jid == 0) {
            fprintf(stderr, "bg: invalid job ID %s\n", arg);
            return;
        }
    }
    int idx = find_job_by_id(jid);
    if (idx < 0) {
        fprintf(stderr, "bg: no such job %d\n", jid);
        return;
    }
    job_t *j = &job_list[idx];
    if (j->state == JOB_RUNNING) {
        fprintf(stderr, "bg: job %d already running\n", j->id);
        return;
    }
    // resume it in background
    if (kill(j->pid, SIGCONT) < 0) {
        perror("bg: kill (SIGCONT)");
        return;
    }
    j->state = JOB_RUNNING;
    printf("[%d] %s &\n", j->id, j->cmdline);
    fflush(stdout);
}

// ────────────────────────────────────────────────────────────────────────────
// Milestone 5: combination of built-ins, I/O redirection, history, and external commands
// ────────────────────────────────────────────────────────────────────────────

// SIGINT handler: if I press Ctrl-C and a child is running, I send SIGINT to the child.
// Otherwise I just reprint the prompt.
static void handle_sigint_int(int sig) {
    if (fg_pid > 0) {
        printf("\n");
        kill(fg_pid, SIGINT);
    } else {
        printf("\nicsh $ ");
        fflush(stdout);
    }
}

// SIGTSTP handler: if I press Ctrl-Z and a child is running, I send SIGTSTP to the child.
// Otherwise I just reprint prompt.
static void handle_sigtstp_int(int sig) {
    if (fg_pid > 0) {
        printf("\n");
        kill(fg_pid, SIGTSTP);
    } else {
        printf("\nicsh $ ");
        fflush(stdout);
    }
}

// handleHistory: if I type "!!", I replace buffer with last_cmd (and print it).
// If buffer is anything else non-empty, I copy it into last_cmd.
static int handleHistory(char buffer[], char last_cmd[]) {
    if (strcmp(buffer, "!!") == 0) {
        if (strlen(last_cmd) == 0) {
            // if there was no previous command, I do nothing
            return 0;
        }
        printf("%s\n", last_cmd);
        strcpy(buffer, last_cmd);
    }
    else if (strlen(buffer) > 0) {
        // store this line as last_cmd
        strncpy(last_cmd, buffer, MAX_CMD_BUFFER - 1);
        last_cmd[MAX_CMD_BUFFER - 1] = '\0';
    }
    return 1;
}

// runCmd: this is where I parse a line, handle background (&), built-ins (jobs, fg, bg, echo, exit),
// I/O redirection (< and >), and then fork/exec external commands.
void runCmd(char last_cmd[], char buffer[]) {
    // 1) Check for trailing '&' → background flag
    int background = 0;
    {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '&') {
            background = 1;
            // remove the '&' from buffer
            buffer[len - 1] = '\0';
            // trim spaces before the '&'
            while (len > 1 && buffer[len - 2] == ' ') {
                buffer[len - 2] = '\0';
                len--;
            }
        }
    }

    // 2) Copy the full (possibly-stripped) buffer into jobcmd[] so I remember the original
    //    for background job printing later.
    char jobcmd[MAX_CMD_BUFFER];
    strncpy(jobcmd, buffer, MAX_CMD_BUFFER - 1);
    jobcmd[MAX_CMD_BUFFER - 1] = '\0';

    // 3) Tokenize buffer by spaces, looking for "<" and ">" to set infile/outfile.
    char *tokens[MAX_ARGS];
    char *infile = NULL;
    char *outfile = NULL;
    int   ntok = 0;

    // first token is the command
    char *cmd = strtok(buffer, " ");
    if (cmd == NULL) {
        // blank line or just "&"
        return;
    }
    tokens[ntok++] = cmd;

    // parse the rest of the tokens
    char *t = strtok(NULL, " ");
    while (t != NULL && ntok < MAX_ARGS - 1) {
        if (strcmp(t, "<") == 0) {
            t = strtok(NULL, " ");
            if (t != NULL) {
                infile = t;
            }
        }
        else if (strcmp(t, ">") == 0) {
            t = strtok(NULL, " ");
            if (t != NULL) {
                outfile = t;
            }
        }
        else {
            tokens[ntok++] = t;
        }
        t = strtok(NULL, " ");
    }
    tokens[ntok] = NULL;

    // 4) Check Milestone 6 built-ins: jobs, fg, bg
    if (strcmp(cmd, "jobs") == 0) {
        builtin_jobs();
        return;
    }
    if (strcmp(cmd, "fg") == 0) {
        if (tokens[1] != NULL) builtin_fg(tokens[1]);
        else                  builtin_fg(NULL);
        return;
    }
    if (strcmp(cmd, "bg") == 0) {
        if (tokens[1] != NULL) builtin_bg(tokens[1]);
        else                  builtin_bg(NULL);
        return;
    }

    // 5) "echo" and "exit" are built-ins too, but they run in the parent with I/O redirection handled here.
    int is_echo = (strcmp(cmd, "echo") == 0);
    int is_exit = (strcmp(cmd, "exit") == 0);

    if (is_echo || is_exit) {
        int saved_in  = -1;
        int saved_out = -1;

        // if they requested input redirection, I dup/redirect stdin
        if (infile != NULL) {
            saved_in = dup(STDIN_FILENO);
            int fd = open(infile, O_RDONLY);
            if (fd < 0) {
                perror("open input");
                goto restore_fds;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        // if they requested output redirection, I dup/redirect stdout
        if (outfile != NULL) {
            saved_out = dup(STDOUT_FILENO);
            int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open output");
                goto restore_fds;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (is_echo) {
            // handle "echo $?" or just echo arguments
            char **args = tokens + 1;
            if (args[0] && strcmp(args[0], "$?") == 0) {
                printf("%d\n", last_status);
            } else {
                int i = 0;
                while (args[i]) {
                    printf("%s", args[i]);
                    if (args[i+1]) printf(" ");
                    i++;
                }
                printf("\n");
            }
        }
        else {  // exit built-in
            int code = 0;
            if (tokens[1]) {
                code = atoi(tokens[1]) & 0xFF;
            }
            printf("bye\n");
            exit(code);
        }

    restore_fds:
        // restore stdout if I changed it
        if (saved_out != -1) {
            dup2(saved_out, STDOUT_FILENO);
            close(saved_out);
        }
        // restore stdin if I changed it
        if (saved_in != -1) {
            dup2(saved_in, STDIN_FILENO);
            close(saved_in);
        }
        return;
    }

    // 6) Otherwise, I must run an external command: I fork, handle redirection in the child, then execvp.
    pid_t pid = fork();
    if (pid < 0) {
        perror("failed to fork");
        last_status = 1;
        return;
    }
    if (pid == 0) {
        // child process: apply infile/outfile if requested
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
        // now execute the command
        execvp(tokens[0], tokens);
        perror("failed to execute");
        exit(1);
    }
    else {
        // parent process:
        if (background) {
            // if user said "&", I add it to my job list and return without waiting
            add_job(pid, jobcmd);
            return;
        }

        // otherwise this is a foreground command: wait for it, but allow it to stop (WUNTRACED)
        fg_pid = pid;
        int status;
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            perror("waitpid");
        }
        fg_pid = -1;

        // if it got stopped by Ctrl-Z, re-add as a stopped job
        if (WIFSTOPPED(status)) {
            if (num_jobs < MAX_JOBS) {
                job_t *newj = &job_list[num_jobs++];
                newj->id    = next_job_id++;
                newj->pid   = pid;
                newj->state = JOB_STOPPED;
                strncpy(newj->cmdline, jobcmd, MAX_CMD_BUFFER - 1);
                newj->cmdline[MAX_CMD_BUFFER - 1] = '\0';

                // overwrite prompt, print "[id]  Stopped  cmdline", then reprint prompt
                printf("\r[%d]  Stopped     %s\n", newj->id, newj->cmdline);
                printf("icsh $ ");
                fflush(stdout);

                just_handled_stop = 1;
            } else {
                fprintf(stderr, "icsh: cannot add stopped job (max %d)\n", MAX_JOBS);
            }
        }
        else {
            // if it exited normally or by signal, record its exit code
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else {
                last_status = 1;
            }
        }
    }
}

// I always call this to print my prompt "icsh $ ".
static void prompt() {
    printf("icsh $ ");
    fflush(stdout);
}

// interactiveMode: I print "Starting IC shell", then loop reading lines.
// I handle "!!" history, then call runCmd() on each line.
void interactiveMode() {
    char buffer[MAX_CMD_BUFFER];
    char last_cmd[MAX_CMD_BUFFER] = "";

    printf("Starting IC shell\n");
    while (1) {
        // if runCmd already printed a prompt after a stop, skip printing
        if (just_handled_stop) {
            just_handled_stop = 0;
        } else {
            prompt();
        }

        if (fgets(buffer, MAX_CMD_BUFFER, stdin) == NULL) {
            break;  // EOF (Ctrl-D), exit loop
        }
        buffer[strcspn(buffer, "\n")] = '\0';

        if (!handleHistory(buffer, last_cmd)) {
            continue;
        }
        runCmd(last_cmd, buffer);
    }
}

// scriptMode: similar to interactive, but I read commands from a file instead of stdin.
void scriptMode(const char *filename) {
    char buffer[MAX_CMD_BUFFER];
    char last_cmd[MAX_CMD_BUFFER] = "";
    FILE *input = fopen(filename, "r");
    if (!input) {
        printf("Could not open the file.\n");
        return;
    }
    while (fgets(buffer, MAX_CMD_BUFFER, input) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        if (!handleHistory(buffer, last_cmd)) continue;
        runCmd(last_cmd, buffer);
    }
    fclose(input);
}

int main(int argc, char *argv[]) {
    // I install the SIGINT and SIGTSTP handlers so my shell isn't killed or suspended by Ctrl-C/Z.
    signal(SIGINT,  handle_sigint_int);
    signal(SIGTSTP, handle_sigtstp_int);

    // I install the SIGCHLD handler so I can track background job changes.
    struct sigaction sa_chld;
    sa_chld.sa_handler = handle_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    // If there’s a script file argument, run in script mode; otherwise interactive.
    if (argc == 2) {
        scriptMode(argv[1]);
    } else {
        interactiveMode();
    }
    return 0;
}
