#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

//for frontend
#define FN_NUMBER 3
#define CMD_SIZE 128
#define ADD 0
#define DELETE 1
#define SHOW 2

//for worker
#define BUFF_SIZE 4096
#define READ_DELAY 0

//for dispatcher
#define CHUNK_SIZE 4096
#define WINDOW_RAT 0.05
#define MIN_WORKER_CHUNKS 5
#define NORMAL_WORKER_CHUNKS 20 
#define min(a, b) (((a) < (b) ? (a) : (b)))
#define max(a, b) (((a) < (b) ? (b) : (a)))
//because of limited number of pipes
#define MAX_WORKER_COUNT 1000

#define int long long
