/* ICCS227: Project 1: icsh
 * Name: Ratchapong Tiebsornchai
 * StudentID: 6380164
 */

#include "stdio.h"
#include <string.h>
#include <stdlib.h>

#define MAX_CMD_BUFFER 255

int main() {
    char buffer[MAX_CMD_BUFFER]; // for identifying command type
    char last_cmd[MAX_CMD_BUFFER] = "";  // for "!!", initialized, to set to "\0" to avoid printing garbage

    printf("Starting IC shell\n");

    while (1) { // while 1(true) so always entering this while loop.
        printf("icsh $ ");
	// keyboard input:
        fgets(buffer, 255, stdin); // store the input in buffer
	buffer[strcspn(buffer, "\n")] = '\0'; // removes \n(from pressing enter) and replace with \0
        // printf("you said: %s\n", buffer);

	// "!!" prints the last command then runs it, if no last command just give back prompt
	// handle "!!" first, to handle the last command
	// if not we end up checking strtok("!!", " ");
	// and it will not match any command, since its not a real command
	// (it is used to call the previous command)
	if (strcmp(buffer, "!!") == 0) {
		if (strlen(last_cmd) == 0) {
			continue; // no last_cmd, so it returns back to the shell prompt
		}
		printf("%s\n",last_cmd); // show the last_cmd
		strcpy(buffer,last_cmd); // copy last command to buffer(overwritten)
	}
	// ***Buffer is used for command execution, all my command runs based
	// on what is currently in the buffer.
	// SO NOW the the thing in the BUFFER will get run
	else if (strlen(buffer) > 0) {
		strcpy(last_cmd, buffer); // copy buffer(input) into last_cmd
					// last_cmd gets updated every iteration(that is not !!)
				// so next time "!!" is called the last_cmd can be printed and executed
	}
	// ***We start checking the type of command here
	char *cmd = strtok(buffer, " "); // cmd points to the start of the string inside buffer
	if (cmd == NULL) { // no input from user(maybe user just press ENTER)
		continue;  // "\n\0", to prevent from calling strcmp on NULL, and get segment fault
	}		// and just continue since there is no INPUT

	// echo command:(first word in buffer matches the "echo" command
	if (strcmp(cmd, "echo") == 0) {
		char *next = strtok(NULL, " ");
		while (next != NULL) {
			printf("%s ", next);
			next = strtok(NULL, " ");
		}
		printf("\n"); // continue to move to next iteration immediately, or else it will get into the else bad command.
		continue;                                  // == 0 means string are equal.
	// since it doesnt match with exit so it goes to else.							 // strtok replaces the first " " with \0
					 // in the buffer , to seperate them into single
					 // string each and points to the first word in the buffer
					 // call again with NULL, to continue scanning from where we left off after
					 // \0 of previous string, and so on.
        }

	// exit command: this command exits the shell with a given exit code
	if (strcmp(cmd, "exit") == 0) {
		char *next = strtok(NULL, " ");
		int exit_code = 0; // exit code 0 means "im done" just exit
				   // success (normal exit)
		if (next != NULL) {
			exit_code = atoi(next); //string to integer
			exit_code = exit_code % 256;
			// Linux only allows exit codes from 0 to 255 (8 bit limit)
			// we do error_code % 256, to wrap 256(max number)
			// back to 0(wraps to 0), 256 % 256 = 0
			// 257 % 256 = 1
		}

		printf("bye\n");
		exit(exit_code); // exits the main() function
		//return error_code;
		// only have to return with a legal exit code (not beyond 255)

	}

	else {
		printf("bad command\n");

	}
}

}
