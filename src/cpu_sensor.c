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

int main(void)
{
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
        sleep(1);
    }

    close(fifo_fd);
    return 0;
}
