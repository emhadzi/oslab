//TODO; small bug partioning last worker
//TODO: bulletproof signal handling (mashing Ctrl+c) (looks good)
//TODO: add timeout for trying to read from worker

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

struct Worker{
	int pid; 
	int wStart, wLoad; //description of worker assignment
	int wCur, wCnt; //worker's most recent results
	int pipefd[2];
};

int fileSz, workerPop;
struct Worker *work;

void getReport(){
	//send signal to each worker
	for(int i = 0; i < workerPop; i++)
		kill(work[i].pid, SIGUSR1);
	//receive results
	for(int i = 0; i < workerPop; i++){
		int ans[2];
		while(read(work[i].pipefd[0], &ans, sizeof(ans)) == 0); //keep reading until there is something there
		work[i].wCur = ans[0] + 1; //update worker state based on ans
		work[i].wCnt = ans[1];
	}
}

void printReport(){
	//calculate individual progress
	int totProc = 0, totLoad = 0, totFound = 0;
	for(int i = 0; i < workerPop; i++){
		int proc = work[i].wCur - work[i].wStart;
		double per = (proc * 100.0) / work[i].wLoad;
		double perFound = (work[i].wCnt * 100.0) / proc;
		printf("Worker (%d): Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", i + 1, proc, work[i].wLoad, per, work[i].wCnt, perFound);
		totProc += proc, totLoad += work[i].wLoad, totFound += work[i].wCnt;
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

	fileSz = lseek(fdr, 0, SEEK_END);
	int chunks = fileSz / CHUNK_SIZE + (fileSz % CHUNK_SIZE > 0 ? 1 : 0);
	printf("Target file size: %d\n", fileSz);
	close(fdr);	

	workerPop = (argv[3] == NULL ? 2 : atoi(argv[3]));	
	int workerChunkCount = chunks / workerPop + (chunks % workerPop > 0 ? 1 : 0);
	int workerByteCount = workerChunkCount * CHUNK_SIZE;
 	printf("Assigning up to %d bytes to each worker\n", workerByteCount);

	work = malloc(workerPop * sizeof(struct Worker));	

	for(int i = 0; i < workerPop; i++){	
		work[i].wStart = i * workerByteCount; 
		work[i].wLoad = min(workerByteCount, fileSz - work[i].wStart);
	}

	int ind = 0, p = 1;
	for(; ind < workerPop && p > 0; ind++){
		if(pipe(work[ind].pipefd) == -1){
			printf("Error creating pipe for %d worker\n");
			p = -1;
		}
		else{
			p = fork();
			if(p > 0)
				work[ind].pid = p;
		}	
	}

	if(p < 0){
		//only parent process will land here if "ind" child is not created successfully
		//children [0, ind-2] should terminate
		printf("Error creating worker %d, terminating the rest\n", ind);
		for(int i = 0; i < ind-1; i++)
			kill(work[i].pid, SIGTERM);
		free(work);
		return -1;
	}
	else if(p == 0){
		//only child/worker will land here
		--ind;
		printf("Worker %d: Starting\n", ind + 1);
		//close ununsed (by child) reading end of pipe
		close(work[ind].pipefd[0]);	
		//TODO: LOOK INTO DUP2
		char indStr[12], startStr[12], sizeStr[12], pipeStr[12];
		sprintf(indStr, "%d", ind + 1);
		sprintf(startStr, "%d", work[ind].wStart);
		sprintf(sizeStr, "%d", work[ind].wLoad);
		sprintf(pipeStr, "%d", work[ind].pipefd[1]);

		free(work);

		char* args[] = {"a1.4-worker", indStr, argv[1], startStr, sizeStr, argv[2], pipeStr, NULL};	
		if(execv("./a1.4-worker", args) == -1){
			printf("Error loading exec\n");
			return -1; 
		}
	}
	else{
		//only parent process will land here if all children are created successfully
		printf("Successfully created %d workers\n", workerPop);
		
		struct sigaction slog;
		slog.sa_handler = handle_log;
		sigemptyset(&slog.sa_mask);
		sigaction(SIGINT, &slog, NULL);

		//wait for all children to finish and report result
		//if one child fails remember it, after all finish, report it
		for(int i = 0; i < workerPop; i++){		
			int wstatus, done;
			do{	
				errno = 0;
				done = waitpid(work[i].pid, &wstatus, 0);
			}while(errno == EINTR && done == -1);
			if(done == -1)
				printf("Worker (%d): Failed\n", i+1);
		}
		getReport();
		printReport();
		free(work);
		return 0;
	}
}
