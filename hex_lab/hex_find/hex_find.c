#include <dirent.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
// Usage:
// jekfind [start_dir] [options]
// Examples:
// jekfind . (print everything in working directory, and in those contained directories recursively)
// jekfind . -n foo (print any file or directory whose name is foo no case sensitivity)
// jekfind . -c foo (print any file or directory whose name contains the string foo)
// jekfind . -t f (same as [jekfind .], but only prints files)
// jekfind . -t d (same as [jekfind .], but only prints directories)
// -t and -n/-c can be combined in any order

typedef struct {
    int name;
    char *name_string;
    int contains;
    char *contains_string;
    int file;
    int directory;
    char *start_dir;
} Flags;

typedef struct Dir_Node {
    char path[PATH_MAX];
    struct Dir_Node *next;
} Dir_Node;

typedef struct {
    // pthread lock and conditions
    pthread_mutex_t lock;
    pthread_cond_t job_ready;
    pthread_cond_t traversal_complete;

    // FIFO Queue (linked list)
    Dir_Node *head;
    Dir_Node *tail;

    // Worker Flags
    int active_workers;
    int job_count;
    int stop;

    // Worker Context (for error handling)
    int worker_err;
} Job_Pool;

// DOES NOT INFLUENCE MUTEX, THREAD SAFETY IS MANTAINED BY WORKERS
Dir_Node *get_job(Job_Pool *pool) {
    if (pool->head == NULL) {
        return NULL;
    }
    Dir_Node *job = pool->head;
    // if the head node is the last element in the queue
    // we just want to set head to NULL after taking the job
    if (pool->head->next != NULL) {
        pool->head = pool->head->next;
    } else {
        pool->head = NULL;
        pool->tail = NULL;
    }

    return job;
}

// DOES NOT INFLUENCE MUTEX, THREAD SAFETY IS MANTAINED BY WORKERS
int add_job(Job_Pool *pool, char *path) {
    // When we add jobs lets just put them as the tail of the Linked List
    // Then when we pop a job we just take the head
    Dir_Node *new_job = (Dir_Node *)malloc(sizeof(Dir_Node));
    if (new_job == NULL) {
        perror("error in add_job: ");
        return -1;
    }
    // String copy does not add null terminator for some reason??
    strncpy(new_job->path, path, PATH_MAX - 1);
    new_job->path[PATH_MAX - 1] = '\0';
    new_job->next = NULL;

    // Add new job to the end of the FIFO Linked List Queue

    // if queue is empty:
    if (pool->head == NULL) {
        pool->head = new_job;
        pool->tail = new_job;
    } else {
        pool->tail->next = new_job;
        pool->tail = new_job;
    }

    return 0;
}

void *run_worker(void *arg) {
    Job_Pool *pool = (Job_Pool *)arg;
    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (pool->job_count == 0 && !pool->stop) {
            int worker_wait = pthread_cond_wait(&pool->job_ready, &pool->lock);
            if (worker_wait != 0) {
                pool->worker_err = worker_wait;
                pool->stop = 1;
                pthread_mutex_unlock(&pool->lock);
                return NULL;
            }
        }
        // If we get here the worker has been woken up
        // we need to check if its for a shutdown, or for a job
        if (pool->stop) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }

        // if we get here it means the worker was woken for a job

        // pull the job off of the queue (then we can unlock the mutex so we don't block work while
        // we do this job)
        Dir_Node *job = get_job(pool);
        pthread_mutex_unlock(&pool->lock);

        DIR *dir = opendir(job->path);
        if (dir == NULL) {
            // CHECK WHAT HAPPENS IF WE OPEN AN EMPTY DIR
        }
    }
}
void pool_destructor(Job_Pool *pool) {
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->job_ready);
    pthread_cond_destroy(&pool->traversal_complete);

    // Walk the linked list freeing all of the jobs
    // (if there are any)
    Dir_Node *curr = pool->head;
    while (curr != NULL) {
        Dir_Node *next = curr->next;
        free(curr);
        curr = next;
    }
    pool->head = NULL;
    pool->tail = NULL;

    free(pool);
}

Job_Pool *init_pool(void) {
    Job_Pool *job_pool = (Job_Pool *)(malloc(sizeof(Job_Pool)));
    pthread_mutex_init(&job_pool->lock, NULL);
    pthread_cond_init(&job_pool->job_ready, NULL);
    pthread_cond_init(&job_pool->traversal_complete, NULL);

    job_pool->head = NULL;
    job_pool->tail = NULL;

    job_pool->active_workers = 0;
    job_pool->job_count = 0;
    job_pool->stop = 0;

    job_pool->worker_err = 0;

    return job_pool;
}

// Create a worker for each running core on the cpu
// This is currently just a linux implementation
int init_workers(Job_Pool *pool) {
    int num_cpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[num_cpu];

    for (int i = 0; i < num_cpu; i++) {
        int create_thread = pthread_create(&threads[i], NULL, run_worker, pool);
        if (create_thread != 0) {
            perror("Error Creating Thread");
            return 1;
        }
    }

    for (int i = 0; i < num_cpu; i++) {
        pthread_join(threads[i], NULL);
    }
    // After all workers have returned it is safe to DESTROY THE POOl!
    pool_destructor(pool);

    return 0;
}

int handle_flags(int argc, char *argv[], Flags *flags) {
    int opt;
    while ((opt = getopt(argc, argv, "n:c:t:")) != -1) {
        switch (opt) {
        case 'n':
            flags->name = 1;
            flags->name_string = optarg;
            break;
        case 'c':
            flags->contains = 1;
            flags->contains_string = optarg;
            break;
        case 't':
            if (optarg[0] == 'f') {
                flags->file = 1;
            } else if (optarg[0] == 'd') {
                flags->directory = 1;
            } else {
                fprintf(stderr,
                        "Invalid argument for -t (f: files, d: directories)\n"
                        "do not use -t to get both: %s\n",
                        optarg);
                return -1;
            }
            break;
        default:
            fprintf(stderr, "Usage: jekfind [start_dir] [options]\n");
            return -1;
        }
    }
    if ((flags->file == 0) && (flags->directory == 0)) {
        flags->file = 1;
        flags->directory = 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    fprintf(stdout, "hello jacob");
    if (argc < 2) {
        fprintf(stderr, "USAGE: jekfind [start_dir] [options]");
        return 1;
    }
    // Initialize the flags struct with 0 values
    Flags *flags = calloc(1, sizeof(Flags));
    handle_flags(argc, argv, flags);

    return 0;
}
