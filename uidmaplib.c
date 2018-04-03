#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int add_uid_mapping(pid_t pid, uid_t host_start, uid_t ns_start, int range)
{
	char *path[PATH_MAX];
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
	char *path[PATH_MAX];
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
