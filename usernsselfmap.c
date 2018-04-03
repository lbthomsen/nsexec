#include <stdio.h>
#include <sched.h>
#include <linux/sched.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>

int unshare(int flags);

int writemaps(pid_t pid, int origuid, int origgid)
{
	FILE *fout;
	char path[1024];
	int ret;

	printf("starting from uid %d gid %d\n", origuid, origgid);
	snprintf(path, 1024, "/proc/%d/uid_map", pid);
	fout = fopen(path, "w");
	ret = fprintf(fout, "0 %d 1\n", origuid);
	if (ret < 0) {
		perror("writing uidmap\n");
		return -1;
	}
	ret = fclose(fout);
	if (ret < 0) {
		perror("closing uidmap\n");
		return -1;
	}

	snprintf(path, 1024, "/proc/%d/gid_map", pid);
	fout = fopen(path, "w");
	ret = fprintf(fout, "0 %d 1\n", origgid);
	if (ret < 0) {
		perror("writing gidmap\n");
		return -1;
	}
	ret = fclose(fout);
	if (ret < 0) {
		perror("closing gidmap\n");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char *args[] = { "/bin/bash", NULL };
	int ret;
	int origuid = getuid();
	int origgid = getgid();

	ret = unshare(CLONE_NEWUSER);
	ret = writemaps(getpid(), origuid, origgid);
	if (ret < 0) {
		printf("Error writing maps\n");
		exit(1);
	}
	if (ret < 0) {
		perror("unshare");
		exit(1);
	}
	ret = setgid(0);
	if (ret < 0)
		perror("setgid");
	ret = setuid(0);
	if (ret < 0)
		perror("setuid");
	ret = setgroups(0, NULL);
	if (ret < 0)
		perror("setgroups");
	printf("execing bash (I am  now %d %d)\n", getuid(), getgid());
	execv(args[0], args);
    exit(1);
}
