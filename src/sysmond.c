#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <sysmon_ipc.h>

extern char **environ;

enum
{
    CH_MEM = 0,
    CH_CPU,
    CH_PROC,
    CH_COUNT
};

struct sysmon_ctx
{
    struct pollfd fds[CH_COUNT];
    pid_t pids[CH_COUNT];
    MemData mem;
    CpuData cpu;
    ProcData proc;
    bool has_data[CH_COUNT];
};

static volatile sig_atomic_t keep_running = 1;

static void handle_termination(int sig)
{
    (void)sig;
    keep_running = 0;
}

static int setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_termination;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0)
    {
        perror("sysmond: sigaction failed");
        return -1;
    }
    return 0;
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        perror("sysmond: fcntl F_GETFL failed");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("sysmond: fcntl F_SETFL O_NONBLOCK failed");
        return -1;
    }
    return 0;
}

static pid_t spawn_mem_sensor(int *out_read_fd, const char *interval_arg)
{
    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        perror("sysmond: pipe failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("sysmond: fork mem_sensor failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        char fd_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", pipefd[1]);
        char *args[] = { "./bin/mem_sensor", fd_str, (char *)interval_arg, NULL };
        execve("./bin/mem_sensor", args, environ);
        perror("execve mem_sensor failed");
        _exit(1);
    }

    close(pipefd[1]);
    *out_read_fd = pipefd[0];
    return pid;
}

static pid_t spawn_fifo_sensor(const char *fifo_path, const char *bin_path,
                               const char *interval_arg, int inherited_fd_to_close)
{
    unlink(fifo_path);
    if (mkfifo(fifo_path, 0666) < 0 && errno != EEXIST)
    {
        perror("sysmond: mkfifo failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("sysmond: fork sensor failed");
        return -1;
    }

    if (pid == 0)
    {
        if (inherited_fd_to_close >= 0)
        {
            close(inherited_fd_to_close);
        }
        char *args[] = { (char *)bin_path, (char *)interval_arg, NULL };
        execve(bin_path, args, environ);
        perror("execve sensor failed");
        _exit(1);
    }

    return pid;
}

static int open_nonblocking_fifo(const char *fifo_path, const char *label)
{
    int fd = open(fifo_path, O_RDONLY);
    if (fd < 0)
    {
        if (keep_running)
        {
            fprintf(stderr, "sysmond: open %s FIFO failed: %s\n", label, strerror(errno));
        }
        return -1;
    }
    if (set_nonblocking(fd) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

static int drain_fd(int fd, void *dst, size_t item_size, bool *updated)
{
    unsigned char buf[1024];
    *updated = false;

    if (item_size > sizeof(buf))
    {
        return -1;
    }

    while (1)
    {
        ssize_t n = read(fd, buf, item_size);
        if (n == (ssize_t)item_size)
        {
            memcpy(dst, buf, item_size);
            *updated = true;
        }
        else if (n == 0)
        {
            return 0;
        }
        else if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 1;
            }
            return -1;
        }
        else
        {
            return -1;
        }
    }
}

static void render_dashboard(const struct sysmon_ctx *ctx)
{
    printf("\033[2J\033[H");
    printf("SYSMOND - SUPERVISED IPC MONITOR (PHASE 3)\n");
    printf("[SUPERVISOR] PIDs -> mem: %d | cpu: %d | proc: %d\n",
           ctx->pids[CH_MEM], ctx->pids[CH_CPU], ctx->pids[CH_PROC]);

    if (ctx->has_data[CH_MEM])
    {
        printf("[MEMORY]  Total: %ld kB | Free: %ld kB | Avail: %ld kB\n",
               ctx->mem.mem_total, ctx->mem.mem_free, ctx->mem.mem_avail);
        printf("[SWAP]    Total: %ld kB | Free: %ld kB\n",
               ctx->mem.swap_total, ctx->mem.swap_free);
    }
    else
    {
        printf("[MEMORY]  Waiting for first sample...\n");
    }

    if (ctx->has_data[CH_CPU])
    {
        printf("[CPU]     Load Avg (1m / 5m / 15m): %.2f / %.2f / %.2f\n",
               ctx->cpu.load1, ctx->cpu.load5, ctx->cpu.load15);
    }
    else
    {
        printf("[CPU]     Waiting for first sample...\n");
    }

    if (ctx->has_data[CH_PROC])
    {
        printf("[PROCS]   Total Active Processes: %d\n", ctx->proc.total_procs);
    }
    else
    {
        printf("[PROCS]   Waiting for first sample...\n");
    }

    printf("Press Ctrl+C to terminate supervisor and all sensors...\n");
    fflush(stdout);
}

static int sysmon_init(struct sysmon_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    for (int i = 0; i < CH_COUNT; i++)
    {
        ctx->fds[i].fd = -1;
        ctx->fds[i].events = POLLIN;
        ctx->pids[i] = -1;
    }

    if (setup_signals() < 0)
    {
        return -1;
    }

    int mem_fd = -1;
    ctx->pids[CH_MEM] = spawn_mem_sensor(&mem_fd, "1");
    if (ctx->pids[CH_MEM] < 0)
    {
        return -1;
    }
    if (set_nonblocking(mem_fd) < 0)
    {
        close(mem_fd);
        return -1;
    }
    ctx->fds[CH_MEM].fd = mem_fd;

    ctx->pids[CH_CPU] = spawn_fifo_sensor(CPU_FIFO_PATH, "./bin/cpu_sensor", "2", ctx->fds[CH_MEM].fd);
    if (ctx->pids[CH_CPU] < 0)
    {
        return -1;
    }

    ctx->pids[CH_PROC] = spawn_fifo_sensor(PROC_FIFO_PATH, "./bin/proc_sensor", "5", ctx->fds[CH_MEM].fd);
    if (ctx->pids[CH_PROC] < 0)
    {
        return -1;
    }

    ctx->fds[CH_CPU].fd = open_nonblocking_fifo(CPU_FIFO_PATH, "cpu");
    if (ctx->fds[CH_CPU].fd < 0)
    {
        return -1;
    }

    ctx->fds[CH_PROC].fd = open_nonblocking_fifo(PROC_FIFO_PATH, "proc");
    if (ctx->fds[CH_PROC].fd < 0)
    {
        return -1;
    }

    return 0;
}

static int sysmon_process(struct sysmon_ctx *ctx)
{
    void *targets[CH_COUNT] = { &ctx->mem, &ctx->cpu, &ctx->proc };
    size_t sizes[CH_COUNT] = { sizeof(MemData), sizeof(CpuData), sizeof(ProcData) };

    while (keep_running)
    {
        int ready = poll(ctx->fds, CH_COUNT, 1000);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("sysmond: poll failed");
            return -1;
        }
        if (ready == 0)
        {
            continue;
        }

        bool changed = false;
        for (int i = 0; i < CH_COUNT; i++)
        {
            if (ctx->fds[i].fd < 0 || ctx->fds[i].revents == 0)
            {
                continue;
            }

            if (ctx->fds[i].revents & POLLIN)
            {
                bool updated = false;
                int rc = drain_fd(ctx->fds[i].fd, targets[i], sizes[i], &updated);
                if (updated)
                {
                    ctx->has_data[i] = true;
                    changed = true;
                }
                if (rc <= 0)
                {
                    close(ctx->fds[i].fd);
                    ctx->fds[i].fd = -1;
                }
            }
            else if (ctx->fds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                close(ctx->fds[i].fd);
                ctx->fds[i].fd = -1;
            }
        }

        if (changed)
        {
            render_dashboard(ctx);
        }
    }

    return 0;
}

static void sysmon_teardown(struct sysmon_ctx *ctx)
{
    for (int i = 0; i < CH_COUNT; i++)
    {
        if (ctx->pids[i] > 0)
        {
            kill(ctx->pids[i], SIGTERM);
        }
    }
    for (int i = 0; i < CH_COUNT; i++)
    {
        if (ctx->pids[i] > 0)
        {
            waitpid(ctx->pids[i], NULL, 0);
            ctx->pids[i] = -1;
        }
        if (ctx->fds[i].fd >= 0)
        {
            close(ctx->fds[i].fd);
            ctx->fds[i].fd = -1;
        }
    }

    unlink(CPU_FIFO_PATH);
    unlink(PROC_FIFO_PATH);
}

int main(void)
{
    struct sysmon_ctx ctx;
    int status = 0;

    if (sysmon_init(&ctx) < 0)
    {
        status = 1;
        goto out;
    }

    if (sysmon_process(&ctx) < 0)
    {
        status = 1;
    }

out:
    sysmon_teardown(&ctx);
    if (status == 0)
    {
        printf("\nShutting down sysmond: children reaped and FIFOs unlinked.\n");
    }
    return status;
}
