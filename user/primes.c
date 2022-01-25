#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define READ 0
#define WRITE 1
#define NULL (int *)0

int main(int argc, char *argv[])
{
    int pipe_parent[2];
    pipe(pipe_parent);
    int pid = fork();
    if (pid > 0)
    {
        close(pipe_parent[READ]);
        for (int i = 2; i <= 35; i++)
        {
            write(pipe_parent[WRITE], &i, sizeof(int));
        }
        close(pipe_parent[WRITE]);
        wait(NULL);
        exit(0);
    }
    else
    {
        int min;
        close(pipe_parent[WRITE]);
        while (read(pipe_parent[READ], &min, sizeof(int)))
        {
            printf("prime %d\n", min);
            int pipe_child[2];
            pipe(pipe_child);
            int i;
            while (read(pipe_parent[READ], &i, sizeof(int)))
            {
                if (i % min)
                {
                    write(pipe_child[WRITE], &i, sizeof(int));
                }
            }
            close(pipe_child[WRITE]);
            int pid_2 = fork();
            if (pid_2 == 0)
            {
                pipe_parent[READ] = dup(pipe_child[READ]);
                close(pipe_child[READ]);
            }
            else
            {
                close(pipe_child[READ]);
                wait(NULL);
                exit(0);
            }
        }
        exit(0);
    }
    exit(0);
}
