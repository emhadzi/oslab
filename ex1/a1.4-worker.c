//Worker receives as arguments : (1) his index, (2) input file name (to count from), (3) position (int bytes) to start reading
//(4) number of bytes to read, (5) character to count, (6) file descriptor to write-only pipe (write result for each chunk)
//TODO: look at dup2(fd, ...) to avoid passing file descriptors as arguments to workers

//Logging (chunks processed, count so far) will be sent through (SIGUSR1, SIGUSR2)

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#define BUFF_SIZE 4096
#define READ_DELAY 10

int pfd, workerId;
int cur, cnt;

void write_to_pipe(int lastProc, int count){ //lastProc: last processed byte (from start of file)
	char msg[12];
	int write_sz = snprintf(msg, sizeof(msg), "%d%d%d", workerId, lastProc, count);
	//write(pfd, msg, sizeof(msg));
	//TODO: Erase later
	printf("Successfully wrote: %d, %d, %d\n", workerId, lastProc, count);
}

void handle_log(int sig){
	printf("Singal received\n");
	write_to_pipe(cur-1, cnt);
	printf("Worker (%d) report sent\n", workerId);
}

int main(int argc, char **argv){
	if(argc < 7){
		printf("Invalid arguments\n");
		return -1; 
	}

	pfd = atoi(argv[6]), workerId = atoi(argv[1]);
	int fd = open(argv[2], O_RDONLY);
	if(fd < 0){
		printf("Worker %d: Error opening file to read\n", workerId);
		return -1;
	}

	struct sigaction slog;
	slog.sa_handler = handle_log;
	sigemptyset(&slog.sa_mask);
	slog.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &slog, NULL);

	int startPos = atoi(argv[3]), endPos = startPos + atoi(argv[4]) - 1;
	cur = startPos, cnt = 0;
	lseek(fd, startPos, SEEK_SET);
	char buff[BUFF_SIZE];

	//For signal blocking to work:
	sigset_t mask, oldmask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	//Dispatcher should allways provide workers with chunks that are alligned with BUFF_SIZE
	while(cur < endPos){
		read(fd, buff, BUFF_SIZE); //TODO: look at what happens if SIGINT is received while reading
		sleep(READ_DELAY);
		int batch = 0;
		for(int j = 0; j < BUFF_SIZE; j++)
			batch += buff[j] == argv[5][0];
		sigprocmask(SIG_BLOCK, &mask, &oldmask); //blocking
		cnt += batch;
		cur += BUFF_SIZE;	
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
	} 
	close(fd);
	
	printf("Worker (%d) finished: Found %d occurances\n", workerId, cnt);
	write_to_pipe(endPos, cnt);
	close(pfd);

	return 0;
}

