//got eepy sleepy, will continue work tomorrow
// also is printf cheating? Should I use write(STDOUT_FILENO, ..)?

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#define FN_NUMBER 3
#define CMD_SIZE 128

enum Function {
    ADD_WORKER=0,
    DELETE_WORKER=1,
    SHOW_WORKERS=2
};

int (*fns[FN_NUMBER])(int n);
char tgt;
int fdr;
char *fnNames[FN_NUMBER];


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Invalid argument list\n");
        exit(-1);
    }
    fdr = open(argv[1], O_RDONLY | O_EXCL);
    if(fdr < 0){
        printf("Could not open file\n");
        exit(-1);
    }
    if(strlen(argv[2]) != 1){
        printf("Search target should be a character\n");
        exit(-1);
    }
    tgt = *argv[2];
    printf("File is %s \n", argv[1]);
    printf("Target char is %c \n", *argv[2]);
    /*
    int status=1;
    char cmd[CMD_SIZE];
    do{
        int cmdSize = read(STDIN_FILENO, cmd, CMD_SIZE);
        if(cmdSize < 0)
            continue;
        char *cmdName = strtok(cmd, " ");
        char *cmdArg = strtok(cmd, " ");
        char *cmdRem = strtok(cmd, " ");
        if(!cmdName || !cmdArg || cmdRem){
            printf("Invalid command structure\n");
        }
        int i=0;
        for(i=0; i<FN_NUMBER; ++i){
            if(!strcmp(cmd,fnNames[i]))
                break;
        }
        if(i == FN_NUMBER){
            printf("Command name not found\n");
        }
        char *cmdArgRem;
        int cmdArgInt = strtol(cmdArg, &cmdArgRem, 10);
        if(!cmdArgRem){
            printf("Command argument should be an integer");
        }
        int fd1[1];
        pipe()
    }while();
     */
}