#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sysmon_ipc.h>

extern char **environ;

static volatile sig_atomic_t keep_running = 1;
static pid_t mem_pid = -1;
static pid_t cpu_pid = -1;
static pid_t proc_pid = -1;

static int mem_read_fd = -1;
static int cpu_fifo_fd = -1;
static int proc_fifo_fd = -1;

static void handle_termination(int sig)
{
    (void)sig;
    keep_running = 0;
}

static void cleanup_and_exit(void)
{
    pid_t pids[3] = {mem_pid, cpu_pid, proc_pid};

    for (int i = 0; i < 3; i++)
    {
        if (pids[i] > 0)
        {
            kill(pids[i], SIGTERM);
        }
    }

    for (int i = 0; i < 3; i++)
    {
        if (pids[i] > 0)
        {
            waitpid(pids[i], NULL, 0);
        }
    }

    if (mem_read_fd >= 0) close(mem_read_fd);
    if (cpu_fifo_fd >= 0) close(cpu_fifo_fd);
    if (proc_fifo_fd >= 0) close(proc_fifo_fd);

    unlink(CPU_FIFO_PATH);
    unlink(PROC_FIFO_PATH);
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_termination;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("sysmond: pipe failed");
        return 1;
    }

    mem_pid = fork();
    if (mem_pid < 0) {
        perror("sysmond: fork mem_sensor failed");
        return 1;
    } else if (mem_pid == 0)
	   {
               close(pipefd[0]);
               char fd_str[16];
               snprintf(fd_str, sizeof(fd_str), "%d", pipefd[1]);
               char *args[] = {"./bin/mem_sensor", fd_str, NULL};
               execve("./bin/mem_sensor", args, environ);
               perror("execve mem_sensor failed");
               _exit(1);
    }
    close(pipefd[1]);
    mem_read_fd = pipefd[0];

    unlink(CPU_FIFO_PATH);
    unlink(PROC_FIFO_PATH);

    if (mkfifo(CPU_FIFO_PATH, 0666) == -1 && errno != EEXIST)
    {
        perror("sysmond: mkfifo cpu failed");
        cleanup_and_exit();
        return 1;
    }
    if (mkfifo(PROC_FIFO_PATH, 0666) == -1 && errno != EEXIST)
    {
        perror("sysmond: mkfifo proc failed");
        cleanup_and_exit();
        return 1;
    }

    cpu_pid = fork();
    if (cpu_pid < 0)
    {
        perror("sysmond: fork cpu_sensor failed");
        cleanup_and_exit();
        return 1;
    } else if (cpu_pid == 0)
           {
                close(mem_read_fd);
                char *args[] = {"./bin/cpu_sensor", NULL};
                execve("./bin/cpu_sensor", args, environ);
                perror("execve cpu_sensor failed");
                _exit(1);
           }

    proc_pid = fork();
    if (proc_pid < 0)
    {
        perror("sysmond: fork proc_sensor failed");
        cleanup_and_exit();
        return 1;
    } else if (proc_pid == 0)
           {
             close(mem_read_fd);
             char *args[] = {"./bin/proc_sensor", NULL};
             execve("./bin/proc_sensor", args, environ);
             perror("execve proc_sensor failed");
            _exit(1);
           }

    cpu_fifo_fd = open(CPU_FIFO_PATH, O_RDONLY);
    if (cpu_fifo_fd < 0)
    {
        if (keep_running) perror("sysmond: open cpu FIFO failed");
        cleanup_and_exit();
        return 1;
    }

    proc_fifo_fd = open(PROC_FIFO_PATH, O_RDONLY);
    if (proc_fifo_fd < 0)
    {
        if (keep_running) perror("sysmond: open proc FIFO failed");
        cleanup_and_exit();
        return 1;
    }

    MemData mem = {0};
    CpuData cpu = {0};
    ProcData proc = {0};

    while (keep_running)
    {
        ssize_t m_bytes = read(mem_read_fd, &mem, sizeof(MemData));
        ssize_t c_bytes = read(cpu_fifo_fd, &cpu, sizeof(CpuData));
        ssize_t p_bytes = read(proc_fifo_fd, &proc, sizeof(ProcData));

        if (!keep_running || m_bytes <= 0 || c_bytes <= 0 || p_bytes <= 0)
        {
            break;
        }

        if (m_bytes == sizeof(MemData) && c_bytes == sizeof(CpuData) && p_bytes == sizeof(ProcData))
	{
            printf("\033[2J\033[H");
            printf("SYSMOND - SUPERVISED IPC MONITOR (PHASE 3) \n");
            printf("[SUPERVISOR] PIDs -> mem: %d | cpu: %d | proc: %d\n",
                   mem_pid, cpu_pid, proc_pid);
            printf("[MEMORY]  Total: %ld kB | Free: %ld kB | Avail: %ld kB\n",
                   mem.mem_total, mem.mem_free, mem.mem_avail);
            printf("[SWAP]    Total: %ld kB | Free: %ld kB\n",
                   mem.swap_total, mem.swap_free);
            printf("[CPU]     Load Avg (1m / 5m / 15m): %.2f / %.2f / %.2f\n",
                   cpu.load1, cpu.load5, cpu.load15);
            printf("[PROCS]   Total Active Processes: %d\n",
                   proc.total_procs);
            printf("Press Ctrl+C to terminate supervisor and all sensors...\n");
        }
    }
    cleanup_and_exit();
    printf("\nShutting down sysmond: children reaped and FIFOs unlinked.\n");
    return 0;
}
