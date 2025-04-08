#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv){
	//intended: if input file does not exist, create it resulting in 0 occurances 
	if(argc != 4 && argc != 5){
		printf("Incorrect number of arguments provided\n");
		return -1;
	}

	if(strlen(argv[3]) > 1){
		printf("Program does not support matching >1 character\n");
		return -1;
	}

	int fdr = open(argv[1], O_RDONLY | O_CREAT, 0666);	

	if(fdr < 0){
		printf("Problem opening file: %s to read\n", argv[1]);
		return -1;
	}

	int fdw = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if(fdw < 0){
		printf("Problem opening file: %s to write\n", argv[2]);
		return -1;
	}

	const int batch_sz = (argv[4] == NULL ? 1024 : atoi(argv[4]));
	char buff[batch_sz];
	int cnt = 0, read_sz;
	
	do{
		read_sz = read(fdr, buff, batch_sz);
		for(int i = 0; i < read_sz; i++)
			cnt += buff[i] == argv[3][0];
	}while(read_sz == batch_sz);
	
	close(fdr);

	char msg[128]; //problem when msg has more than 128 characters?	
	int write_sz = snprintf(msg, sizeof(msg)
		, "The character '%c' appears %d times in file %s.\n", argv[3][0], cnt, argv[1]);

	int written = write(fdw, msg, write_sz);
	if(written != write_sz){
		printf("Problem writing to file\n");
		return -1;
	}

	close(fdw);
}
