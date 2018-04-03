#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define PATH_MAX 300

#define UID 1
#define GID 2
#define BOTH 3

int verify_range(int which, int reqstart, int reqrange)
{
	int ret;
	char path[PATH_MAX];
	char line[400];
	uid_t me = getuid();
	FILE *fin;

	if (me == 0)
		return 1;
	ret = snprintf(path, PATH_MAX, "/etc/id_permission/%cids", which == UID ? 'u' : 'g');
	if (ret < 0 || ret >= PATH_MAX)
		return -1;
	fin = fopen(path, "r");
	if (!fin)
		return -1;
	while (fgets(line, 400, fin) ) {
		if (*line == '#')  // comment
			continue;
		char *idx = strchr(line, ':');
		if (!idx)
			continue;
		*idx = '\0';
		int id = atoi(line);
		if (id != me)
			continue;
		char *end = strchr(idx+1, ':');
		if (!end)
			continue;
		*end = '\0';
		int authstart = atoi(idx+1);
		int authend = atoi(end+1);
		if (authstart > authend)  // nonsense
			continue;
		if (authstart > reqstart)  // authorized range starts too high
			continue;
		if (authend < reqstart+reqrange) // authorized range doesn't go high enough
			continue;
#if 0
		printf("reqstart %d reqrange %d authstart %d authend %d\n",
			reqstart, reqrange, authstart, authend);
#endif
		/* found a validating entry */
		fclose(fin);
		return 0;
	}
	fclose(fin);
	return -1;
}

int add_uid_mapping(pid_t pid, uid_t host_start, uid_t ns_start, int range)
{
	char path[PATH_MAX];
	int ret;
	FILE *f;

	ret = snprintf(path, PATH_MAX, "/proc/%d/uid_map", pid);
	if (ret < 0 || ret >= PATH_MAX) {
		fprintf(stderr, "%s: path name too long", __func__);
		return -E2BIG;
	}
	f = fopen(path, "w");
	if (!f) {
		perror("open");
		return -EINVAL;
	}
	ret = fprintf(f, "%d %d %d", ns_start, host_start, range);
	if (ret < 0)
		perror("write");
	fclose(f);
	return ret < 0 ? ret : 0;
}

int add_gid_mapping(pid_t pid, uid_t host_start, uid_t ns_start, int range)
{
	char path[PATH_MAX];
	int ret;
	FILE *f;

	ret = snprintf(path, PATH_MAX, "/proc/%d/gid_map", pid);
	if (ret < 0 || ret >= PATH_MAX) {
		fprintf(stderr, "%s: path name too long", __func__);
		return -E2BIG;
	}
	f = fopen(path, "w");
	if (!f) {
		perror("open");
		return -EINVAL;
	}
	ret = fprintf(f, "%d %d %d", ns_start, host_start, range);
	if (ret < 0)
		perror("write");
	fclose(f);
	return ret < 0 ? ret : 0;
}

void usage(char *me, int help) {
	printf("Usage: %s help\n", me);
	printf("Usage: %s pid rangespec\n", me);
	printf("Usage: %s pid uid rangespec (map only uids)\n", me);
	printf("Usage: %s pid gid rangespec (map only gids)\n", me);
	if (!help)
		exit(1);
	printf(" rangespec can be:\n");
	printf("  hostuid - just map uid 0 in the namespace to hostuid on the host\n");
	printf("  hostuid <num> - map (0,<num>) in ns to (hostuid,hostuid+num) on host\n");
	printf("  hostuid nsuid <num> - map (<nsuid>,<num>) in ns to (hostuid,hostuid+num) on host\n");
	printf("\n");
	printf("Examples:\n");
	printf("  %s 9999 55550 - uid and gid 0 in pid 9999's ns map to 55550 on host\n", me);
	printf("  %s uid 55550 10 - map uids 0-10 in pid 9999's ns to 55550-55560 on host\n", me);
	printf("  %s gid 55550 1000 10 - map gids 1000-1010 in ns to 55550-55560 on host\n", me);
	exit(0);
}

int main(int argc, char *argv[])
{
	int arg = 1;
	int which = BOTH;
	int i;
	pid_t pid;
	uid_t nsid = 0;
	int range = 1;
	uid_t hostid;

	if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
		usage(argv[0], 1);
	
	if (argc < 3)
		usage(argv[0], 0);

	pid = atoi(argv[arg++]);
	if (!strcmp(argv[arg], "uid")) {
		which = UID;
		arg++;
	} else if (!strcmp(argv[arg], "gid")) {
		which = GID;
		arg++;
	}

	if (arg >= argc)
		usage(argv[0], 0);

	hostid = atoi(argv[arg++]);

	if (arg <= argc-2)
		nsid = atoi(argv[arg++]);

	if (arg <= argc-1)
		range = atoi(argv[arg++]);

#if 0
	printf("I got: which %d hostid %d nsid %d range %d\n\n",
		which, hostid, nsid, range);
#endif
	
	if (argc > arg) {
		printf("Unknown args: ");
		for (i = arg; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");
		usage(argv[0], 0);
	}

	if (which == GID || which == BOTH) {
		if (verify_range(GID, hostid, range) < 0) {
			fprintf(stderr, "GID range not permitted for uid %d\n", getuid());
			exit(1);
		}
		add_gid_mapping(pid, hostid, nsid, range);
	}

	if (which == UID || which == BOTH) {
		if (verify_range(UID, hostid, range) < 0) {
			fprintf(stderr, "UID range not permitted for uid %d\n", getuid());
			exit(1);
		}
		add_uid_mapping(pid, hostid, nsid, range);
	}
	return 0;
}
