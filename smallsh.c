/******************************************************************************
 * smallsh.c
 *	Name: 		Cramer Smith
 * 	Date: 		November 22, 2012
 *  Class:      CS 344 Operating Systems
 *	Decription: This is a miniture shell that allows foreground and background
 *				processes. 
 ******************************************************************************/
#include <stdio.h>				// 1
#include <string.h>				// 2
#include <sys/types.h> 			// 3
#include <sys/wait.h>			// 4
#include <unistd.h> 			// 5
#include <stdlib.h>				// 6
#include "dynamicArray.h"		// 7
#include <fcntl.h>				// 8
//#include <bits/sigaction.h>		// 9
#include <signal.h>				// 10
#include <errno.h>				// 11

// just messing around with color stuff.
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

void nrm() { printf("%s", KNRM); }
void red() { printf("%s", KRED); }
void grn() { printf("%s", KGRN); }
void yel() { printf("%s", KYEL); }
void blu() { printf("%s", KBLU); }
void mag() { printf("%s", KMAG); }
void cyn() { printf("%s", KCYN); }
void wht() { printf("%s", KWHT); }
// Done messing around with color stuff

// needed to catch the signal
void catchint(int signo) {
	printf("Caught an interrupt: %d\n", signo);
}

void print_stack(DynArr* stack) {
	printf("size: %d\n", sizeDynArr(stack) );
	int i;
	for (i = 0; i < sizeDynArr(stack); i++) {
		if (i != sizeDynArr(stack)) {
			printf("[%d], ", getDynArr(stack, i) );
		} else {
			printf("[%d]\n", getDynArr(stack, i) );
		}
	}
}


/******************************************************************************
 * sstrcmp( char*, char* )
 *  This function takes in two Strings and returns 1 if they are the same and 0
 *  if the are not the same this is how strcmp should work.
 *  NEEDS: <string.h>
 ******************************************************************************/
int sstrcmp(char* one, char* two) {
	return (strcmp(one, two) == 0);
}

/******************************************************************************
 * change_directory( char* )
 *  This function takes in a file directory in the form of a string and changes
 *  the current working directory to that directory. If the given directory is
 *  NULL then the directory is not changed. If the given directory does not
 *  exsist then an error message is printed out.
 *  It also returns an integer, 0 if successful, 1 if it encountered an error
 ******************************************************************************/
int change_directory(char *new_directory) {
	if(new_directory == NULL) {						// if given no argument for cd
		char cwd[1024];
		new_directory = getcwd(cwd, sizeof(cwd));	
	}
	int ret = chdir(new_directory);					
	if (ret == -1) {
		printf("-smallsh: cd: %s: No such file or directory\n", new_directory);		// error message
		ret = 1;									// set ret to be the standard fail status
	} 
	return ret;
}

/******************************************************************************
 * get_last_status( int )
 *  This function flushed the output buffer and then prints out the given int
 ******************************************************************************/
void get_the_last_status(int status) {
	fflush(stdout);
	printf("%d\n", status);
}

/******************************************************************************
 * exit_the_loop( int* )
 *  This sets the given int pointer to zero
 ******************************************************************************/
void exit_the_loop(int* playing) {
	*playing = 0;
}

// well I made this so I am keeping it.
/******************************************************************************
 * built_in_commands( char*, char**, int, int, int, char*, int, DynArr* )
 *  This is the function that takes the parsed command and then forks, and 
 *  execs it.
 ******************************************************************************/
int built_in_commands(char* command, char** arguments_full, int number_of_arguments, int file_redirect, char *file_name, int background, DynArr* prosses_stack) {
	char *arguments[number_of_arguments + 2];		// we want to recopy the array of arguments so that it is not to long for exec
	int w;
	for (w = 0; w < number_of_arguments; w++ ) {
		arguments[w] = arguments_full[w];			// copying only the important parts
	}
	arguments[number_of_arguments] = NULL;			// setting the end of the array to NULL 
	pid_t pid = fork();								// THE FORK!
	if (pid == -1) {
		printf("SOMETHING IS WRONG WITH FORK\n");
		_exit(2); 									// HARD EXIT
	} else if (pid == 0) {
		int fd;										// Child
		int fd2;
		if(file_redirect == 1 || background) {		// either you are directing it to a file or to /dev/null
			fd = open(file_name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (fd == -1) {
				printf("-smallsh: %s: No such file or directory\n", file_name);  // error message for weird files but this shouldn't happen... it would just create it
				exit(1); 
			}
			fd2 = dup2(fd, 1); 
			if (fd2 == -1) {
				perror("dup2");
				exit(2); 
			}
		}
		if (file_redirect == 2) {
			fd = open(file_name, O_RDONLY); 
			if (fd == -1) {
				printf("-smallsh: %s: No such file or directory\n", file_name); // error message for weird files
				exit(1);
			}
			fd2 = dup2(fd, 0);
			if (fd2 == -1) {
				perror("dup2");
				exit(2); 
			}
		}
		fflush(stdout);								
		if (execvp(command, arguments) < 0) {
			printf("-smallsh: %s: command not found\n", command); // error message for bad commands
		}
		exit(0);
	} else {
		int status;									// Parent
		if (!background) {	
			waitpid(pid, &status, 0);				// wait for the process to fini9sh if it is not in the background
		} else {
			pushDynArr(prosses_stack, pid);			// if it is in the background push it to the process stack
		}
		if (status != 0) {
			return 1;								// if the exec status was funky return a 1 
		}
		return status;
	}
	
}


/******************************************************************************
 * comand_sorter( char*, int, int*, int, DynArr* )
 *  This is the function that takes the command and parses it into an array
 ******************************************************************************/
int command_sorter(char* command, int *playing, int status, DynArr* prosses_stack) {
	char* arguments[256];
	int cleaner;									// clean the array setting them all to NULL 
	for (cleaner = 0; cleaner < 256; cleaner++) {
		arguments[cleaner] = NULL;					// clearing out all the garabage
	}

	char* token;									
	token = strtok(command, " ");					// make the first token
	arguments[0] = token; 							// set this to be the first argument
	int argument_count = 1;							// argument count is at one
	int file_redirect = 0;							// 1 == >, 2 == < // this will choose the second one if the user does <> or something dumb like that... stupid user
	int background = 0;								// so far it is not in the background 
	char file_name[50] = "stdout";					// initial file output is to standard out
	
	// this loop parses the command string
	while (token != NULL) {							
		token = strtok(NULL, " ");
		if (token != NULL) {						// if there is still something to tokenize
			if (token[0] == '#') {					// the rest of the command is a comment
				break;								// We don't care about the comment so forget about the rest of the command
			} else if (token[0] == '>') {			
				file_redirect = 1;					// file redirect
				token = strtok(NULL, " ");
				if (token != NULL) {
					strcpy(file_name, token);		// get the next token is the file
				}
				break;								// there shouldnt be more arguments...
			} else if (token[0] == '<') {
				file_redirect = 2;
				token = strtok(NULL, " ");			
				if (token != NULL) {				// same here as above
					strcpy(file_name, token);
				}
				break;
			} else if (token[0] == '&') { 			// in the background
				background = 1;
			} else {
				arguments[argument_count] = token;	// add the argument to the array
				argument_count++;
			}
		}
	}
	if (background && file_redirect != 1) {			// if it was in the background but not set to a different file
		strcpy(file_name, "/dev/null");
		file_redirect = 1;
	}

	// The command string is turned into a array of arguments and the other information of the given command is interpretted
	// now looking at the first command should allow the program to know what to do.
	if (sstrcmp(arguments[0], "exit")) {			// exit smallsh
		exit_the_loop(playing);						
	} else if (sstrcmp(arguments[0], "cd")) {		// change directory
		status = change_directory(arguments[1]);
	} else if (sstrcmp(arguments[0], "status")) {	// get the last status
		get_the_last_status(status);
	} else {										// anything else
		status = built_in_commands(arguments[0], arguments, argument_count, file_redirect, file_name, background, prosses_stack);	
	}
	return status;
}


/******************************************************************************
 * MAIN
 *  The main just runs the while loop to get the user input, it also keeps
 *  track of the process stack.
 ******************************************************************************/
int main(int argc, char* argv[]) {

	// setting up the signal catching struct
	struct sigaction act; 
	act.sa_handler = catchint; 
	act.sa_flags = 0; 
	sigfillset(&(act.sa_mask));
	sigaction(SIGINT, &act, NULL);

	DynArr *dyn;									// stole this from my CS 261 days
	dyn = createDynArr(256);						// it's dynamic so it will grow if it needs to
	int playing = 1;
	int last_status = 0;							// the last status of the program
	int background_status;
   
    printf("poop\n");
	do {
		//print_stack(dyn);											// take in commands while command is not exit.
		int checking_processes = 1;								
		if(!isEmptyDynArr(dyn)) {
			while(checking_processes) { // look at all the background processes if they are still running ok
				if ((waitpid(topDynArr(dyn), &background_status, WNOHANG)) != 0) {
					//print_stack(dyn);
					printf("[%d] - status: %d\n", topDynArr(dyn), background_status);	// if they have stopped print the pid and the status they stopped with
					popDynArr(dyn);				//if that process has finished remove it from the array
				} 
				checking_processes = 0;
			}
		}
		char command[2050];
		grn();										// I did this for fun! it makes the colons green 
		printf(": ");
		nrm();										// the colon at the begginning of each command
		fgets(command, 2048, stdin);
		command[strlen(command) - 1] = '\0';		// take away the new line character from the users input
		last_status = command_sorter(command, &playing, last_status, dyn);
	} while (playing);
    
	nrm();
	deleteDynArr(dyn);
	return 0;
}
