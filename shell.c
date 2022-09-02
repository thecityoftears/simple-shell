#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
//Andy Sun 260905332
pid_t pid = 1;
pid_t childlist[1000];
int child_count = 0;

int executePipe(char *pipein[], char *pipeout[], int *background){
	pid_t pidIn, pidOut;
	int pipefd[2];

	if(pipe(pipefd)<0){
		perror("\nPipe initalization failed.");
		return 1;
	}

	pidIn = fork();
	if (pidIn < 0){
		perror("\nChild creation failed.");
		return 1;
	}else if (pidIn == 0){
		//execution of first command
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);

		if(execvp(pipein[0], pipein)<0){
			printf("\nInvalid Command.");
			exit(1);
		}
		exit(0);
	}else{
		pidOut = fork();
		if(pidOut < 0){
			perror("\nChild creation failed.");
			return 1;
		}else if (pidOut == 0){
			//execution of second command
			close(pipefd[1]);
			dup2(pipefd[0], STDIN_FILENO);
			close(pipefd[0]);
			
			if(execvp(pipeout[0], pipeout)<0){\
				printf("\nInvalid Command.");
				exit(1);
			}
			exit(0);
		}else{
			if(*background == 0){
				close(pipefd[0]);
				close(pipefd[1]);
				wait(NULL);
				wait(NULL);
			}else{
				close(pipefd[0]);
				close(pipefd[1]);
				childlist[child_count++] = pidIn;
				childlist[child_count++] = pidOut;
			}
		}
		return 0;
	}
}

int checkCmdPiping(char *pipein[], char *pipeout[], char *args[]){
	int cmdpipe = 0;
	int pipe2 = 0;
	for (int i = 0; args[i] != NULL; i++){
		if(strcmp(args[i], "|") == 0){
			cmdpipe = 1;
		}
		if (cmdpipe == 0){
			pipein[i] = args[i];
		}else if (cmdpipe == 1 && strcmp(args[i], "|") != 0){
			pipeout[pipe2++] = args[i];
		}
	}
	return cmdpipe;
}

int checkOutRedirect(char *redirect[], char *copy[], char *args[]){
	int rd = 0;
	for(int i = 0; args[i] != NULL; i++){
		if(strcmp(args[i], ">") == 0){
			rd = 1;
		}
		if (rd == 0){
			copy[i] = args[i];
		}
		redirect[0] = args[i];	
	}
	return rd;
}

//make ctrl c kill a process unless there is no process to kill
static void newSigInt(){
	if(kill(pid, 2) == 0){
		kill(pid, SIGKILL);
	}else{
	}
}

int builtin(char *args[]){
	if(strcmp(args[0], "cd") == 0){
		if(args[1]!=NULL){
			if(chdir(args[1])!=0){
				perror("\n");
			}
		}else{
			char cwd[PATH_MAX];
			getcwd(cwd, sizeof(cwd));
			printf("\n%s", cwd);
		}
	}else if(strcmp(args[0], "pwd") == 0){
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof(cwd));
		printf("\n%s", cwd);
	}else if(strcmp(args[0], "exit") == 0){
		exit(0);
	}else if(strcmp(args[0], "fg") == 0){
		//if the second argument is specified, wait for the specified background process 
		//to finish, else wait for any to finish
		int status;
		if(args[1]!=NULL){
			waitpid(atoi(args[1]), &status, 0);
		}else{
			waitpid(-1, &status, 0);
		}
	}else if(strcmp(args[0], "jobs") == 0){
		//go through the processes declared to run in the background,
		//if it is still running, print it
		for(int i = 0; i<1000; i++){
			if(waitpid(childlist[i], NULL, WNOHANG)==0){
				printf("\n%d", childlist[i]);
			}
		}
	}
	return 0;
}

int getcmd(char *args[], int *background, char *line){
	int i = 0;
	char *token, *loc;

	//check if background is specified
	if ((loc = index(line, '&')) != NULL){
		*background = 1;
		*loc = ' ';
	}else{
		*background = 0;
	}
	
	while((token = strsep(&line, " \t\n")) != NULL){
		for(int j = 0; j < strlen(token); j++){
			if(token[j] <= 32){
				token[j] = '\0';
			}
		}if (strlen(token) > 0){
			args[i++] = token;
		}
	}

	return i;
}
int main(void){
        //handle signals
	signal(SIGINT, newSigInt);
	signal(SIGTSTP, SIG_IGN);

	//set list of built-in cmds
	char *built_in_commands[5];
	built_in_commands[0] = "cd";
	built_in_commands[1] = "pwd";
	built_in_commands[2] = "exit";
	built_in_commands[3] = "fg";
	built_in_commands[4] = "jobs";
	while(1){
		int bcmd = 0;
		size_t linecap = 0;
		char *line = NULL;
		int length = 1;
		int bg;
		
		//get input
		while (length == 1){
			printf("\n>>");
			length = getline(&line, &linecap, stdin);
			if(length == 1){
				printf("\n There was no input, please try again.");
			}
		}

		//count the words in line
		int wordcount = 0;
		for (int i = 0; line[i] != '\0'; i++){
			if(line[i] == ' ' && line[i+1] != ' '){
				wordcount++;
			}
		}
		wordcount+=2;
		char *args[wordcount+100];
		char *parsedargs[wordcount+100];
		char *pipeInArgs[wordcount+100];
		char *pipeOutArgs[wordcount+100];
		memset(pipeInArgs, 0, (wordcount+100)*sizeof(char*));
		memset(pipeOutArgs, 0, (wordcount+100)*sizeof(char*));
		memset(parsedargs, 0, (wordcount+100)*sizeof(char*));
		memset(args, 0, (wordcount+100)*sizeof(char*));
		int cnt = getcmd(args, &bg, line);
			
		//check if command is built-in
		for (int i = 0; i < 5; i++){
			if(strcmp(args[0], built_in_commands[i]) == 0){
				bcmd = 1;
				break;
			}
		}
		if (bcmd == 1){
			builtin(args);
		}
		//check for command piping
		else if(checkCmdPiping(pipeInArgs, pipeOutArgs, args) == 1){
			executePipe(pipeInArgs, pipeOutArgs, &bg);
		}
		else{
	                //fork a child
        	        pid = fork();

	                //error handling
		        if(pid<0){		
                    	        fprintf(stderr, "Fork Failure");
	            	        return 1;	
    	       	        }else if (pid == 0){
				//if the child is to run in the background, add it to the jobs list
				if (bg == 1){
					childlist[child_count++] = pid;
				}
			        //if the command contains an output redirection, 
				//redirect the output and
		                //use the parsed lines instead
			        char *redirect[1] = {0};
		    	        if(checkOutRedirect(redirect, parsedargs, args) == 1){
			    	        close(1);
				        int fd = open(redirect[0], O_WRONLY | O_APPEND | O_CREAT, 0777);
				        if(execvp(parsedargs[0], parsedargs)<0){
				 	        printf("Invalid Command.");
					        exit(1);
				        }
			 	        exit(0);
			        }
		    	        if(execvp(args[0], args)<0){
	           		        printf("Invalid Command.");
        	    		        exit(1);
                	        }
	                        exit(0);
	                 //parent handling whether or not to wait
	                 }else{
	                         if(bg == 0){
	                                 wait(NULL);
	                         }else{
	                                 //note that because no waiting is done here, formatting
	                                 //of the shell will screw up
	                         }
	                 }
	        }
	}
	return 0;
}
