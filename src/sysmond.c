#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define INTERVAL_SEC 2
#define OUTPUT_DIR   "/tmp/sysmon"
#define MEM_FILE    "/tmp/sysmon/mem.dat"
#define CPU_FILE     "/tmp/sysmon/cpu.dat"
#define PROC_FILE    "/tmp/sysmon/proc.dat"


void print_sensor_file(const char *title, const char *filepath)
{
	FILE *fp = NULL;
	char line[256];

	printf(" %s \n", title);

	fp = fopen(filepath, "r");
	if (!fp)
	{
		printf("Waiting for data from %s.\n", filepath);
		return;
	}

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		printf(" %s", line);
	}
	fclose(fp);
}

void generate_report(void)
{
	time_t now = time(NULL);
	char *time_str = ctime(&now);

	printf("! SYSMON - SYSTEM STATUS REPORT !\n");
	if (time_str)
	{
		printf("Time: %s", time_str);
	}
	print_sensor_file("Memory Metrics", MEM_FILE);
	print_sensor_file("CPU Load Averages", CPU_FILE);
	print_sensor_file("Process Statistics", PROC_FILE);

	fflush(stdout);
}

int main(void)
{
	if (mkdir(OUTPUT_DIR, 0777) == -1 && errno != EEXIST)
	{
		perror("mkdir /tmp/sysmon");
		return EXIT_FAILURE;
	}

	while (1)
	{
		generate_report();
		sleep(INTERVAL_SEC);
	}
	return EXIT_SUCCESS;
}
