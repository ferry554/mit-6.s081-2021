#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define READ 0
#define WRITE 1
#define NULL (int*) 0

int main(int argc, char *argv[])
{
    int pipe1[2];
    int pipe2[2];
    pipe(pipe1);
    pipe(pipe2);

    int pid = fork();
    if (pid == 0)
    {
        char buf_son[8];
        close(pipe1[WRITE]);
        read(pipe1[READ], buf_son, sizeof(buf_son));
        close(pipe1[READ]);
        close(pipe2[READ]);
        write(pipe2[WRITE], "pong\n", 5);
        close(pipe2[WRITE]);
        printf("%d: received %s", getpid(), buf_son);
        exit(0);
    }
    else
    {
        char buf_parent[8];
        close(pipe1[READ]);
        write(pipe1[WRITE], "ping\n", 5);
        close(pipe1[WRITE]);
        close(pipe2[WRITE]);
        read(pipe2[READ], buf_parent, sizeof(buf_parent));
        close(pipe2[READ]);
        wait(NULL);
        printf("%d: received %s", getpid(), buf_parent);
        exit(0);
    }
}