//TODO: bulletproof signal handling (mashing Ctrl+c)
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
#define WINDOW_RAT 0.05
#define MIN_WORKER_CHUNKS 5
#define min(a, b) (((a) < (b) ? (a) : (b)))
#define max(a, b) (((a) < (b) ? (b) : (a)))

typedef struct Worker{
	int pid; 
	int wStart, wLoad; //description of worker assignment
	int wCur, wCnt; //worker's most recent results
	bool isUpToDate;
	int pipefd[2];
	struct Worker *nxt, *prv; //pointer to next and previous worker in list
} Worker;

typedef struct Segment{
	int segStart, segSize;
	struct Segment* nxt;
} Segment;

int workerCount = 0, gapCount = 0;
int fileSz, normalWLoad;
int front = 0, proc = 0, occ = 0;
char *target, *fileName;
bool expectingLog = 0;

//WORKER = PROCESS
Segment *gapRoot = NULL, *gapHead = NULL;
Worker *workRoot = NULL, *workHead = NULL;

void extendWorkerList(){
	Worker *prv = workHead; 
	if(workHead == NULL)	
		workRoot = workHead = malloc(sizeof(Worker));
	else{
		workHead->nxt = malloc(sizeof(Worker));
		workHead = workHead->nxt;
	}
	workHead->prv = prv;
}

void deleteWorker(Worker *cur){	
	printf("deleting\n");
	workerCount--;
	if(workerCount == 0)
		gapRoot = gapHead = NULL;
	else if(cur == workRoot){
		workRoot = cur->nxt;
		workRoot->prv = NULL;
	}
	else if(cur == workHead){
		workHead = workHead->prv;
		workHead->nxt = NULL;
	}
	else{
		Worker *l = cur->prv, *r = cur->nxt;
		l->nxt = r;
		r->prv = l;
	}
	free(cur);
	printf("delete OK\n");
}

void popGapList(){
	Segment *nxt = gapRoot->nxt;
	free(gapRoot);
	gapRoot = nxt;
	if(gapRoot == NULL)
		gapHead = NULL;
}

//Finds correct position and load of "ind" worker
//Sets up its pipe
//Creates new process and returns pid of child to parent 
bool createWorker(){
	//no job to assign
	if(gapRoot == NULL && front == fileSz)
		return 0;

	workerCount++;
	extendWorkerList();

	if(gapRoot == NULL){
		workHead->wStart = workHead->wCur = front;
		workHead->wLoad = min(normalWLoad, fileSz - front);
		front += workHead->wLoad;
	}
	else{
		workHead->wStart = workHead->wCur = gapRoot->segStart;
		workHead->wLoad = gapRoot->segSize;
		popGapList();
	}
	workHead->wCnt = workHead->isUpToDate = 0;	
	pipe(workHead->pipefd);
	fcntl(workHead->pipefd[0], F_SETFL, O_NONBLOCK);		
	
	int p = fork();
	//Parent process gets back pid of child process
	if(p > 0){
		workHead->pid = p;
		return 1;
	}

	//Load child process executable	
	printf("Worker %d: Starting\n", getpid());
	//close ununsed (by child) reading end of pipe
	close(workHead->pipefd[0]);	
	
	char indStr[12], startStr[12], sizeStr[12];
	sprintf(indStr, "%d", getpid());
	sprintf(startStr, "%d", workHead->wStart);
	sprintf(sizeStr, "%d", workHead->wLoad);
	//replace stdout with pipe write end
	dup2(workHead->pipefd[1], STDOUT_FILENO);
	close(workHead->pipefd[0]);			
	close(workHead->pipefd[1]);
	
	char *args[] = {"a1.4-worker", indStr, fileName, startStr, sizeStr, target, NULL};	
	if(execv("./a1.4-worker", args) == -1){
		printf("Error loading exec\n");
		return -1; 
	}
}

void resetUpToDate(){	
	for(Worker *cur = workRoot; cur != NULL; cur = cur->nxt)	
		cur->isUpToDate = 0;
}

void requestReport(){
	resetUpToDate();
	expectingLog = 1;
	for(Worker *cur = workRoot; cur != NULL; cur = cur->nxt)	
		kill(cur->pid, SIGUSR1);
}

void printReport(){
	resetUpToDate();
	expectingLog = 0;
	//calculate individual progress
	for(Worker *cur = workRoot; cur != NULL; cur = cur->nxt){
		int wProc = cur->wCur - cur->wStart;
		double per = (wProc * 100.0) / cur->wLoad;
		double perFound = (cur->wCnt * 100.0) / wProc;
		printf("Worker (%d): Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", cur->pid, wProc, cur->wLoad, per, cur->wCnt, perFound);
	}

	//total resutlts
	double totPer = (proc * 100.0) / fileSz, totPerFound = (occ * 100.0) / proc;
	printf("Summary: Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", proc, fileSz, totPer, occ, totPerFound);
}

void handle_log(int sig){
	printf("Requesting report\n");
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
	printf("Target file size: %d\n", fileSz);
	close(fdr);	

	int initWorkerCount = (argv[3] == NULL ? 2 : atoi(argv[3]));	

	//Out of the total chunks, first batch of workers should occupy aprox FIRST_JOB%
	//So determine an aproximate load for each worker
	//ALTERNATIVE: DEFINE NORMALWLOAD FROM THE START 	
	int chunks = fileSz / CHUNK_SIZE + (fileSz % CHUNK_SIZE > 0 ? 1 : 0);
	int windowChunks = chunks * WINDOW_RAT;
	normalWLoad = CHUNK_SIZE * max((windowChunks / initWorkerCount), MIN_WORKER_CHUNKS);
	
	for(int ind = 0; ind < initWorkerCount; ind++)
		createWorker(ind);
	printf("Successfully created %d workers\n", workerCount);
		
	struct sigaction slog;
	slog.sa_handler = handle_log;
	sigemptyset(&slog.sa_mask);
	sigaction(SIGINT, &slog, NULL);

	sigset_t block;
	sigemptyset(&block);
	sigaddset(&block, SIGINT);
	
	while(proc < fileSz){
		sigprocmask(SIG_BLOCK, &block, NULL); //blocking
		//look at each worker's pipe
		bool logAfter = expectingLog;
		for(Worker *cur = workRoot; cur != NULL;){
			Worker *nxt = cur->nxt; //MIN TO PEIRAKSEIS
			int ans[2];
			if(read(cur->pipefd[0], &ans, sizeof(ans)) > 0){
				//(ans[0], ans[1]) : (position of last processed byte, targets found so far)
				//update global stats
				proc += ans[0] + 1 - cur->wCur;
				occ += ans[1] - cur->wCnt;
				cur->wCur = ans[0] + 1;
				cur->wCnt = ans[1];
				cur->isUpToDate = 1;
						
				//If worker is done, delete him and create a new one
				if(cur->wCur == cur->wStart + cur->wLoad){
					deleteWorker(cur); //LOGO AUTOU
					createWorker(); 
				}
			}
			logAfter &= cur->isUpToDate;
			cur = nxt;
		}

		if(logAfter)
			printReport();

		sigprocmask(SIG_UNBLOCK, &block, NULL); //unblocking
		//Signals are handled here	
		//TODO: HANDLE REQUESTS FROM FRONT-END			
	}
	printf("DONE\n");
	return 0; 
}
