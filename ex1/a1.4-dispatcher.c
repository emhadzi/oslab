//TODO; small bug partioning last worker (will change partiioning anyways)
//TODO: bulletproof signal handling (mashing Ctrl+c)
//TODO: change job assignment algorithm to "tread"
//TODO: have "smooth" finish (no children crashing)
//TODO: fix bug: unexpected crashes with >= 10 workers when logging second time

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#define CHUNK_SIZE 4096
#define WINDOW_RAT 0.20
#define min(a, b) (((a) < (b) ? (a) : (b)))

struct Worker{
	int pid; 
	int wStart, wLoad; //description of worker assignment
	int wCur, wCnt; //worker's most recent results
	bool isUpToDate;
	int pipefd[2];
};

int workerPop, fileSz, normalWLoad;
int upToDate = 0, front = 0;
int proc = 0, occ = 0;
char *target, *fileName;
bool expectingLog = 0;

struct Worker *work;

//Finds correct position and load of "ind" worker
//Sets up its pipe
//Creates new process and returns pid of child to parent 
int createWorker(int ind){
	//TODO: Look for gaps because of failures
	work[ind].wStart = work[ind].wCur = front;
	work[ind].wLoad = normalWLoad;
	work[ind].wCnt = 0;
	if(work[ind].isUpToDate){	
		upToDate--;
		work[ind].isUpToDate = 0;	
	}
	front += normalWLoad;
	pipe(work[ind].pipefd);
	
	int p = fork();
	//Parent process gets back pid of child process
	if(p > 0)
		return p;

	//Load child process executable	
	printf("Worker %d: Starting\n", ind + 1);
	//close ununsed (by child) reading end of pipe
	close(work[ind].pipefd[0]);	
	
	char indStr[12], startStr[12], sizeStr[12];
	sprintf(indStr, "%d", ind + 1);
	sprintf(startStr, "%d", work[ind].wStart);
	sprintf(sizeStr, "%d", work[ind].wLoad);
	//replace stdout with pipe write end
	dup2(work[ind].pipefd[1], STDOUT_FILENO);
	close(work[ind].pipefd[1]);			
	
	char* args[] = {"a1.4-worker", indStr, fileName, startStr, sizeStr, target, NULL};	
	if(execv("./a1.4-worker", args) == -1){
		printf("Error loading exec\n");
		return -1; 
	}
}

void resetUpToDate(){	
	//Only workers that are done are up-to-date	
	upToDate = 0;
	for(int i = 0; i < workerPop; i++)
		work[i].isUpToDate = 0;
}

void requestReport(){
	resetUpToDate();
	expectingLog = 1;
	//send signal to each worker that is not done
	for(int i = 0; i < workerPop; i++)
		if(work[i].wCur < work[i].wStart + work[i].wLoad)
			kill(work[i].pid, SIGUSR1);
}

void printReport(){
	resetUpToDate();
	expectingLog = 0;
	//calculate individual progress
	for(int i = 0; i < workerPop; i++){
		int wProc = work[i].wCur - work[i].wStart;
		double per = (wProc * 100.0) / work[i].wLoad;
		double perFound = (work[i].wCnt * 100.0) / wProc;
		printf("Worker (%d): Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", i + 1, wProc, work[i].wLoad, per, work[i].wCnt, perFound);
	}

	//total resutlts
	double totPer = (proc * 100.0) / fileSz, totPerFound = (occ * 100.0) / proc;
	printf("Summary: Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", proc, fileSz, totPer, occ, totPerFound);
}

void handle_log(int sig){
	printf("Singal received\n");
	requestReport();
}

//argv[1] : fin.txt
//argv[2] : character to search
//(optional argv[3] : number of workers)
int main(int argc, char** argv){
	if(argc < 3){
		printf("Incorrect arguments provided\n");
		return -1;
	}

	fileName = argv[1];
	target = argv[2];
	//get target file size and chunks
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
	work = malloc(workerPop * sizeof(struct Worker));	

	//Out of the total chunks, first batch of workers should occupy aprox FIRST_JOB%
	//So determine an aproximate load for each worker
	//ALTERNATIVE: DEFINE NORMALWLOAD FROM THE START 
	int windowChunks = chunks * WINDOW_RAT;
	normalWLoad = CHUNK_SIZE * (windowChunks / workerPop);

	for(int ind = 0; ind < workerPop; ind++){
		//Only for parent process 
		int p = createWorker(ind);
		work[ind].pid = p;
	}

	printf("Successfully created %d workers\n", workerPop);
		
	struct sigaction slog;
	slog.sa_handler = handle_log;
	sigemptyset(&slog.sa_mask);
	sigaction(SIGINT, &slog, NULL);

	while(proc < fileSz){
		//look at each worker's pipe
		for(int i = 0; i < workerPop; i++){
			int ans[2];
			if(read(work[i].pipefd[0], &ans, sizeof(ans)) > 0){
				//(ans[0], ans[1]) : (position of last processed byte, targets found so far)
				//update global stats
				proc += ans[0] + 1 - work[i].wCur;
				occ += ans[1] - work[i].wCnt;
				work[i].wCur = ans[0] + 1;
				work[i].wCnt = ans[1];
				
				upToDate += !work[i].isUpToDate;
				work[i].isUpToDate = 1;
					
				//assign new job to worker if he's done
				if(work[i].wCur == work[i].wStart + work[i].wLoad)
					work[i].pid = createWorker(i); 
	
				if(expectingLog && upToDate == workerPop)
					printReport();
			}
		}
		//TODO: HANDLE REQUESTS FROM FRONT-END			
	}
	printf("DONE\n");
	free(work);
	return 0; 
}
