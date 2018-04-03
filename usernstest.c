#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

int unshare(int flags);
int clone(int (*fn)(void *), void *child_stack,
        int flags, void *arg, ...
        /* pid_t *ptid, struct user_desc *tls, pid_t *ctid */ );

#ifndef CLONE_NEWUSER
#define CLONE_NEWUSER		0x10000000	/* New user namespace */
#endif

int parentpid;
int tochild[2], fromchild[2];
int wassignaled  = 0;
void sighandler(int sig) {
    wassignaled = 1;
}

int uidmap(int pid, int mapped) {
    char pathname[1024];
    int ret;
    FILE *f;

    snprintf(pathname, 1024, "/proc/%d/uid_map", pid);
    f = fopen(pathname, "w");
    if (!f)
        return 0;
    ret = fprintf(f, "0 %d 1", mapped);
    fclose(f);
    if (ret < 5)
        return 0;
    snprintf(pathname, 1024, "/proc/%d/gid_map", pid);
    f = fopen(pathname, "w");
    if (!f)
        return 0;
    ret = fprintf(f, "0 %d 1", mapped);
    fclose(f);
    if (ret < 5)
        return 0;
    return 1;
}

#if STRICT
#define myassert(what, why) if (!what) { printf(why); exit(1); }
#else
#define myassert(what, why) if (!what) { printf(why); }
#endif

void writesync(int *p) {
    write(p[1], "go", 2);
}

void readsync(int *p) {
    char buf[3];
    read(p[0], buf, 2);
}

void touch(char *p)
{
    FILE *f = fopen(p, "w");
    if (!f)
        return;
    fprintf(f, "hi");
    fclose(f);
}

int child(void *data) {
    struct stat mystat;
    int ret;
    FILE *f;

    writesync(fromchild); // sync 1
    readsync(tochild); // sync 2
    setgid(0);
    setuid(0);
    ret = stat("/tmp/.usernstest4321", &mystat);
    printf("child saw %d for parent file owner\n", mystat.st_uid);
#if 0
    myassert((mystat.st_uid == -1), "child saw wrong owner for parent file\n");
#else
    myassert((mystat.st_uid == 0), "child saw wrong owner for parent file\n");
#endif
    f = fopen("/tmp/.usernstest4321", "w");
    myassert((f == NULL), "child was able to open parent's file\n");
    f = fopen("/tmp/.usernstest1234", "w");
    myassert((f != NULL), "child was not able to open his own file\n");
    ret = fprintf(f, "hi");
    fclose(f);
    myassert((ret > 0), "child was not able to write to his own file\n");
    writesync(fromchild); // sync 3
    readsync(tochild); // sync 4
    kill(parentpid, SIGUSR1);
    writesync(fromchild); // sync 5
    readsync(tochild); // sync  - never happens, should get killed
    exit(0);
}

int main(int argc, char *argv[])
{
    void *clone_stack, *page;
    int pagesize;
    int mapped;
    struct stat mystat;
    int pid;
    int ret;
    int status;

    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        printf("usage: %s [mappeduid]\n", argv[0]);
        exit(1);
    }
    mapped = atoi(argv[1]);

    parentpid = getpid();
    if (getuid() != 0) {
        printf("run as root (uidmap requires it)");
        exit(1);
    }
    unlink ("/tmp/.usernstest4321");
    unlink ("/tmp/.usernstest1234");

    if (pipe(tochild) == -1)
        exit(1);
    if (pipe(fromchild) == -1)
        exit(1);

    pagesize = 16*getpagesize();
    page = malloc(pagesize);
    if (!page) {
            perror("malloc");
            exit(-1);
    }
    clone_stack = page + pagesize;

    pid = clone(child, clone_stack, CLONE_NEWUSER | SIGCHLD, NULL);
    if (pid < 0) {
        perror("clone");
        exit(1);
    }

    readsync(fromchild); // sync 1
    if (!uidmap(pid, mapped)) {
        printf("failed at uidmap\n");
        exit(1);
    }
    touch ("/tmp/.usernstest4321");
    writesync(tochild); // sync 2
    readsync(fromchild); // sync 3
    ret = stat("/tmp/.usernstest1234", &mystat);
    if (ret < 0)
        perror("stat");
    myassert(mystat.st_uid == mapped, "parent sees wrong uid on child file\n");
    signal(SIGUSR1, sighandler);
    writesync(tochild); // sync 4
    readsync(fromchild); //sync 5

    myassert((!wassignaled), "child was able to signal parent\n");
    kill(pid, SIGTERM);
    writesync(tochild); // in case our kill didn't work
    waitpid(pid, &status, 0);
    myassert(WIFSIGNALED(status), "parent could not signal child\n");
    printf("all testcases passed\n");
    exit(0);
}
