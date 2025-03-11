
#include <stdio.h>
#include <string.h>
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
    char tgtChar = *argv[2];
    // create dispatcher
    int p = fork();
    if(p < 0){
        printf("Could not create dispatcher\n")
        exit(-1);
    }
    char *args[]={"./a1.4-dispatcher", tgt, NULL};
    if(p == 0){
        if(!execv(args[0], args)){

        }
    }

    if(p == 0)
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
        int i=0;
        for(i=0; i<FN_NUMBER; ++i){
            if(!strcmp(cmdName,fnNames[i]))
                break;
        }
        if(i == FN_NUMBER){
            printf("Command not found\n");
            continue;
        }
        char *intEndPtr;
        int cmdArgInt = strtol(cmdArg, &intEndPtr, 10);
        if(*intEndPtr != '\0'){
            printf("Command argument should be an integer\n");
            continue;
        }
    }while(1);
}