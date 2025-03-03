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

void handle_log(int sig){
	printf("Singal received\n");
	exit(0);
}

int main(int argc, char **argv){
	if(argc < 7){
		printf("Invalid arguments\n");
		return -1; 
	}

	int fd = open(argv[2], O_RDONLY);
	if(fd < 0){
		printf("Worker %s: Error opening file to read\n", argv[1]);
		return -1;
	}

	struct sigaction slog;
	slog.sa_handler = handle_log;
	sigemptyset(&slog.sa_mask);
	slog.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &slog, NULL);

	int cnt = 0;
	int startPos = atoi(argv[3]), endPos = startPos + atoi(argv[4]) - 1;
	lseek(fd, startPos, SEEK_SET);
	char buff[BUFF_SIZE];

	//Dispatcher should allways provide workers with chunks that are alligned with BUFF_SIZE
	for(int i = startPos; i < endPos; i += BUFF_SIZE){
		read(fd, buff, BUFF_SIZE);
		sleep(READ_DELAY);
		int batch = 0;
		for(int j = 0; j < BUFF_SIZE; j++)
			batch += buff[j] == argv[5][0];
		cnt += batch;	
	} 
	close(fd);
	
	printf("Worker (%s): Found %d occurances\n", argv[1], cnt);
	int pfd = atoi(argv[6]);
	write(pfd, &cnt, sizeof(int));
	close(pfd);

	return 0;
}

