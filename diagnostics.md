# Phase 3 Forensic Lab Report — Linux IPC & `sysmond` Supervisor

## Prerequisites Check
- Clean compilation verified with `make clean && make` without warnings and clean runtime environment prior to testing.

## Experiment 1: FDs, Pipes & FIFOs
- **1A / 1B:** [הכנס כאן את התשובות שלך לשאלות 1A ו-1B]
- *Screenshot:* [צרף כאן את צילומי `ps -ef` ו-`/proc/<pid>/fd`]

## Experiment 2: FIFO Rendezvous Blocking & Kernel Wait Channel (`WCHAN`)
- **Question 2A:** `sysmond` sleeps in kernel function `wait_for_partner` due to the blocking `open(fifo_path, O_RDONLY)` call inside `open_nonblocking_fifo()` in `src/sysmond.c` (called from `sysmon_init()` for `CPU_FIFO_PATH`) prior to enabling `O_NONBLOCK`.
- **Question 2B:** `mem_sensor` and `proc_sensor` were spawned before `sysmond` reached the blocking `open()` on `CPU_FIFO_PATH`; `mem_sensor` sleeps in `hrtimer_nanosleep` between writes to its anonymous pipe, and `proc_sensor` waits in `wait_for_partner` for a reader on `PROC_FIFO_PATH`, so neither is blocked by `sysmond` waiting on `CPU_FIFO_PATH`.
- **Question 2C:** Default pipe buffer size is `65,536` bytes (`64 KiB`), and `sizeof(MemData)` is `40` bytes (`5 * 8` bytes on x86_64). At most floor(65536 / 40) = 1,638 writes fit before blocking, which at 1 write per second takes **1,638 seconds**.
- *Screenshot:* [צרף כאן את צילום המסך של `sysmond` התקוע ו-`ps -eo pid,stat,wchan:20,comm`]

## Experiment 3: Mid-Stream Failure (EOF vs. EPIPE / SIGPIPE)
- **Question 3:**
  - **Part A (`EOF`):** Killing `proc_sensor` (`SIGKILL`) closes its write end on `PROC_FIFO_PATH`; once no writers remain open, `read()` in `sysmond` returns `0` (`EOF`), causing `drain_fd()` to return `0` and `sysmond` to close and clear that channel's fd.
  - **Part B (`EPIPE` / `SIGPIPE`):** Killing `sysmond` (`SIGKILL`) closes the read end of the anonymous pipe; on its next write, `mem_sensor`'s `write()` fails with `-1` (`errno = EPIPE`) and the kernel delivers `SIGPIPE`.
  - **FIFO persistence vs. anonymous pipe:** Anonymous pipes exist only in kernel memory with no filesystem entry and vanish when all FDs close, whereas FIFOs (`/tmp/sysmon_*.fifo`) are special filesystem nodes created by `mkfifo()` and removed only by `unlink()` in `sysmon_teardown()` (skipped on `SIGKILL`).
- *Screenshots:* [צרף כאן את צילומי Part A (`EOF`), Part B (`strace` עם `EPIPE`/`SIGPIPE` ו-`ls -l /tmp/sysmon_*.fifo`)]

## Experiment 4: Zombie Process Inspection (`waitpid` Omission)
- **Question 4:**
  - When `cpu_sensor` terminates while `sysmond` is still running without reaping, the kernel frees its address space, open files, and other resources, but retains a minimal `task_struct` entry (PID and exit status) so the parent can retrieve termination status and to prevent immediate PID reuse before collection.
  - `waitpid(ctx->pids[i], NULL, 0)` in `sysmon_teardown()` collects the child's exit status from the kernel, allowing the kernel to reap the zombie entry and free the process table slot and PID.
- *Screenshots:* [צרף כאן את צילומי מצב `Z` ב-`ps` והניקוי אחרי `Ctrl+C`]
