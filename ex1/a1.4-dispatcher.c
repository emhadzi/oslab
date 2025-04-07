#include "config.h"

typedef struct Worker{
	pid_t pid; 
	int wStart, wLoad; //description of worker assignment
	int wCur, wCnt; //worker's most recent results
	bool isUpToDate;
	int pipefd[2];
	struct Worker *nxt, *prv; //pointer to next and previous worker in list
} Worker;
//WORKER = PROCESS

typedef struct Segment{
	int segStart, segSize;
	struct Segment* nxt;
} Segment;

int workerCount = 0, gapCount = 0;
int fileSz, normalWLoad;
int front = 0, proc = 0, occ = 0;
char *target, *fileName;
bool expectingLog = 0;

Segment *gapRoot = NULL, *gapHead = NULL;
Worker *workRoot = NULL, *workHead = NULL;

void popGap(){
	Segment *nxt = gapRoot->nxt;
	free(gapRoot);
	gapRoot = nxt;
	if(gapRoot == NULL)
		gapHead = NULL;
}

void addGap(int gapStart, int gapSize){
	if(gapHead == NULL)
		gapRoot = gapHead = malloc(sizeof(Segment));
	else{
		gapHead->nxt = malloc(sizeof(Segment));
		gapHead = gapHead->nxt;
	}
	gapHead->segStart = gapStart;
	gapHead->segSize = gapSize;	
	gapHead->nxt = NULL;
}

void extendWorkerList(){
	Worker *prv = workHead; 
	if(workHead == NULL)	
		workRoot = workHead = malloc(sizeof(Worker));
	else{
		workHead->nxt = malloc(sizeof(Worker));
		workHead = workHead->nxt;
	}
	workHead->prv = prv;
	workHead->nxt = NULL;
}

void deleteWorker(Worker *cur){	
	workerCount--;
	if(workerCount == 0)
		workRoot = workHead = NULL;
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

	int wProc = cur->wCur - cur->wStart;
	if(wProc < cur->wLoad)
		addGap(cur->wCur, cur->wLoad - wProc);

	close(cur->pipefd[0]);
	free(cur);
}
//Finds correct position and load of next worker
//Sets up its pipe
//Creates new process and returns pid of child to parent 
bool createWorker(){
	//no job to assign
	if(gapRoot == NULL && front == fileSz){
		printf("Could not create worker, no job to assign\n");
		return 0;
	}
	//max worker capacity reached
	if(workerCount >= MAX_WORKER_COUNT){
		printf("Could not create worker, max capacity of %d workers reached\n", MAX_WORKER_COUNT);	
		return 0;
	}

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
		printf("Filling gap (%d, %d)\n", workHead->wStart, workHead->wLoad);
		popGap();
	}
	workHead->wCnt = workHead->isUpToDate = 0;	
	pipe(workHead->pipefd);
	fcntl(workHead->pipefd[0], F_SETFL, O_NONBLOCK);		
	
	pid_t p = fork();
	if(p > 0){
		close(workHead->pipefd[1]);
		workHead->pid = p;
		return 1;
	}

	//load child process executable	
	printf("Worker %d: Starting\n", getpid());
	
	char startStr[12], sizeStr[12];
	sprintf(startStr, "%d", workHead->wStart);
	sprintf(sizeStr, "%d", workHead->wLoad);
	//replace stdout with pipe write end
	dup2(workHead->pipefd[1], STDOUT_FILENO);
	close(workHead->pipefd[0]);			
	close(workHead->pipefd[1]);
	
	char *args[] = {"a1.4-worker", fileName, startStr, sizeStr, target, NULL};	
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

// printing fu
void printProgressbar(char name[], double per){
  	printf("%s : ",name);
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int cols = w.ws_col;
    int barSize = cols*PROGRESSBAR_WIDTH/100;
	printf("\033[0;%im[", PRINT_COLOR);
    int i=sizeof(name)-1;
	for(i=0; i<int(per*barSize); i++){
        printf("=");
	}
    printf("%.2f%%", int(per*100));
    i+=5;
    for(; i<barSize; i++){
    	printf("-");
    }
    printf("]\033[0m\n");
}

void printReport(){
	resetUpToDate();
	expectingLog = 0;
	//calculate individual progress
	printf("%d Workers running:\n", workerCount);
	for(Worker *cur = workRoot; cur != NULL; cur = cur->nxt){
		int wProc = cur->wCur - cur->wStart;
		double per = (wProc * 100.0) / cur->wLoad;
		double perFound = (cur->wCnt * 100.0) / wProc;
		printf("\tWorker (%d): Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", cur->pid, wProc, cur->wLoad, per, cur->wCnt, perFound);
	}

	//total resutlts
	double totPer = (proc * 100.0) / fileSz, totPerFound = (occ * 100.0) / proc;
	printf("Front: %d\n", front);
	
	if(gapRoot == NULL)
		printf("No gaps found\n");	
	else
		printf("Gaps:\n");
	

	for(Segment *cur = gapRoot; cur != NULL; cur = cur->nxt)
		printf("\tStart: %d, Length: %d\n", cur->segStart, cur->segSize);

	printf("Summary: Processed %d out of %d characters (%f%). Found %d occurances so far (%f%)\n", proc, fileSz, totPer, occ, totPerFound);
    printProgressbar("total progress", totPer);
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

	int initWorkerCount = (argv[3] == NULL ? 0 : atoi(argv[3]));	

	//ALTERNATIVE:
	//Out of the total chunks, first batch of workers should occupy aprox FIRST_JOB%
	//So determine an aproximate load for each worker
	normalWLoad = CHUNK_SIZE * NORMAL_WORKER_CHUNKS;
	
	for(int ind = 0; ind < initWorkerCount; ind++)
		createWorker(ind);
	printf("Successfully created %d workers\n", workerCount);
	
	//make reading from stdin non-blocking	
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);		
	
	while(proc < fileSz || workerCount > 0){
		//look if each worker is ok and if there is something in his pipe
		for(Worker *cur = workRoot; cur != NULL;){
			Worker *nxt = cur->nxt; //MIN TO PEIRAKSEIS
		
			int ans[2];
			if(read(cur->pipefd[0], ans, sizeof(ans)) > 0){
				//(ans[0], ans[1]) : (position of last processed byte, targets found so far)
				//update global stats
				proc += ans[0] + 1 - cur->wCur;
				occ += ans[1] - cur->wCnt;
				cur->wCur = ans[0] + 1;
				cur->wCnt = ans[1];
				cur->isUpToDate = 1;	
			}
	
			int status;
			pid_t p = waitpid(cur->pid, &status, WNOHANG);
			if(p == cur->pid){ 
				printf("Child with pid: %d terminated\n", cur->pid);
				deleteWorker(cur);
				bool made = createWorker();
				if(nxt == NULL && made)
					nxt = workHead;
			}

			cur = nxt;
		}
		
		bool logAfter = expectingLog;
		for(Worker *cur = workRoot; cur != NULL; cur = cur->nxt)
			logAfter &= cur->isUpToDate;

		if(logAfter)
			printReport();

		//handle request from frontend
		int req[2];
		while(read(STDIN_FILENO, req, sizeof(req)) > 0){
			switch(req[0]){
				case 0: //add
					for(int i = 0; i < req[1]; i++)
						createWorker();
					break;	
				case 1: //delete
					for(int i = 0; i < req[1] && workerCount > 0; i++)
						deleteWorker(workRoot);
					break;
				case 2: //log
					printf("Requesting report\n");
					requestReport();
					break;
				default:
					return -1;
			}
		}
	}
	printf("Final report:\n");
	printReport();
	kill(getppid(), SIGKILL);
	return 0; 
}
