//Worker receives as arguments : (1) input file name (to count from), (2) position (int bytes) to start reading
//(3) number of bytes to read, (4) character to count 

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#define BUFF_SIZE 4096
#define READ_DELAY 1

int startPos, endPos;
int cur, cnt;

void write_to_pipe(int lastProc, int count){ //lastProc: last processed byte (from start of file)
	int msg[2];
	msg[0] = lastProc, msg[1] = count;
	write(STDOUT_FILENO, msg, sizeof(msg));
	//printf("Successfully wrote: %d, %d, %d\n", workerId, lastProc, count);
}

void handle_log(int sig){
	//printf("Worker (%d): Singal received\n", workerId);
	write_to_pipe(cur-1, cnt);
	//printf("Worker (%d): Report sent\n", workerId);
}

int main(int argc, char **argv){
	if(argc < 5){
		//printf("Invalid arguments\n");
		return -1; 
	}

	int fd = open(argv[1], O_RDONLY);
	if(fd < 0){
		//printf("Worker %d: Error opening file to read\n", workerId);
		return -1;
	}

	struct sigaction slog;
	slog.sa_handler = handle_log;
	sigemptyset(&slog.sa_mask);

	sigaction(SIGUSR1, &slog, NULL);

	startPos = atoi(argv[2]), endPos = startPos + atoi(argv[3]) - 1;
	cur = startPos, cnt = 0;
	lseek(fd, startPos, SEEK_SET);
	char buff[BUFF_SIZE];

	//For signal blocking to work:
	sigset_t block;
	sigemptyset(&block);
	sigaddset(&block, SIGUSR1);
	//Dispatcher should allways provide workers with chunks that are alligned with BUFF_SIZE
	//Final chunk might not be full
	while(cur < endPos){
		sigprocmask(SIG_BLOCK, &block, NULL); //blocking
		int readSz = read(fd, buff, BUFF_SIZE); 
		sleep(READ_DELAY);
		int batch = 0;
		for(int j = 0; j < readSz; j++)
			batch += buff[j] == argv[4][0];
		cnt += batch;
		cur += readSz;	
		sigprocmask(SIG_UNBLOCK, &block, NULL);
	} 
	close(fd);
	
	//printf("Worker (%d) finished: Found %d occurances\n", workerId, cnt);
	write_to_pipe(endPos, cnt);

	return 0;
}

