#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define INTERVAL_SEC 2
#define LOADAVG_PATH "/proc/loadavg"
#define OUTPUT_DIR   "/tmp/sysmon"
#define OUTPUT_FILE  "/tmp/sysmon/cpu.dat"

int collect_cpu_metrics(void)
{
	FILE *fp_in = NULL;
	FILE *fp_out = NULL;
	char line[256];
	int err = 0;

	float load_1 = 0.0f;
	float load_5 = 0.0f;
	float load_15 = 0.0f;

	fp_in = fopen(LOADAVG_PATH, "r");
	if (!fp_in)
	{
		perror("fopen /proc/loadavg");
		return -errno;
	}

	if (fgets(line, sizeof(line), fp_in) == NULL)
	{
		perror("fgets /proc/loadavg");
		err = -EIO;
		goto close_in;
	}

	if (sscanf(line, "%f %f %f", &load_1, &load_5, &load_15) != 3)
	{
		fprintf(stderr, "Error parsing /proc/loadavg\n");
		err = -EINVAL;
		goto close_in;
	}

	fp_out = fopen(OUTPUT_FILE, "w");
	if (!fp_out)
	{
		perror("fopen /tmp/sysmon/cpu.dat");
		err = -errno;
		goto close_in;
	}

	fprintf(fp_out, "CPU Load (1m, 5m, 15m): %.2f, %.2f, %.2f\n",
		load_1, load_5, load_15);

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
		collect_cpu_metrics();
		sleep(INTERVAL_SEC);
	}

	return 0;
}
