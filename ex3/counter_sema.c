#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <semaphore.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define CHUNK_SZ 1024

int n;
pid_t *pid;
int *shared_counter_ptr;
sem_t *sem;

char *input_file;
char target;
int file_size;

void child(int id){
        int fd = open(input_file, O_RDONLY);
        int start, sz, res = 0;
        // file_size / n + 1,
        if(id < file_size % n){
                start = (1 + file_size / n) * id;
                sz = 1 + file_size / n;
        }
        else{
                start = file_size % n + id * (file_size / n);
                sz = file_size / n;
        }

        char buff[CHUNK_SZ];
        lseek(fd, start, SEEK_SET);

        while(sz > 0){
                int read_sz = read(fd, buff, min(sz, CHUNK_SZ));
                for(int i = 0; i < read_sz; i++)
                        if(buff[i] == target)
                                res++;
                sz -= read_sz;
        }
        sem_wait(sem);
        *shared_counter_ptr += res;
        sem_post(sem);
}

/*
argv[1]: Input file
argv[2]: Character to search
argv[3]: Processes to create
*/
int main(int argc, char *argv[]){
        if(argc < 4){
                printf("Invalid arguments\n");
                return -1;
        }

        input_file = argv[1];
        target = argv[2][0];
        struct stat st;
        stat(input_file, &st);
        file_size = (int)st.st_size;

        n = atoi(argv[3]);
        pid = malloc(n * sizeof(pid_t));

        void *shared_memory = mmap(NULL, sizeof(int) + sizeof(sem_t),
                 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        shared_counter_ptr = (int *)shared_memory;
        *shared_counter_ptr = 0;
        sem = (sem_t *)(shared_memory + sizeof(int));
        sem_init(sem, 1, 1); // 1 for process sharing!

        for(int i = 0; i < n; i++){
                pid[i] = fork();
                if(pid[i] == 0){
                        child(i);
                        return 0;
                }
        }

        // This is parent
        for(int i = 0; i < n; i++){
                int status;
                waitpid(pid[i], &status, 0);
        }

        printf("Found %d occurances of character %c", *shared_counter_ptr, target);
}
