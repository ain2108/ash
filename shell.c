#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/stat.h>

#include "shell.h"

#define SUCCESS 0 
#define FAILURE -1
#define ON_MAC 0

/* Few initial values */
#define COMMON_SIZE 128
#define HISTORY_SIZE 100

#define DEBUG 0

/* To not get confused */
#define PIPE_READ 0
#define PIPE_WRITE 1

/* Few more states that the program can take */
#define NO_PIPE 0
#define IN_FROM_PIPE 1
#define OUT_TO_PIPE 2
#define IN_OUT_PIPE 3

/* The two possible orientations of pipes relative to a process */
#define FROM1TO2 0
#define FROM2TO1 1

/* Error handling */
#define PIPE_OPEN_ERROR 1
#define PIPE_CLOSE_ERROR 2
#define CHDIR_ERROR 3
#define FORK_ERROR 4
#define DUP_ERROR 5


#define EXIT 1

/* Array of implemented builtins */
const char * BUILTINS[] = {"cd", "history", "exit"};
#define BUILTINS_NUM (sizeof(BUILTINS) / sizeof(const char *))

/* History setup: need an arryay and a pointer to last command */
char * history[100];
int last_cmd_pos = 0;

/* Pipe setup: we need two pipes to support mutiple pipes. 
   We also need a way to track the orientation of these pipes in relation
   to the process */
int pipe1[2];
int pipe2[2];
int ORIENTATION = 0;

/* Due to bad design, we also need few globals to keep track of our state */
int PIPED = 0;
int PIPELINE = 0;

/* Toggle this variable to switch $PATH lookup on/off */
int PATH_LOOKUP = 0;
/* Toggle this to log cmd = history [offset] to history */
int FROM_HISTORY_EXECUTION = 0;

int main(){

	reset_history();
	loop();
	free_history();

	/* On Mac i have to explicitly close stdin/stdout 
	to prevent valgrind complains */ 
	if(ON_MAC){
		fclose(stdout);
		fclose(stdin);
		fclose(stderr);
	}
	return SUCCESS; 
}

void loop(){

	while(1){
		fprintf(stdout, "$");

		/* Read a line from stdin. Line is heap allocated */
		char * line_heap = get_big_line(stdin);
		if(line_heap == NULL) die("malloc() failed");

		/* To free the heap allocated buffer immediately */
		char line_stack[strlen(line_heap) + 1];
		memset(line_stack, 0, strlen(line_heap) + 1);
		strcpy(line_stack, line_heap);
		free(line_heap);

		if( execute_line(line_stack) == EXIT) break;
	}
}

/* Function returns a pointer to a heap allocated null terminated array.
   This array contains the characters of the line of file fp.
   Function returns NULL if malloc error occured. */
char * get_big_line(FILE * fp){
	
	int actual_size = 0;
	int buf_size = COMMON_SIZE;

	char * buf = malloc(buf_size * sizeof *buf);
	if(buf == NULL) return NULL; 

	char c = fgetc(fp);
	while( c != EOF && c != '\n'){
		
		/* Reallocate the buffer and expand it in case it is full */
		if(actual_size > buf_size - 5){
			buf_size *= 2;
			buf = realloc(buf, buf_size);
			if(buf == NULL) return NULL;
		}
		buf[actual_size++] = c;
		c = fgetc(fp);
	}

	buf[actual_size] = '\0';
	df("get_big_line");
	return buf;
}

/* Tokenize and build an array of pointers */
void get_argv(char * line, char ** argv){
	char * arg = strtok(line, " ");
	int i = 0;
	while(arg != NULL){
		argv[i++] = arg;
		arg = strtok(NULL, " "); 
	}
	df("get_argv");
}

/* Function get the number of arguments/tokens in a string */
int get_argc(char * line, char delim){
	
	int count = 0;
	int i = 0;

	/* Empty prompt case */
	if(strlen(line) == 0) return 0;

	/* Ignore the trailing spaces on the front */
	while(line[i] == delim) i++;

	if(i >= strlen(line) - 1) return 0;  

	/* Calculate the number of tokens separated by whitespace */
	for(i = i; i < strlen(line) - 1; i++){
		if(line[i] == delim && line[i + 1] != delim) count++;
	}

	return ++count; 
}

void print_argv(char ** argv, int argc){
	int i = 0;
	while(i < argc){
		fprintf(stdout, "%s ", argv[i]);
		i++;
	}
	fprintf(stdout, "\n");
}

/* This functions will interpret argv[][] of the cmd and 
   perform the desired action */
int interpret(char ** argv, int argc){

	int number_of_pipes = count_pipes(argv, argc);

	if(number_of_pipes > 0){

		if(argc <= 2){
			fprintf(stderr, "error: %s\n", "bad pipeline structure");
			return SUCCESS;
		}

		/* Put the flag up the the program is in PIPELINE state
		   This will cause certain changes to the sideeffects of the
		   functions. Like for example history will be disabled for 
		   individual commands in the pipeline */
		PIPELINE = 1;

		/* We want to be able to separate between different commands */ 
		int pipe_pos[number_of_pipes];
		get_pipe_positions(argv, argc, pipe_pos);
		if(exist_consecutive_pipes(pipe_pos, number_of_pipes)){
			fprintf(stderr, "error: %s\n", "bad pipeline structure");
			PIPELINE = 0;
			return SUCCESS;
		}
		

		if(DEBUG){
			int i;
			for(i = 0; i < number_of_pipes; i++){
				fprintf(stderr, "pipe at %d\n", pipe_pos[i]);
			}
			fprintf(stderr, "%d pipe(s)\n", number_of_pipes);
		}

		/* We need to open the pipes since at least one of them is goind to be used */
		if(open_pipes() == FAILURE){ return handle_error(PIPE_OPEN_ERROR); }

		/* We need to retrieve the first command in the pipeline */ 
		char * pipe_argv[number_of_pipes + 1];
		int new_argc = pipe_pos[0];
		pipe_argv[0] = argv_to_line(argv, new_argc);

		/* All intermediate commands in the pipeline */
		int i;
		for(i = 1; i < number_of_pipes; i++){
			new_argc = pipe_pos[i] - pipe_pos[i - 1];
			pipe_argv[i] = argv_to_line(argv + pipe_pos[i - 1] + 1, new_argc - 1);
		}

		/* Last command in the pipeline */
		new_argc = argc - pipe_pos[i - 1];
		pipe_argv[i] = argv_to_line(argv + pipe_pos[i - 1] + 1, new_argc - 1);

		if(pipeline_with_builtin(pipe_argv, number_of_pipes + 1)){
			/* Cleanup and reset of state */
			if(close_pipes() == FAILURE) return handle_error(PIPE_CLOSE_ERROR);
			free_argv(pipe_argv, number_of_pipes + 1);
			PIPED = PIPELINE = ORIENTATION = 0;
			fprintf(stderr, "error: %s\n", "builtins not supported in pipes");
			return SUCCESS;
		}

		/* In case of single pipe, it is simple */
		if(number_of_pipes == 1){
			PIPED = OUT_TO_PIPE;
			execute_line(pipe_argv[0]);

			PIPED = IN_FROM_PIPE;
			execute_line(pipe_argv[1]);

		}else{

			/* First command executed */
			PIPED = OUT_TO_PIPE;
			execute_line(pipe_argv[0]);

			/* Initialize the orientaion and execute the second command */
			ORIENTATION = FROM1TO2;
			PIPED = IN_OUT_PIPE;
			execute_line(pipe_argv[1]);

			/* If there are more than two pipes, execute all the commands before
			   the last one. */
			int i;
			for(i = 2; i < number_of_pipes; i++){
				/* Change the orientation of pipes */
				ORIENTATION = (ORIENTATION + 1) % 2;
				
				/* Close the old-not-in-use pipe and open a new one */
				recycle_pipes(ORIENTATION);
				PIPED = IN_OUT_PIPE;
				execute_line(pipe_argv[i]);
			}

			/* Execute the last command */
			ORIENTATION = (ORIENTATION + 1) % 2;
			PIPED = IN_FROM_PIPE;
			execute_line(pipe_argv[number_of_pipes]);	
		}
		
		/* Cleanup and reset of state */
		if(close_pipes() == FAILURE) return handle_error(PIPE_CLOSE_ERROR);
		while( wait(NULL) != -1);
		free_argv(pipe_argv, number_of_pipes + 1);
		PIPED = PIPELINE = ORIENTATION = 0;

	}else if(!strcmp(argv[0], "exit")){
		return EXIT;
	
	}else if(!strcmp(argv[0], "history")){
		if(argc == 1){
			print_history();
		}else if(argc == 2 && !strcmp(argv[1], "-c") ){
			free_history();
			reset_history();
		}else if(argc == 2){
			/* We need to be careful with the argument to strtol */
			char * number_end;
			char * argv2_end = argv[1] + strlen(argv[1]);
			int offset = (int) strtol(argv[1], &number_end, 10);
			if(number_end != argv2_end){
				fprintf(stderr, "error: %s\n", "built in not supported");
				return SUCCESS;
			}

			if(offset >= HISTORY_SIZE || offset < 0){
				fprintf(stderr, "error: %s\n", "bad offset in history");
				return SUCCESS;
			}

			/* Execute the command from history */
			FROM_HISTORY_EXECUTION = 1;
			char * cmd = cmd_from_history(offset);
			if(cmd == NULL){
				fprintf(stderr, "error: %s\n", "bad offset in history");
				return SUCCESS;
			}

			char temp[strlen(cmd) + 1];
			memset(temp, 0, strlen(cmd) + 1);
			strcpy(temp,cmd);
			execute_line(temp);
			FROM_HISTORY_EXECUTION = 0;
		}else{
			fprintf(stderr, "error: %s\n", "built in not supported");
		}
	}else if(!strcmp(argv[0], "cd") && argc == 2){
		if(chdir(argv[1]) == -1) return handle_error(CHDIR_ERROR);
	}else{
		execute(argv);
		if(!PIPELINE){while( wait(NULL) != -1);}
	}

	df("interpret");
	return SUCCESS;
}

/* Function executes the the binary */
void execute(char ** argv){
	
	pid_t child; 
	if((child = fork()) < 0) handle_error(FORK_ERROR);
	if(child == 0){
		
		switch(PIPED){
			case IN_FROM_PIPE:
				/* If the program before last one wrote to pipe1 */
				if(ORIENTATION == FROM1TO2){
					if(dup2(pipe1[PIPE_READ], 0) 
						== FAILURE) handle_error(DUP_ERROR); 
					if(close(pipe1[PIPE_WRITE]) |
					   close(pipe2[PIPE_READ])  |
					   close(pipe2[PIPE_WRITE]) == FAILURE) 
					handle_error(PIPE_CLOSE_ERROR);

				/* If the program before last one wrote to pipe2 */
				}else if(ORIENTATION == FROM2TO1){
					if(dup2(pipe2[PIPE_READ], 0) 
						== FAILURE) handle_error(DUP_ERROR); 
					if(close(pipe2[PIPE_WRITE]) |
					   close(pipe1[PIPE_READ])  |
					   close(pipe1[PIPE_WRITE]) == FAILURE) 
					handle_error(PIPE_CLOSE_ERROR);
				}
				break;
			case OUT_TO_PIPE:
				/* First program will always write to the first pipe */
				if(dup2(pipe1[PIPE_WRITE], 1) 
					== FAILURE) handle_error(DUP_ERROR);
				if(close(pipe1[PIPE_READ]) |
				   close(pipe2[PIPE_READ]) |
				   close(pipe2[PIPE_WRITE]) == FAILURE) 
					handle_error(PIPE_CLOSE_ERROR);
				break;
			case IN_OUT_PIPE:
				if(ORIENTATION == FROM1TO2){
					if(dup2(pipe1[PIPE_READ], 0) 
						== FAILURE) handle_error(DUP_ERROR);
					if(close(pipe1[PIPE_WRITE]) == FAILURE)
						handle_error(PIPE_CLOSE_ERROR);
					
					if(dup2(pipe2[PIPE_WRITE], 1) 
						== FAILURE) handle_error(DUP_ERROR);
					if(close(pipe2[PIPE_READ]) == FAILURE)
						handle_error(PIPE_CLOSE_ERROR);
				}else if(ORIENTATION == FROM2TO1){
					if(dup2(pipe2[PIPE_READ], 0) 
						== FAILURE) handle_error(DUP_ERROR);
					if(close(pipe2[PIPE_WRITE]) == FAILURE)
						handle_error(PIPE_CLOSE_ERROR);
					if(dup2(pipe1[PIPE_WRITE], 1) 
						== FAILURE) handle_error(DUP_ERROR);
					if(close(pipe1[PIPE_READ]) == FAILURE)
						handle_error(PIPE_CLOSE_ERROR);
				}
				break;
			default:;
		}
		if(DEBUG) fprintf(stderr, "executing %s\n", argv[0]);
		if(PATH_LOOKUP){
			execvp(argv[0], argv);
		}else if(!PATH_LOOKUP){
			execv(argv[0], argv);
		}

		/*int argc;
		for(argc = 0; *(argv + argc) != NULL; argc++);
		fprintf(stderr, "argc %d\n", argc);
		free_argv(argv, argc - 1);*/
		die_errno();
	}
}

/* Error reporting functions */
void die(char * error_message){
	fprintf(stderr, "error: %s\n", error_message);
	if(ON_MAC){
		fclose(stdout);
		fclose(stdin);
		fclose(stderr);
	}
	close(0);
	close(1);
	free_history();
	exit(FAILURE);
}

void die_errno(){
	fprintf(stderr, "error: %s\n", strerror(errno));
	if(ON_MAC){
		fclose(stdout);
		fclose(stdin);
		fclose(stderr);
	}
	close(0);
	close(1);
	free_history();
	exit(FAILURE);
}

/* Histoty helper functions */ 
void reset_history(){
	memset((char *) history, 0, HISTORY_SIZE * sizeof(char *) );
	last_cmd_pos = 0;
}

void free_history(){
	int i;
	for(i = 0; i < HISTORY_SIZE; i++){
		if(history[i] != 0) free(history[i]);		
	}
}

int add_to_history(char * cmd){

	/* Allocate the command on the heap for persistance */	
	char * cmd_heap = malloc((strlen(cmd) + 1) * sizeof(char));
	memset(cmd_heap, 0, (strlen(cmd) + 1) * sizeof(char) );
	if(cmd_heap == NULL) die("malloc() failed");
	strcpy(cmd_heap, cmd);
	cmd_heap[strlen(cmd)] = '\0';
	
	/* In case of an overwrite, don't forget to free() previous */
	if(history[last_cmd_pos] != 0){
		free(history[last_cmd_pos]);
	}

	history[last_cmd_pos] = cmd_heap;
	last_cmd_pos = (last_cmd_pos + 1) % HISTORY_SIZE;
	df("add_to_history");
	return 0;
}

char * cmd_from_history(int offset){

	if(offset < 0 || offset >= HISTORY_SIZE) return NULL;

	int temp = last_cmd_pos;
	while(history[temp] == 0){
		temp++;
		temp %= HISTORY_SIZE; 
	}

	temp += offset;
	temp %= HISTORY_SIZE; 

	if(history[temp] == 0){
		return NULL;
	}

	return history[temp];
}

void print_history(){

	/* Temp is pointing at the oldest command or to 0 */
	int temp = last_cmd_pos;
	int i;
	int cmd_num = 0;
	for(i = 0; i < HISTORY_SIZE; i++){
		
		/* Wrap around if went over the edge of the array */
		if(temp == -1) temp = HISTORY_SIZE - 1; 
		
		/* Handle the case when temp os pointing to 0 */ 
		if(history[temp] == 0){
			temp = (temp + 1) % HISTORY_SIZE;
		 	continue;
		}

		fprintf(stdout, "%d %s\n", cmd_num++, history[temp++]);
		temp %= HISTORY_SIZE; 
	}
}

int execute_line(char * line){

	if(line == NULL){
		fprintf(stderr, "error: %s\n", "invalid offset");
		return 0;
	}

	char hist_line[strlen(line) + 1];
	memset(hist_line, 0, strlen(line) + 1);
	strcpy(hist_line, line);

	char * formatted_line = format_cmd(line);

	/* Get the shell arguments into usual argv form */
	int argc = get_argc(formatted_line, ' ');		
	if(argc == 0) return SUCCESS;
	if(DEBUG) fprintf(stderr, "argc = %d\n", argc);

	char form_stack_line[strlen(formatted_line) + 1];
	memset(form_stack_line, 0, strlen(formatted_line) + 1);
	strcpy(form_stack_line, formatted_line);
	free(formatted_line);

	char * argv[argc + 1];
	get_argv(form_stack_line, argv);
	argv[argc] = 0; 

	/* Add the command to history or NOT */
	if(!((argc == 2 || argc == 1) && (!strcmp(argv[0], "history"))) 
		&& !PIPELINE 
		&& !FROM_HISTORY_EXECUTION){
		add_to_history(hist_line);
	}


	if(interpret(argv, argc) == EXIT){ 
		return EXIT;
	}
	return SUCCESS;
}

/* Function counts the number of pipes */
int count_pipes(char ** argv, int argc){

	int i;
	int counter = 0;
	for(i = 0; i < argc; i++){
		if(!strcmp("|", argv[i])) counter++; 
	}

	df("count_pipes");
	return counter;
}

void get_pipe_positions(char ** argv, int argc, int * positions){

	int i;
	int counter = 0;
	for(i = 0; i < argc; i++){
		if(!(strcmp("|", argv[i]))) positions[counter++] = i;
	}

	return;
}

void df(char * fname){ if(DEBUG) fprintf(stderr, "%s: OK\n", fname); }

int close_pipes(){
	if(close(pipe1[PIPE_READ]) == FAILURE 
	|| close(pipe2[PIPE_READ]) == FAILURE
	|| close(pipe1[PIPE_WRITE]) == FAILURE
	|| close(pipe2[PIPE_WRITE]) == FAILURE){ return FAILURE; }
	return SUCCESS;
}

int open_pipes(){
	if(pipe(pipe1) == -1 || pipe(pipe2) == -1) return FAILURE;
	return SUCCESS;
}

/* Closes the appropriate pipe based on the current process ORIENTATION,
   and opens a new pipe */
int recycle_pipes(int ORIENTATION){
	if(ORIENTATION == FROM2TO1){
		if(close(pipe1[PIPE_READ]) == FAILURE || close(pipe1[PIPE_WRITE]) == FAILURE)	
			return PIPE_CLOSE_ERROR;
		if(pipe(pipe1) == FAILURE)
			return PIPE_OPEN_ERROR;

	}else if(ORIENTATION == FROM1TO2){
		if(close(pipe2[PIPE_READ]) == FAILURE || close(pipe2[PIPE_WRITE]) == FAILURE)
			return PIPE_CLOSE_ERROR;
		if(pipe(pipe2) == FAILURE)
			return PIPE_OPEN_ERROR;
	}
	return SUCCESS;
}

int get_argv_length(char ** argv, int argc){
	int i;
	int counter = 0;
	for(i = 0; i < argc; i++){
		counter += strlen(argv[i]);
	}
	return counter;
}

/* Function translates char ** into a char * */
char * argv_to_line(char ** argv, int argc){
	int i, j;
	int total = 0;

	int length = get_argv_length(argv, argc);
	char * buffer = malloc(2 * length * sizeof(char));
	if(buffer == NULL) die("malloc failed");
	memset(buffer, 0, 2 * length * sizeof(char));
	
	for(i = 0; i < argc; i++){
		for(j = 0; j < strlen(argv[i]); j++){
			buffer[total++] = argv[i][j]; 
		}
		buffer[total++] = ' ';
	}
	buffer[total] = '\0';
	return buffer;
}

void free_argv(char ** argv, int argc){
	int i;
	for(i = 0; i < argc; i++){
		free(argv[i]);
	}
}

/* Function handles errors */
int handle_error(int error){
	switch(error){
		case PIPE_OPEN_ERROR:
			die_errno();
			break;
		case PIPE_CLOSE_ERROR:
			die_errno();
			break;
		case CHDIR_ERROR:
			fprintf(stderr, "error: %s\n", strerror(errno));
			return SUCCESS;
		case FORK_ERROR:
			die_errno();
			break;
		case DUP_ERROR:
			die_errno();
			break;
		default:
			die("unknown");
	}
	return SUCCESS;
}

int pipeline_with_builtin(char ** argv, int argc){
	int i;
	for(i = 0; i < argc; i++){
		if(cmd_is_builtin(argv[i])) return 1;
	}
	return 0;
}

int cmd_is_builtin(char * cmd){
	int i;
	char * tmp;
	for(i = 0; i < BUILTINS_NUM; i++){
		tmp = strstr(cmd, BUILTINS[i]);
		if(tmp != NULL && tmp == cmd) return 1;
	}
	return 0;
}

char * format_cmd(char * cmd){

	char * buf = malloc(1 + strlen(cmd) * 3 * sizeof *buf);
	memset(buf, 0, 1 + strlen(cmd) * 3 * sizeof *buf);
	if(buf == NULL) return NULL; 

	char c;
	int j = 0;
	int i;
	for( i = 0; i < strlen(cmd); i++){
		c = cmd[i];
		/* To handle | placed adjacently to commands (no spaces) */
		if( c == '|'){
			buf[j++] = ' ';
			buf[j++] = c;
			buf[j++] = ' ';
		}else{
			buf[j++] = c;
		}
	}

	buf[j] = '\0';
	df("format_cmd");
	return buf;
}

int exist_consecutive_pipes(int * position, int number_of_pipes){
	int i;
	for(i = 0; i < number_of_pipes - 1; i++){
		if(position[i] == (position[i + 1] - 1)) return 1; 
	}
	return 0;
}












