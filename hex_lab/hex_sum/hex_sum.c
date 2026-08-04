#include "../../hex_lib/hex_debug/hex_debug.h"
#include <dirent.h>
#include <linux/limits.h>
#include <pthread.h>
#include <sodium.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct job {
    char path[PATH_MAX];
    struct job *next_job;
} job;

typedef struct {
    // Mutex Lock
    pthread_mutex_t lock;
    pthread_cond_t job_ready;
    pthread_cond_t traversal_complete;

    // Job Queue FIFO Linked-List, we will mantain a tail so that appending is easy for adding new
    // jobs.
    struct job *jq_head;
    struct job *jq_tail;

    // Thread info
    int threads_alive;
    int active_workers;
    int job_count;
    int worker_err;
    int shutdown;

} thread_manager;

// Function for threads to use, to add jobs to the thread manager's job queue
int add_job(char *path, thread_manager *tm) {
    debug("\nJob Added\nPath: %s", path);
    // Create new job node, make current job queue tail's next node be the new node
    // Then make this new node the tail of the job queue
    job *new_job = (job *)malloc(sizeof(job));
    debug("\nmalloc'd the new_job");
    // load the passed in path into new job node's path char array
    snprintf(new_job->path, PATH_MAX, "%s", path);
    // make sure new_job's next is NULL since malloc fills with garbage
    new_job->next_job = NULL;

    // Lock mutex before editing the thread manager.
    pthread_mutex_lock(&tm->lock);

    // make new_job the next_job of the current tail
    // make new job the new tail of the job queue
    if (tm->jq_head) {
        tm->jq_tail->next_job = new_job;
        tm->jq_tail = new_job;
    } else {
        tm->jq_head = new_job;
        tm->jq_tail = new_job;
    }

    // Unlock mutex
    pthread_mutex_unlock(&tm->lock);
    pthread_cond_broadcast(&tm->job_ready);
    debug("\nnew job broadcasted");

    return 0;
}

// Function for threads to use, takes a job off the thread worker queue
job *get_job(thread_manager *tm, int t_id) {
    debug("Thread (%d), has called get_job()", t_id);
    // Take the head of the queue (least recent job added), make the head's next the new head,
    // return pointer

    // Lock mutex
    pthread_mutex_lock(&tm->lock);
    job *acquired_job = tm->jq_head;
    tm->jq_head = acquired_job->next_job;
    // Unlock mutex
    pthread_mutex_unlock(&tm->lock);

    // I feel like I should just set the acquired job's next job link to NULL
    //  just to be safe because it should never be touched again, thread only needs to worry about
    //  the path
    acquired_job->next_job = NULL;
    debug("\nsomeone got a job?");

    return acquired_job;
}

int do_job(job *job_todo, int t_id) {
    debug("Thread (%d) is completing job for dir: %s", job_todo->path);
    char *cwd = job_todo->path;
    DIR *dir = opendir(job_todo->path);
    if (!dir) {
        perror("Error Opening Dir for Job");
        return -1;
    }
    struct dirent *entry;
    struct stat *entry_info;
    while ((entry = readdir(dir)) != NULL) {
        char *file_name = entry->d_name;
        char file_path[PATH_MAX];
        sprintf(file_path, "%s/%s", cwd, file_name);
        int stat_entry = stat(file_path, entry_info);
    }

    return 0;
}

void *run_thread(void *arg) {
    thread_manager *tm = arg;
    // register thread as alive
    pthread_mutex_lock(&tm->lock);
    tm->threads_alive++;
    int t_id = tm->threads_alive;
    debug("\nThread %d has come to life!", tm->threads_alive);
    pthread_mutex_unlock(&tm->lock);

    while (1) {
        // Lock the mutex right away, so no other threads can add jobs before we start sleep and
        // wait for a new job

        pthread_mutex_lock(&tm->lock);
        debug("\nsleeping (%d)", t_id);
        int job_wait_cond = pthread_cond_wait(&tm->job_ready, &tm->lock);
        debug("\nThread (%d), has awoken!", t_id);

        // if wait condition threw an error, we need to set the shutdown flag and trigger all other
        // threads to shutdown
        if (job_wait_cond != 0) {
            debug("\nnew job wait cond? (%d)\n%d", t_id, job_wait_cond);
            perror("\nthread error?");
            // something went wrong
            // lets lock the mutex, update the error code in the thread manager and then trigger
            // shutdown
            pthread_mutex_lock(&tm->lock);
            tm->worker_err = job_wait_cond;
            pthread_cond_broadcast(&tm->job_ready);
            pthread_mutex_unlock(&tm->lock);
            return NULL;
        }
        if (tm->shutdown) {
            debug("Thread (%d) shutting down", t_id);
            return NULL;
        }

        // if we get here then we were woken up and nothing is wrong, so we can get and complete a
        // job
        debug("\nend of run-thread");
        job *job_to_complete = get_job(tm, t_id);
        debug("thread (%d), has gotten job for path %s", job_to_complete->path);
        do_job(job_to_complete, t_id);
    }
    return NULL;
}

void test_job_insertion(thread_manager *tm) {
    add_job("/home/jemanuel/jek_utils/hex_lab/hex_sum", tm);
}

void *run_kernel_thread(void *arg) {
    debug("\nKernel thread is alive!");
    sleep(3);
    thread_manager *tm = arg;
    // test a simulated job
    test_job_insertion(tm);
    return NULL;
}

int thread_manager_init(thread_manager *tm) {
    int err;
    tm->jq_head = NULL;
    tm->jq_tail = NULL;
    err = pthread_cond_init(&tm->job_ready, NULL);
    if (err != 0) {
        perror("error initializing job_ready thread condition");
        return -1;
    }
    err = pthread_cond_init(&tm->traversal_complete, NULL);
    if (err != 0) {
        perror("error initializing traversal_complete thread condition");
        return -1;
    }
    err = pthread_mutex_init(&tm->lock, NULL);
    if (err != 0) {
        perror("error initializing mutex lock");
        return -1;
    }
    return 0;
}

int handle_threads(int num_cores) {
    // Initialize Thread Manager Struct
    thread_manager *tm = (thread_manager *)calloc(1, sizeof(thread_manager));
    int tm_initialization = thread_manager_init(tm);
    if (tm_initialization != 0) {
        perror("error initializing thread manager");
        return -1;
    }

    pthread_t kernel_thread;
    int err = pthread_create(&kernel_thread, NULL, run_kernel_thread, tm);
    if (err != 0) {
        perror("error creating kernel thread");
        return -1;
    }

    pthread_t threads[num_cores];

    for (int i = 0; i < num_cores; i++) {
        int create_thread = pthread_create(&threads[i], NULL, run_thread, tm);
        if (create_thread != 0) {
            perror("Error Creating Thread");
            return -1;
        }
    }

    // These thread joins are blockers, so we need to make sure they are at the end of the function
    // First we clean up the kernel thread (which bootstraps the process by injecting the first job)
    // Currently the kernel thread has a 3 second sleep in it (to allow other threads to start up)
    // Before it injects the job, I want to find a cleaner way to do this
    int join_kernel_thread = pthread_join(kernel_thread, NULL);
    if (join_kernel_thread != 0) {
        perror("error joining kernel thread");
    } else {
        debug("KERNEL THREAD HAS ENDED!");
    }
    // now that all threads are running we wait for them all to complete
    for (int i = 0; i < num_cores; i++) {
        int join_threads = pthread_join(threads[i], NULL);
        if (join_threads != 0) {
            perror("Error Joining Threads");
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    hex_debug_init(argc, argv);
    int sodium_initialization = sodium_init();
    if (sodium_initialization != 0) {
        perror("error initializing libsodium");
        return 1;
    }
    printf("SUCCESFULLY INITIALIZED SODIUM");

    int num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) {
        perror("error getting number of cores");
        return 1;
    }

    int thread_status = handle_threads(num_cores);
    if (thread_status == -1) {
        debug("thread error");
        return 1;
    }
    return 0;
}
