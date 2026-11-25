#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sysmon_ipc.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    keep_running = 0;
}

static unsigned int parse_interval(int argc, char *argv[], int idx, unsigned int fallback)
{
    if (argc > idx && argv[idx] != NULL)
    {
        char *endptr = NULL;
        long val = strtol(argv[idx], &endptr, 10);
        if (endptr != argv[idx] && *endptr == '\0' && val > 0 && val <= 86400)
        {
            return (unsigned int)val;
        }
    }
    return fallback;
}

int main(int argc, char *argv[])
{
    unsigned int interval = parse_interval(argc, argv, 1, 2);

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    int fifo_fd = open(CPU_FIFO_PATH, O_WRONLY);
    if (fifo_fd < 0)
    {
        perror("cpu_sensor: open FIFO failed");
        return 1;
    }

    while (keep_running)
    {
        FILE *fp = fopen("/proc/loadavg", "r");
        if (fp != NULL)
        {
            CpuData data;
            memset(&data, 0, sizeof(CpuData));

            if (fscanf(fp, "%lf %lf %lf", &data.load1, &data.load5, &data.load15) == 3)
            {
                if (write(fifo_fd, &data, sizeof(CpuData)) != sizeof(CpuData))
                {
                    fclose(fp);
                    break;
                }
            }
            fclose(fp);
        }
        sleep(interval);
    }

    close(fifo_fd);
    return 0;
}
