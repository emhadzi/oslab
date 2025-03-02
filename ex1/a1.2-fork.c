#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

int main(){
	int x;
	pid_t p = fork();
	if(p < 0){
		printf("Error creating process\n");
		return -1;
	}
	else if(p == 0){
		pid_t child_pid = getpid();
		pid_t parent_pid = getppid();
		printf("Hello, i am the child process with pid: %ld, my parent is %ld\n"
			, child_pid, parent_pid);
		char* args[] = {"a1.1-system_calls", "fin.txt", "fout.txt", "a", NULL};
		if(execv("./a1.1-system_calls", args) == -1){
			printf("Error loading exec\n");
			return -1; 
		}
		//exit(1);
	}
	else{
		printf("My child's pid is: %ld\n", p);
		int wstatus;
		pid_t done = wait(&wstatus);
		printf("Child terminated with: %ld\n", wstatus);
		exit(1);
	}

}
