#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sysmon_ipc.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    keep_running = 0;
}

static int is_pid_dir(const char *name)
{
    if (!name || *name == '\0') return 0;
    while (*name)
    {
        if (!isdigit((unsigned char)*name)) return 0;
        name++;
    }
    return 1;
}

static unsigned int get_interval(int argc, char *argv[], int idx, unsigned int def_val)
{
    if (argc > idx && argv[idx] != NULL)
    {
        long v = strtol(argv[idx], NULL, 10);
        if (v > 0 && v <= 86400)
        {
            return (unsigned int)v;
        }
    }
    return def_val;
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    unsigned int interval = get_interval(argc, argv, 1, 5);

    int fifo_fd = open(PROC_FIFO_PATH, O_WRONLY);
    if (fifo_fd < 0)
    {
        perror("proc_sensor: open FIFO failed");
        return 1;
    }

    while (keep_running)
    {
        DIR *dir = opendir("/proc");
        if (dir != NULL)
        {
            int count = 0;
            struct dirent *entry;

            while ((entry = readdir(dir)) != NULL)
            {
                if (is_pid_dir(entry->d_name))
                {
                    count++;
                }
            }
            closedir(dir);

            ProcData data;
            data.total_procs = count;

            if (write(fifo_fd, &data, sizeof(ProcData)) != sizeof(ProcData))
            {
                break;
            }
        }
        sleep(interval);
    }

    close(fifo_fd);
    return 0;
}
