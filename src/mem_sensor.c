#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define INTERVAL_SEC 2
#define MEMINFO_PATH "/proc/meminfo"
#define OUTPUT_DIR   "/tmp/sysmon"
#define OUTPUT_FILE  "/tmp/sysmon/mem.dat"

int collect_mem_metrics(void)
{
	FILE *fp_in = NULL;
	FILE *fp_out = NULL;
	char line[256];
	int err = 0;

	unsigned long mem_total = 0;
	unsigned long mem_free = 0;
	unsigned long mem_available = 0;
	unsigned long swap_total = 0;
	unsigned long swap_free = 0;

	fp_in = fopen(MEMINFO_PATH, "r");
	if (!fp_in)
	{
		perror("fopen /proc/meminfo");
		return -errno;
	}

	while (fgets(line, sizeof(line), fp_in) != NULL)
	{
		sscanf(line, "MemTotal: %lu kB", &mem_total);
		sscanf(line, "MemFree: %lu kB", &mem_free);
		sscanf(line, "MemAvailable: %lu kB", &mem_available);
		sscanf(line, "SwapTotal: %lu kB", &swap_total);
		sscanf(line, "SwapFree: %lu kB", &swap_free);
	}

	fp_out = fopen(OUTPUT_FILE, "w");
	if (!fp_out)
	{
		perror("fopen /tmp/sysmon/mem.dat");
		err = -errno;
		goto close_in;
	}

	fprintf(fp_out, "MemTotal:     %lu kB\n", mem_total);
	fprintf(fp_out, "MemFree:      %lu kB\n", mem_free);
	fprintf(fp_out, "MemAvailable: %lu kB\n", mem_available);
	fprintf(fp_out, "SwapTotal:    %lu kB\n", swap_total);
	fprintf(fp_out, "SwapFree:     %lu kB\n", swap_free);

	fclose(fp_out);

close_in:
	fclose(fp_in);
	return err;
}

int main(void)
{
	mkdir(OUTPUT_DIR, 0777);

	while (1)
	{
		collect_mem_metrics();
		sleep(INTERVAL_SEC);
	}

	return 0;
}
