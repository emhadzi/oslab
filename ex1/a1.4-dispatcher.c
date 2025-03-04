//TODO; small bug partioning last worker
//TODO: organise worker info better with a dedicated struct and free memory
//TODO: bulletproof signal handling (mashing Ctrl+c) (looks good)

#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#define CHUNK_SIZE 4096
#define min(a, b) (((a) < (b) ? (a) : (b)))

int sz, workers;
int *pid, *workerCur, *workerCnt, *workerStart, *workerLoad;
int (*pipefd)[2];

//send signal to all children to report their results
//look at each pipe for results
void getReport(){
	//send signal to each worker
	for(int i = 0; i < workers; i++)
		kill(pid[i], SIGUSR1);
	//receive results
	for(int i = 0; i < workers; i++){
		int ans[2];
		while(read(pipefd[i][0], &ans, sizeof(ans)) == 0); //keep reading until there is something there
		workerCur[i] = ans[0] + 1; //update worker state based on ans
		workerCnt[i] = ans[1];
	}
}

void printReport(){
	//calculate individual progress
	int totProc = 0, totLoad = 0, totFound = 0;
	for(int i = 0; i < workers; i++){
		int proc = workerCur[i] - workerStart[i];
		double per = (proc * 100.0) / workerLoad[i];
		double perFound = (workerCnt[i] * 100.0) / proc;
		printf("Worker (%d): Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", i + 1, proc, workerLoad[i], per, workerCnt[i], perFound);
		totProc += proc, totLoad += workerLoad[i], totFound += workerCnt[i];
	}

	//total resutlts
	double totPer = (totProc * 100.0) / totLoad, totPerFound = (totFound * 100.0) / totProc;
	printf("Summary: Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", totProc, totLoad, totPer, totFound, totPerFound);
}

void handle_log(int sig){
	printf("Singal received\n");
	getReport();
	printReport();
}

//argv[1] : fin.txt
//argv[2] : character to search
//(optional argv[3] : number of workers)
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
	sz = lseek(fdr, 0, SEEK_END);
	int chunks = sz / CHUNK_SIZE + (sz % CHUNK_SIZE > 0 ? 1 : 0);
	printf("Target file size: %d\n", sz);
	close(fdr);	

	workers = (argv[3] == NULL ? 2 : atoi(argv[3]));	
	int workerChunkCount = chunks / workers + (chunks % workers > 0 ? 1 : 0);
	int workerByteCount = workerChunkCount * CHUNK_SIZE;
 	printf("Assigning up to %d bytes to each worker\n", workerByteCount);

	pid = malloc(workers * sizeof(pid_t));
	workerCur = malloc(workers * sizeof(int));	
	workerCnt = malloc(workers * sizeof(int));	
	workerStart = malloc(workers * sizeof(int));
	workerLoad = malloc(workers * sizeof(int));
	pipefd = malloc(workers * sizeof(int[2]));

	for(int i = 0; i < workers; i++){	
		workerStart[i] = i * workerByteCount; 
		workerLoad[i] = min(workerByteCount, sz - workerStart[i]);
	}

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
		--ind;
		//only child/worker will land here
		printf("Worker %d: Starting\n", ind + 1);
		//close ununsed (by child) reading end of pipe
		close(pipefd[ind][0]);	
	
		char indStr[12], startStr[12], sizeStr[12], pipeStr[12];
		sprintf(indStr, "%d", ind + 1);
		sprintf(startStr, "%d", workerStart[ind]);
		sprintf(sizeStr, "%d", workerLoad[ind]);
		sprintf(pipeStr, "%d", pipefd[ind][1]);

		char* args[] = {"a1.4-worker", indStr, argv[1], startStr, sizeStr, argv[2], pipeStr, NULL};	
		if(execv("./a1.4-worker", args) == -1){
			printf("Error loading exec\n");
			return -1; 
		}
	}
	else{
		//only parent process will land here if all children are created successfully
		printf("Successfully created %d workers\n", workers);
		
		struct sigaction slog;
		slog.sa_handler = handle_log;
		sigemptyset(&slog.sa_mask);
		sigaction(SIGINT, &slog, NULL);

		//wait for all children to finish and report result
		//if one child fails remember it, after all finish, report it
		for(int i = 0; i < workers; i++){		
			int wstatus, done;
			do{	
				errno = 0;
				done = waitpid(pid[i], &wstatus, 0);
			}while(errno == EINTR && done == -1);
			if(done == -1)
				printf("Worker (%d): Failed\n", i+1);
		}
		getReport();
		printReport();
	}
}
