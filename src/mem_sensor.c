#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sysmon_ipc.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    keep_running = 0;
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    int write_fd = STDOUT_FILENO;
    if (argc > 1)
    {
        write_fd = atoi(argv[1]);
    }

    while (keep_running)
   {
        FILE *fp = fopen("/proc/meminfo", "r");
        if (fp != NULL)
        {
            MemData data;
            memset(&data, 0, sizeof(MemData));
            char line[256];

            while (fgets(line, sizeof(line), fp))
            {
                sscanf(line, "MemTotal: %ld kB", &data.mem_total);
                sscanf(line, "MemFree: %ld kB", &data.mem_free);
                sscanf(line, "MemAvailable: %ld kB", &data.mem_avail);
                sscanf(line, "SwapTotal: %ld kB", &data.swap_total);
                sscanf(line, "SwapFree: %ld kB", &data.swap_free);
            }
            fclose(fp);

            if (write(write_fd, &data, sizeof(MemData)) != sizeof(MemData))
            {
                break;
            }
        }
        sleep(1);
    }

    close(write_fd);
    return 0;
}

