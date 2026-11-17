#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>

struct job
{
  struct job* next;
  int job_id;
};

/* A linked list of pending jobs.  */
struct job* job_queue;

/*extern void process_job (struct job*); */
void process_job (struct job* next_job)
{
  printf("Thread %lu is processing job %d\n", (unsigned long)pthread_self(), next_job->job_id);
  sleep(1);
}

/* A mutex protecting job_queue.  */
pthread_mutex_t job_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Process queued jobs until the queue is empty.  */

void* thread_function (void* arg)
{
  while (1) {
    struct job* next_job;

    /* Lock the mutex on the job queue.  */
    pthread_mutex_lock (&job_queue_mutex);
    /* Now it's safe to check if the queue is empty.  */
    if (job_queue == NULL)
      next_job = NULL;
    else {
      /* Get the next available job.  */
      next_job = job_queue;
      /* Remove this job from the list.  */
      job_queue = job_queue->next;
    }
    /* Unlock the mutex on the job queue, since we're done with the
       queue for now.  */
    pthread_mutex_unlock (&job_queue_mutex);

    /* Was the queue empty?  If so, end the thread.  */
    if (next_job == NULL)
      break;

    /* Carry out the work.  */
    process_job (next_job);
    /* Clean up.  */
    free (next_job);
  }
  return NULL;
}

int main ()
{
  int i;
  pthread_t threads[2];

  for (i = 1; i <= 6; i++)
  {
    struct job* new_job = (struct job*)malloc(sizeof(struct job));
    new_job->job_id = i;
    new_job->next = job_queue;
    job_queue = new_job;
  }

  for (i = 0; i < 2; i++)
  {
    pthread_create(&threads[i], NULL, thread_function, NULL);
  }

  for (i = 0; i < 2; i++)
  {
    pthread_join(threads[i], NULL);
  }
  return 0;
}
