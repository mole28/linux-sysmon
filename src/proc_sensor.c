#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define INTERVAL_SEC 2
#define PROC_PATH    "/proc"
#define OUTPUT_DIR   "/tmp/sysmon"
#define OUTPUT_FILE  "/tmp/sysmon/proc.dat"

int is_pid_folder(const char *name)
{
	if (!name || *name == '\0')
		return 0;

	while (*name)
	{
		if (!isdigit((unsigned char)*name))
			return 0;
		name++;
	}
	return 1;
}

int collect_proc_metrics(void)
{
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	FILE *fp_out = NULL;
	int proc_count = 0;
	int err = 0;

	dir = opendir(PROC_PATH);
	if (!dir)
	{
		perror("opendir /proc");
		return -errno;
	}

	while ((entry = readdir(dir)) != NULL)
	{
		if (is_pid_folder(entry->d_name))
		{
			proc_count++;
		}
	}

	fp_out = fopen(OUTPUT_FILE, "w");
	if (!fp_out)
	{
		perror("fopen /tmp/sysmon/proc.dat");
		err = -errno;
		goto close_dir;
	}

	fprintf(fp_out, "Active Processes: %d\n", proc_count);

	fclose(fp_out);

close_dir:
	closedir(dir);
	return err;
}

int main(void)
{
	mkdir(OUTPUT_DIR, 0777);

	while (1)
	{
		collect_proc_metrics();
		sleep(INTERVAL_SEC);
	}

	return 0;
}
