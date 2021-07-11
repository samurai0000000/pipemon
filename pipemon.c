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
#include <math.h>

static pid_t pid = -1;
static unsigned int interval = 1;
static time_t timeout = 60;
static struct timeval threshold_rtt = {
    .tv_sec = 0,
    .tv_usec = 500000,
};
static unsigned int threshold_count = 5;

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
    char *s;
    int pin[2];
    int pout[2];
    struct timeval since;
    struct timeval now;
    unsigned int violate_rtt = 0;
    time_t conntime, secs, mins, hours, days;

    if ((s = getenv("PIPEMON_INTERVAL")) != NULL) {
        long int v;
        char *endptr;

        v = strtol(s, &endptr, 10);
        if ((*endptr != '\0') || (s == endptr) || (v < 0)) {
            fprintf(stderr, "Invalid PIPEMON_INTERVAL=%s\n", s);
            exit(EXIT_FAILURE);
        } else {
            interval = (unsigned int) v;
        }
    }

    if ((s = getenv("PIPEMON_TIMEOUT")) != NULL) {
        long int v;
        char *endptr;

        v = strtol(s, &endptr, 10);
        if ((*endptr != '\0') || (s == endptr) || (v < 0)) {
            fprintf(stderr, "Invalid PIPEMON_TIMEOUT=%s\n", s);
            exit(EXIT_FAILURE);
        } else {
            timeout = (time_t) v;
        }
    }

    if ((s = getenv("PIPEMON_THRESHOLD_RTT")) != NULL) {
        double v, x, y;
        char *endptr;

        v = strtod(s, &endptr);
        if ((*endptr != '\0') || (s == endptr) || (v < 0.000001)) {
            fprintf(stderr, "Invalid PIPEMON_THRESHOLD_RTT=%s\n", s);
            exit(EXIT_FAILURE);
        } else {
            y = modf(v, &x);
            modf(y * 1000000, &y);
            printf("y=%f\n", y);
            threshold_rtt.tv_sec = (time_t) x;
            threshold_rtt.tv_usec = (suseconds_t) y;
        }
    }

    if ((s = getenv("PIPEMON_THRESHOLD_COUNT")) != NULL) {
        long int v;
        char *endptr;

        v = strtol(s, &endptr, 10);
        if ((*endptr != '\0') || (s == endptr) || (v < 0)) {
            fprintf(stderr, "Invalid PIPEMON_TIMEOUT=%s\n", s);
            exit(EXIT_FAILURE);
        } else {
            threshold_count = (unsigned int) v;
        }
    }

    fprintf(stderr, "interval: %u\n", interval);
    fprintf(stderr, "timeout:  %zu\n", timeout);
    fprintf(stderr, "threshold_rtt:   %lu.%lu\n",
            threshold_rtt.tv_sec, threshold_rtt.tv_usec);
    fprintf(stderr, "threshold_count: %u\n", threshold_count);

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
            struct timeval t_send;
            struct timeval rtt;

            sleep(interval);

            gettimeofday(&t_send, NULL);

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
            hours %= 24;
            days = conntime / 86400;

            if (now.tv_usec < t_send.tv_usec) {
                int nsec = (t_send.tv_usec - now.tv_usec) / 1000000 + 1;
                t_send.tv_usec -= 1000000 * nsec;
                t_send.tv_sec += nsec;
            }

            if (now.tv_usec - t_send.tv_usec > 1000000) {
                int nsec = (now.tv_usec - t_send.tv_usec) / 1000000;
                t_send.tv_usec += 1000000 * nsec;
                t_send.tv_sec -= nsec;
            }

            rtt.tv_sec = now.tv_sec - t_send.tv_sec;
            rtt.tv_usec = now.tv_usec - t_send.tv_usec;

            if (threshold_count != 0) {
                if ((rtt.tv_sec > threshold_rtt.tv_sec) ||
                    ((rtt.tv_sec == threshold_rtt.tv_sec) &&
                     (rtt.tv_usec > threshold_rtt.tv_usec))) {
                    violate_rtt++;
                } else {
                    violate_rtt = 0;
                }

                if (violate_rtt > threshold_count) {
                    ret = -1;
                    goto done;
                }
            }

            if (days > 0) {
                printf("alive: %lud:%.2lu:%.2lu:%.2lu (rtt=%lu.%.6lus)\n",
                       days, hours, mins, secs, rtt.tv_sec, rtt.tv_usec);
            } else {
                printf("alive: %.2lu:%.2lu:%.2lu (rtt=%lu.%.6lus)\n",
                       hours, mins, secs, rtt.tv_sec, rtt.tv_usec);
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
