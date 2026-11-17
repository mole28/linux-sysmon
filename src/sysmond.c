#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <signal.h>
#include <sysmon.h>

static volatile sig_atomic_t keep_running = 1;
static SystemState sys_state;

void handle_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
    pthread_cond_broadcast(&sys_state.alert_cond);
}

static int is_pid_dir(const char *name)
{
    if (!name || *name == '\0') return 0;
    while (*name)
    {
        if (!isdigit((unsigned char)*name)) return 0;
        ++name;
    }
    return 1;
}

void *mem_sensor_thread(void *arg)
{
    SystemState *state = (SystemState *)arg;

    while (keep_running)
    {
        FILE *fp = fopen("/proc/meminfo", "r");
        if (fp != NULL)
        {
            char line[256];
            long total = 0, free_mem = 0, avail = 0, s_total = 0, s_free = 0;


            while (fgets(line, sizeof(line), fp))
            {
                sscanf(line, "MemTotal: %ld kB", &total);
                sscanf(line, "MemFree: %ld kB", &free_mem);
                sscanf(line, "MemAvailable: %ld kB", &avail);
                sscanf(line, "SwapTotal: %ld kB", &s_total);
                sscanf(line, "SwapFree: %ld kB", &s_free);
            }
            fclose(fp);

            pthread_mutex_lock(&state->lock);
            state->mem_total = total;
            state->mem_free = free_mem;
            state->mem_avail = avail;
            state->swap_total = s_total;
            state->swap_free = s_free;
            pthread_mutex_unlock(&state->lock);
        }
        sleep(1);
    }
    return NULL;
}

void *cpu_sensor_thread(void *arg)
{
    SystemState *state = (SystemState *)arg;

    while (keep_running)
{
        FILE *fp = fopen("/proc/loadavg", "r");
        if (fp != NULL)
        {
            double l1 = 0.0, l5 = 0.0, l15 = 0.0;
            if (fscanf(fp, "%lf %lf %lf", &l1, &l5, &l15) == 3)
            {
                pthread_mutex_lock(&state->lock);
                state->load1 = l1;
                state->load5 = l5;
                state->load15 = l15;
                pthread_mutex_unlock(&state->lock);

                if (l1 > 1.50)
	        {
                    pthread_cond_signal(&state->alert_cond);
                }
            }
            fclose(fp);
        }
        sleep(1);
    }
    return NULL;
}

void *proc_sensor_thread(void *arg)
{
    SystemState *state = (SystemState *)arg;

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

            pthread_mutex_lock(&state->lock);
            state->total_procs = count;
            pthread_mutex_unlock(&state->lock);
        }
        sleep(1);
    }
    return NULL;
}

void *alert_thread(void *arg)
{
    SystemState *state = (SystemState *)arg;

    while (keep_running)
    {
        pthread_mutex_lock(&state->lock);
        pthread_cond_wait(&state->alert_cond, &state->lock);

        if (!keep_running)
	{
            pthread_mutex_unlock(&state->lock);
            break;
        }

        if (state->load1 > 1.50)
	{
            fprintf(stderr, "\033[1;31m[ALERT] High CPU Load detected! 1-min load average: %.2f (Threshold: 1.50)\033[0m\n", state->load1);
        }
        pthread_mutex_unlock(&state->lock);
    }
    return NULL;
}

int main(void)
{
    signal(SIGINT, handle_sigint);

    memset(&sys_state, 0, sizeof(SystemState));
    pthread_mutex_init(&sys_state.lock, NULL);
    pthread_cond_init(&sys_state.alert_cond, NULL);

    pthread_t mem_tid, cpu_tid, proc_tid, alert_tid;

    pthread_create(&mem_tid, NULL, mem_sensor_thread, &sys_state);
    pthread_create(&cpu_tid, NULL, cpu_sensor_thread, &sys_state);
    pthread_create(&proc_tid, NULL, proc_sensor_thread, &sys_state);
    pthread_create(&alert_tid, NULL, alert_thread, &sys_state);

    while (keep_running)
    {
        pthread_mutex_lock(&sys_state.lock);
        SystemState snapshot = sys_state;
        pthread_mutex_unlock(&sys_state.lock);

        printf("\033[2J\033[H");
        printf("SYSMOND - MULTI-THREADED MONITOR           \n");
        printf("[MEMORY]  Total: %ld kB | Free: %ld kB | Avail: %ld kB\n",
               snapshot.mem_total, snapshot.mem_free, snapshot.mem_avail);
        printf("[SWAP]    Total: %ld kB | Free: %ld kB\n",
               snapshot.swap_total, snapshot.swap_free);
        printf("[CPU]     Load Avg (1m / 5m / 15m): %.2f / %.2f / %.2f\n",
               snapshot.load1, snapshot.load5, snapshot.load15);
        printf("[PROCS]   Total Active Processes: %d\n",
               snapshot.total_procs);
        printf("Press Ctrl+C to stop...\n");

        sleep(2);
    }

    pthread_join(mem_tid, NULL);
    pthread_join(cpu_tid, NULL);
    pthread_join(proc_tid, NULL);
    pthread_join(alert_tid, NULL);

    pthread_mutex_destroy(&sys_state.lock);
    pthread_cond_destroy(&sys_state.alert_cond);

    printf("\nShutting down sysmond cleanly.\n");
    return 0;
}
