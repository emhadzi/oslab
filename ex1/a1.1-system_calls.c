#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char** args){
	//intended: if input file does not exist, create it resulting in 0 occurances 
	if(args[1] == NULL || args[2] == NULL || args[3] == NULL){
		printf("Incorrect arguments provided\n");
		return -1;
	}
	int fdr = open(args[1], O_RDONLY | O_CREAT, 0666);	

	if(fdr < 0){
		printf("Problem opening file: %s to read\n", args[1]);
		return -1;
	}

	int fdw = open(args[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if(fdw < 0){
		printf("Problem opening file: %s to write\n", args[2]);
		return -1;
	}

	const int batch_sz = (args[4] == NULL ? 1024 : atoi(args[4]));
	char buff[batch_sz];
	int cnt = 0, read_sz;
	
	do{
		read_sz = read(fdr, buff, batch_sz);
		for(int i = 0; i < read_sz; i++)
			cnt += buff[i] == args[3][0];
	}while(read_sz == batch_sz);
	
	close(fdr);

	char msg[128]; //problem when msg has more than 128 characters?	
	int write_sz = snprintf(msg, sizeof(msg)
		, "The character '%c' appears %d times in file %s.\n", args[3][0], cnt, args[1]);

	int written = write(fdw, msg, write_sz);
	if(written != write_sz){
		printf("Problem writing to file\n");
		return -1;
	}

	close(fdw);
}
