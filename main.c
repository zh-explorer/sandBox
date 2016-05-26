#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

#define DEBUG

char *cmd[] = {"Calc", (char *) 0};
char *path = "/tmp/Calc";
pid_t pid;

void err_sys(char *msg);

void set_fi(int fd, int flags);

void connectBoth(int childIn, int childOut);

void sig_child(int signo);

void killChild();

int main(int argc, char *argv[]) {
    int fd1[2], fd2[2];

    //set some signal and fun
    if(atexit(killChild) != 0) {
        err_sys("can't register exit fun");
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        err_sys("signal error");
    }

    if (signal(SIGCHLD, sig_child) == SIG_ERR) {
        err_sys("signal error");
    }

    if (pipe(fd1) < 0 || pipe(fd2) < 0) {
        err_sys("pipe error");
    }

    if ((pid = fork()) < 0) {
        err_sys("fork error");
    }
    else if (pid > 0) {            //father
        connectBoth(fd1[1], fd2[0]);
    } else {                       //children
        close(fd1[1]);
        close(fd2[0]);

        if (fd1[0] != STDIN_FILENO) {
            if (dup2(fd1[0], STDIN_FILENO) != STDIN_FILENO) {
                err_sys("dup2 error to stdin");
            }
            close(fd1[0]);          //not need this pipe
        }

        if (fd2[1] != STDOUT_FILENO) {
            if (dup2(fd2[1], STDOUT_FILENO) != STDOUT_FILENO) {
                err_sys("dup2 error to stdout");
            }
            close(fd2[1]);
        }

        if (execv(path, cmd)) {
            err_sys("execv error");
        }
    }
}

void connectBoth(int childIn, int childOut) {
    fd_set *readset;
    int ret;
    char *buffer;
    ssize_t size, sizeAll;
    buffer = (char *) malloc(4096);

    //some init
    set_fi(childIn, O_NONBLOCK);
    set_fi(childOut, O_NONBLOCK);
    set_fi(STDIN_FILENO, O_NONBLOCK);
    set_fi(STDOUT_FILENO, O_NONBLOCK);


    readset = (fd_set *) malloc(sizeof(readset));
    FD_ZERO(readset);
    FD_SET(childOut, readset);
    FD_SET(STDIN_FILENO, readset);

    while (1) {
        ret = select(FD_SETSIZE, readset, NULL, NULL, NULL);
        if (ret == 0 || ret == -1) {
            continue;
        }

        if (FD_ISSET(childOut, readset)) {
            sizeAll = 0;
            while (1) {
                if ((size = read(childOut, buffer + sizeAll, 4096)) != 4096) {
                    if (size == -1) {
                        size = 0;
                    }
                    sizeAll += size;
                    break;
                }
                sizeAll += size;
                if ((buffer = realloc(buffer, (size_t) (sizeAll + 4096))) == 0) {
                    err_sys("realloc error");
                }
            }
            if ((size = write(STDOUT_FILENO, buffer, (size_t) sizeAll)) != sizeAll) {
                write(STDOUT_FILENO, buffer + size, (size_t) (sizeAll - size));      //not bad to retry
            }
            if ((buffer = realloc(buffer, 4096)) == 0) {
                err_sys("realloc error");
            }
        }

        if (FD_ISSET(STDIN_FILENO, readset)) {
            sizeAll = 0;
            while (1) {
                if ((size = read(STDIN_FILENO, buffer + sizeAll, 4096)) != 4096) {
                    if (size == -1) {
                        size = 0;
                    }
                    sizeAll += size;
                    break;
                }
                sizeAll += size;
                if ((buffer = realloc(buffer, (size_t) (sizeAll + 4096))) == 0) {
                    err_sys("realloc error");
                }
            }
            if ((size = write(childIn, buffer, (size_t) sizeAll)) != sizeAll) {
                write(childIn, buffer + size, (size_t) (sizeAll - size));      //not bad to retry
            }
        }
        //reset the readset
        FD_ZERO(readset);
        FD_SET(childOut, readset);
        FD_SET(STDIN_FILENO, readset);
    }
}

void set_fi(int fd, int flags) {
    int val;
    if ((val = fcntl(fd, F_GETFL, 0)) < 0) {
        err_sys("fcntl F_GETFL error");
    }

    val |= flags;

    if (fcntl(fd, F_SETFL, val) < 0) {
        err_sys("fcntl F_GETFL error");
    }
}

void err_sys(char *msg) {
#ifdef DEBUG
    write(STDERR_FILENO, msg, strlen(msg));
    exit(1);
#endif
}

void sig_child(int signo) {
    wait(NULL);
    exit(0);
}

void killChild(){
    kill(pid,SIGTERM);
}