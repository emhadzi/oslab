#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#define BATCH_SIZE 16384
#define SLEEP_DURATION 5000
#define min(a, b) (((a) < (b) ? (a) : (b)))

int workers, active;

void handler(int sig){
	printf("There are %d active workers\n", active); 
} 

int main(int argc, char** argv){
	if(argc < 3){
		printf("Incorrect arguments provided\n");
		return -1;
	}

	//get target file size
	int fdr = open(argv[1], O_RDONLY | O_CREAT, 0666);	
	if(fdr < 0){
		printf("Problem opening target file\n");
		return -1;
	}	
	int sz = lseek(fdr, 0, SEEK_END);
	printf("Target file size: %d\n", sz);
	close(fdr);	

	
	workers = (argv[3] == NULL ? 2 : atoi(argv[3]));	
	int workerSz = sz / workers, rem = sz % workers;
 	printf("Assigning chunks of up to %d bytes to each worker\n", workerSz);

	pid_t pid[workers];
	bool done[workers];
	memset(done, 0, sizeof(done));

	int pipefd[workers][2];
	
	//block SIGINT for all processes
	sigset_t block;
	sigemptyset(&block);
	sigaddset(&block, SIGINT);	
	sigprocmask(SIG_BLOCK, &block, NULL);

	int ind = 0, p = 1;
	for(; ind < workers && p > 0; ind++){
		if(pipe(pipefd[ind]) == -1){
			printf("Error creating pipe for %d worker\n");
			p = -1;
		}	
		else{
			fcntl(pipefd[ind][0], F_SETFL, O_NONBLOCK);		
			p = fork();
			if(p > 0)
				pid[ind] = p;
		}	
	}

	if(p < 0){
		//only parent process will land here if "ind" child is not created successfully
		//children [0, ind-2] should terminate
		printf("Error creating worker %d, terminating the rest\n", ind);
		for(int i = 0; i < ind-1; i++)
			kill(pid[i], SIGTERM);
		return -1;
	}

	else if(p == 0){
		int i = ind-1;
		//only child/worker will land here
		printf("Worker %d: Starting\n", ind);
		//close ununsed (by child) reading end of pipe
		close(pipefd[i][0]);	
		//block SIGINT
		//sigprocmask(SIG_BLOCK, &block, NULL);
	
		//create new file descriptor
		fdr = open(argv[1], O_RDONLY, 0666);	
		if(fdr < 0){
			printf("Worker %d could not open target file\n", ind);
			return -1;
		}	

		//deterimine starting and ending positions (inclusive)
		int startPos, endPos;
		if(i < rem){
			startPos = i * (workerSz+1);
			endPos = startPos + workerSz;
		}
		else{
			startPos = rem + i * workerSz;
			endPos = startPos + workerSz - 1;
		}
		int ans = 0;
		char buff[BATCH_SIZE];

		//set fd offset to startPos
		lseek(fdr, startPos, SEEK_SET);

		for(int curPos = startPos; curPos <= endPos;){	
			int readSz = min(BATCH_SIZE, endPos - curPos + 1);	
			read(fdr, buff, readSz);
			usleep(SLEEP_DURATION);
			curPos += readSz;			

			for(int i = 0; i < readSz; i++)
				ans += buff[i] == argv[2][0]; 
		} 		
		close(fdr);

		//write result to pipe
		write(pipefd[i][1], &ans, sizeof(int));
		close(pipefd[i][1]);
		printf("Worker %d: Done\n", ind);		

		return 0;
	}

	else{
		//only parent process will land here if all children are created successfully
		active = workers;
		printf("Successfully created %d workers\n", workers);
		//wait for all children to finish and report result
		//if one child fails remember it, after all finish, report it
		int tot = 0;
		bool ok = 1;	
	
		struct sigaction slog;
		slog.sa_handler = handler;
		slog.sa_flags = 0;
		sigemptyset(&slog.sa_mask);
		sigaction(SIGINT, &slog, NULL);		
	
		while(active > 0){
			for(int i = 0; i < workers; i++){
				if(done[i])
					continue;		
				int wstatus;
				sigprocmask(SIG_BLOCK, &block, NULL);
				pid_t pdone = waitpid(pid[i], &wstatus, WNOHANG);
				sigprocmask(SIG_UNBLOCK, &block, NULL);	
				//printf("HI");			
				if(pdone == pid[i]){
					int res;
					int sz = read(pipefd[i][0], &res, sizeof(int));
					if(sz > 0){
						close(pipefd[i][0]); 	
						printf("Worker (%d): Successfully accounted\n", i+1);
						tot += res;
					}
					else{
						printf("Worker (%d): Failed\n", i+1);
						ok = 0;
					}
					done[i] = 1;
					active--;
				}
			}
		}

		if(!ok) printf("Cannot provide final result because some worker failed\n");
		else printf("Final result: %d\n", tot);
	}

}
