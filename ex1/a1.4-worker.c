//Worker receives as arguments : (1) his index, (2) input file name (to count from), (3) position (int bytes) to start reading
//(4) number of bytes to read, (5) character to count, (6) file descriptor to pipe (to report its answer to parent)
//TODO: look at dup2(fd, ...) to avoid passing file descriptors as arguments to workers

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#define BUFF_SIZE 4096

int main(int argc, char** argv){
	if(argc < 7){
		printf("Invalid arguments\n");
		return -1; 
	}

	int fd = open(argv[2], O_RDONLY);
	if(fd < 0){
		printf("Worker %s: Error opening file to read\n", argv[1]);
		return -1;
	}

	int cnt = 0;
	int startPos = atoi(argv[3]), endPos = startPos + atoi(argv[4]) - 1;
	lseek(fd, startPos, SEEK_SET);
	char buff[BUFF_SIZE];

	//Dispatcher should allways provide workers with chunks that are alligned with BUFF_SIZE
	for(int i = startPos; i < endPos; i += BUFF_SIZE){
		read(fd, buff, BUFF_SIZE);
		for(int j = 0; j < BUFF_SIZE; j++)
			cnt += buff[j] == argv[5][0];	
	} 
	close(fd);
	
	printf("Worker (%s): Found %d occurances\n", argv[1], cnt);
	int pfd = atoi(argv[6]);
	write(pfd, &cnt, sizeof(int));
	close(pfd);

	return 0;
}

