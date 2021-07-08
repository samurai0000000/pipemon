/*
 * pipemon.c
 *
 * Copyright (C) 2021, Charles Chiou
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>

static pid_t pid = -1;
static unsigned int interval = 1;
static time_t timeout = 10;

static void execute(int argc, char **argv);

static void sighandler(int signum)
{
    if ((signum == SIGINT) && (pid > 0)) {
        int status;

        kill(pid, SIGINT);
        waitpid(pid, &status, 0);
        exit(0);
    }
}

int main(int argc, char **argv)
{
    int ret = 0;
    int pin[2];
    int pout[2];
    struct timeval since;
    struct timeval now;
    time_t conntime, secs, mins, hours, days;

    signal(SIGINT, sighandler);
    signal(SIGPIPE, SIG_IGN);

    ret = pipe(pin);
    if (ret < 0) {
        perror("pipe");
        goto done;
    }

    ret = pipe(pout);
    if (ret < 0) {
        perror("pipe");
        goto done;
    }

    gettimeofday(&since, NULL);

    /*
     * Fork a monitor that executes ssh
     */
    pid = fork();
    if (pid == -1) {
        perror("fork");
        goto done;
    } else if (pid == 0) {
        /*
         * Child
         */
        close(pin[1]);
        close(pout[0]);
        dup2(pin[0], STDIN_FILENO);
        dup2(pout[1], STDOUT_FILENO);
        close(pin[0]);
        close(pout[1]);

        execute(argc - 1, &argv[1]);
    } else {
        /*
         * Parent
         */
        close(pin[0]);
        close(pout[1]);

        for (;;) {
            ssize_t size;
            char buf[1024];
            fd_set readfds;
            struct timeval ssh_timeout;

            sleep(interval);

            size = write(pin[1], "hello\0", 6);
            if (size != 6) {
                ret = -1;
                goto done;
            }

            FD_ZERO(&readfds);
            FD_SET(pout[0], &readfds);

            ssh_timeout.tv_sec = timeout;
            ssh_timeout.tv_usec = 0;
            ret = select(pout[0] + 1, &readfds, NULL, NULL, &ssh_timeout);
            if (ret <= 0 || !FD_ISSET(pout[0], &readfds)) {
                ret = -1;
                goto done;
            }

            size = read(pout[0], buf, sizeof(buf));
            if (size != 6 || strcmp(buf, "hello") != 0) {
                ret = -1;
                goto done;
            }

            gettimeofday(&now, NULL);
            conntime = now.tv_sec - since.tv_sec;
            secs = conntime % 60;
            mins = conntime / 60;
            mins %= 60;
            hours = conntime / 3600;
            hours %= 60;
            days = conntime / 86400;
            if (days > 0) {
                printf("alive: %lud:%.2lu:%.2lu:%.2lu\n",
                       days, hours, mins, secs);
            } else {
                printf("alive: %.2lu:%.2lu:%.2lu\n", hours, mins, secs);
            }
        } while(1);
    }

done:

    if (pid > 0) {
        kill(pid, SIGINT);
    }

    if (ret == 0) {
        exit(0);
    } else {
        exit(EXIT_FAILURE);
    }

    return 0;
}

static void execute(int argc, char **argv)
{
    int ret;

    ret = execvp(argv[0], argv);
    if (ret == 0)
        exit(0);
    else
        exit(EXIT_FAILURE);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */