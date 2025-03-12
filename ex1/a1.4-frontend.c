#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#define FN_NUMBER 3
#define CMD_SIZE 128

char *fnNames[FN_NUMBER]={"add", "delete", "show"};

int main(int argc, char *argv[]) {
    // check arguments
    if (argc != 3) {
        printf("Invalid argument list\n");
        exit(-1);
    }
    int fdr = open(argv[1], O_RDONLY | O_EXCL);
    if(fdr < 0){
        printf("Could not open file\n");
        exit(-1);
    }
    close(fdr);
    if(strlen(argv[2]) != 1){
        printf("Search target should be a character\n");
        exit(-1);
    }
    // create dispatcher
    int pipefd[2];
    pipe(pipefd);
    int p = fork();
    if(p < 0){
        printf("Could not create dispatcher\n");
        exit(-1);
    }
    char *args[]={"./a1.4-dispatcher", argv[1], argv[2], "2", NULL};
    if(p == 0){
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        if(!execv(args[0], args)){
            printf("Could not load dispatcher\n");
            exit(-1);
        }
    }
    close(pipefd[0]);
    char cmd[CMD_SIZE];
    do{
        // parse command (cmd)
        int cmdSize = read(STDIN_FILENO, cmd, CMD_SIZE);
        if(cmdSize < 0)
            continue;
        char *tokPtr;
        char *cmdName = strtok_r(cmd, " ", &tokPtr);
        char *cmdArg = strtok_r(NULL, "\n", &tokPtr);
        if(!cmdName || !cmdArg){
            printf("Invalid command structure\n");
            continue;
        }
        int fID=0;
        for(fID=0; fID<FN_NUMBER; ++fID){
            if(!strcmp(cmdName,fnNames[fID]))
                break;
        }
        if(fID == FN_NUMBER){
            printf("Command not found\n");
            continue;
        }
        char *intEndPtr;
        int cmdArgInt = strtol(cmdArg, &intEndPtr, 10);
        if(*intEndPtr != '\0'){
            printf("Command argument should be an integer\n");
            continue;
        }
        int buff[2]={fID,cmdArgInt};
        write(pipefd[1], buff, sizeof(buff));
        printf("Buff 0 :%i, 1: %i\n", buff[0],buff[1]);
    }while(1);
}