#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#define BATCH_SIZE 16384
#define min(a, b) (((a) < (b) ? (a) : (b)))

int main(int argvc, char** argv){
	if(argv[1] == NULL || argv[2] == NULL || argv[3] == NULL){
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

	int workers = (argv[4] == NULL ? 2 : atoi(argv[4]));	
	int workerSz = sz / workers + (sz % workers > 0 ? 1 : 0);
 	printf("Assigning chunks of up to %d bytes to each worker\n", workerSz);

	pid_t pid[workers];
	int pipefd[workers][2];

	int ind = 0, p = 1;
	for(; ind < workers && p > 0; ind++){
		if(pipe(pipefd[ind]) == -1){
			printf("Error creating pipe for %d worker\n");
			p = -1;
		}
		
		else{
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
		//only child/worker will land here
		printf("Worker %d: Starting\n", ind);
		//close ununsed (by child) reading end of pipe
		close(pipefd[ind-1][0]);	
	
		//create new file descriptor
		fdr = open(argv[1], O_RDONLY, 0666);	
		if(fdr < 0){
			printf("Worker %d could not open target file\n", ind);
			return -1;
		}	

		//deterimine starting and ending positions (inclusive)
		int startPos = (ind-1) * workerSz;
		int endPos = min(sz-1, ind * workerSz - 1);
		int ans = 0;
		char buff[BATCH_SIZE];

		//set fd offset to startPos
		lseek(fdr, startPos, SEEK_SET);

		for(int curPos = startPos; curPos <= endPos;){	
			int readSz = min(BATCH_SIZE, endPos - curPos + 1);	
			read(fdr, buff, readSz);
			sleep(0.1);
			curPos += readSz;			

			for(int i = 0; i < readSz; i++)
				ans += buff[i] == argv[3][0]; 
		} 		
		close(fdr);

		//write result to pipe
		write(pipefd[ind-1][1], &ans, sizeof(int));
		close(pipefd[ind-1][1]);
		printf("Worker %d: Done\n", ind);		

		return 0;
	}

	else{
		//only parent process will land here if all children are created successfully
		printf("Successfully created %d workers\n", workers);
		//wait for all children to finish and report result
		//if one child fails remember it, after all finish, report it
		int tot = 0, ok = 1;
		for(int i = 0; i < workers; i++){		
			int wstatus;
			pid_t done = waitpid(pid[i], &wstatus, 0);
			
			if(wstatus != 0){
				printf("Worker %d failed\n", i+1);
				ok = 0;
			}
			else{
				int res;
				read(pipefd[i][0], &res, sizeof(int));
				close(pipefd[i][0]); 
				tot += res;
				printf("Worker %d successfully accounted\n", i+1);
			}
		}

		if(!ok) printf("Cannot provide final result because some worker failed\n");
		else printf("Final result: %d\n", tot);
	}

}
