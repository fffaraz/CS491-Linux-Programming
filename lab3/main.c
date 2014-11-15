// Lab 3 - fork, pipes, log server, etc.
// Faraz Fallahi (faraz@siu.edu)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>

//----------------------------------------------------------------------

#define NUMBER_CLIENTS  5
#define NUMBER_MESSAGES 5
#define MAXIMUM_SLEEP   8
#define BUFFER_SIZE     1024

//----------------------------------------------------------------------

struct StatStuff
{
    int pid; // %d
    char comm[256]; // %s
    char state; // %c
    int ppid; // %d
    int pgrp; // %d
    int session; // %d
    int tty_nr; // %d
    int tpgid; // %d
    unsigned long flags; // %lu OR %l
    unsigned long minflt; // %lu
    unsigned long cminflt; // %lu
    unsigned long majflt; // %lu
    unsigned long cmajflt; // %lu
    unsigned long utime; // %lu
    unsigned long stime;  // %lu
    long cutime; // %ld
    long cstime; // %ld
    long priority; // %ld
    long nice; // %ld
    long num_threads; // %ld
    long itrealvalue; // %ld
    unsigned long starttime; // %lu
    unsigned long vsize; // %lu
    long rss; // %ld
    unsigned long rlim; // %lu
    unsigned long startcode; // %lu
    unsigned long endcode; // %lu
    unsigned long startstack; // %lu
    unsigned long kstkesp; // %lu
    unsigned long kstkeip; // %lu
    unsigned long signal; // %lu
    unsigned long blocked; // %lu
    unsigned long sigignore; // %lu
    unsigned long sigcatch; // %lu
    unsigned long wchan; // %lu
    unsigned long nswap; // %lu
    unsigned long cnswap; // %lu
    int exit_signal; // %d
    int processor; // %d
    unsigned long rt_priority; // %lu
    unsigned long policy; // %lu
    unsigned long long delayacct_blkio_ticks; // %llu
};

int readStat(pid_t pid, struct StatStuff *s)
{
    static const char * const procfile = "/proc/%d/stat";
    static const char * const format = "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %lu %lu %llu";

    char buf[128];
    sprintf(buf, procfile, pid);

    FILE *proc = fopen(buf, "r");
    if(proc)
    {
        int ret = fscanf(proc, format,
                         &s->pid,
                         s->comm,
                         &s->state,
                         &s->ppid,
                         &s->pgrp,
                         &s->session,
                         &s->tty_nr,
                         &s->tpgid,
                         &s->flags,
                         &s->minflt,
                         &s->cminflt,
                         &s->majflt,
                         &s->cmajflt,
                         &s->utime,
                         &s->stime,
                         &s->cutime,
                         &s->cstime,
                         &s->priority,
                         &s->nice,
                         &s->num_threads,
                         &s->itrealvalue,
                         &s->starttime,
                         &s->vsize,
                         &s->rss,
                         &s->rlim,
                         &s->startcode,
                         &s->endcode,
                         &s->startstack,
                         &s->kstkesp,
                         &s->kstkeip,
                         &s->signal,
                         &s->blocked,
                         &s->sigignore,
                         &s->sigcatch,
                         &s->wchan,
                         &s->nswap,
                         &s->cnswap,
                         &s->exit_signal,
                         &s->processor,
                         &s->rt_priority,
                         &s->policy,
                         &s->delayacct_blkio_ticks);
        fclose(proc);
        if(ret == 42) return 1;
    }
    return 0;
}

void printStat(FILE *out, struct StatStuff *stuff)
{
    fprintf(out,"pid = %d\n", stuff->pid);
    /*
    fprintf(out,"comm = %s\n", stuff->comm);
    fprintf(out,"state = %c\n", stuff->state);
    fprintf(out,"ppid = %d\n", stuff->ppid);
    fprintf(out,"pgrp = %d\n", stuff->pgrp);
    fprintf(out,"session = %d\n", stuff->session);
    fprintf(out,"tty_nr = %d\n", stuff->tty_nr);
    fprintf(out,"tpgid = %d\n", stuff->tpgid);
    fprintf(out,"flags = %lu\n", stuff->flags);
    fprintf(out,"minflt = %lu\n", stuff->minflt);
    fprintf(out,"cminflt = %lu\n", stuff->cminflt);
    fprintf(out,"majflt = %lu\n", stuff->majflt);
    fprintf(out,"cmajflt = %lu\n", stuff->cmajflt);
    */
    fprintf(out,"utime = %lu\n", stuff->utime);
    fprintf(out,"stime = %lu\n", stuff->stime);
    return;
    fprintf(out,"cutime = %ld\n", stuff->cutime);
    fprintf(out,"cstime = %ld\n", stuff->cstime);
    fprintf(out,"priority = %ld\n", stuff->priority);
    fprintf(out,"nice = %ld\n", stuff->nice);
    fprintf(out,"num_threads = %ld\n", stuff->num_threads);
    fprintf(out,"itrealvalue = %ld\n", stuff->itrealvalue);
    fprintf(out,"starttime = %lu\n", stuff->starttime);
    fprintf(out,"vsize = %lu\n", stuff->vsize);
    fprintf(out,"rss = %ld\n", stuff->rss);
    fprintf(out,"rlim = %lu\n", stuff->rlim);
    fprintf(out,"startcode = %lu\n", stuff->startcode);
    fprintf(out,"endcode = %lu\n", stuff->endcode);
    fprintf(out,"startstack = %lu\n", stuff->startstack);
    fprintf(out,"kstkesp = %lu\n", stuff->kstkesp);
    fprintf(out,"kstkeip = %lu\n", stuff->kstkeip);
    fprintf(out,"signal = %lu\n", stuff->signal);
    fprintf(out,"blocked = %lu\n", stuff->blocked);
    fprintf(out,"sigignore = %lu\n", stuff->sigignore);
    fprintf(out,"sigcatch = %lu\n", stuff->sigcatch);
    fprintf(out,"wchan = %lu\n", stuff->wchan);
    fprintf(out,"nswap = %lu\n", stuff->nswap);
    fprintf(out,"cnswap = %lu\n", stuff->cnswap);
    fprintf(out,"exit_signal = %d\n", stuff->exit_signal);
    fprintf(out,"processor = %d\n", stuff->processor);
    fprintf(out,"rt_priority = %lu\n", stuff->rt_priority);
    fprintf(out,"policy = %lu\n", stuff->policy);
    fprintf(out,"delayacct_blkio_ticks = %llu\n", stuff->delayacct_blkio_ticks);
}

//----------------------------------------------------------------------

volatile sig_atomic_t flag;

void alarm_handler(int signal)
{
    flag = 0;
}

void client(int *pipefd)
{
    static const char * const mesformat  = "%d: Message: #%d: user: %.5f kernel: %.5f\n";

    // Child writes to pipe
    // Close unused read end
    close(pipefd[0]);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, alarm_handler);
    pid_t pid = getpid();
    srand(pid);

    float div = 1;
    div = sysconf(_SC_CLK_TCK);
    //fprintf(stderr, "_SC_CLK_TCK : %d - %f \n", pid, div);

    for(int i = 1; i <= NUMBER_MESSAGES; i++)
    {
        unsigned int delay = random();
        delay = (delay % MAXIMUM_SLEEP) + 1;

        //sleep(delay);
        flag = 1;
        alarm(delay);
        while(flag);

        struct StatStuff stat;
        if(!readStat(pid, &stat))
            perror("readStat failed");
#ifdef DEBUG
        printStat(stderr, &stat);
#endif

        char buf[BUFFER_SIZE];
        int len = snprintf(buf, BUFFER_SIZE, mesformat, pid, i, stat.utime/div, stat.stime/div);

        /*
        * POSIX.1-2001 says that write(2)s of less than PIPE_BUF bytes must be atomic.
        * also it requires PIPE_BUF to be at least 512 bytes.
        * On Linux, PIPE_BUF is 4096 bytes.
       */
        int size = write(pipefd[1], buf, len);
        if(size < 0)
        {
            perror("Broken pipe");
            break;
        }
        if(size != len)
        {
            perror("Write failed");
            break;
        }
    }

    close(pipefd[1]); // Reader will see EOF
    _exit(EXIT_SUCCESS);
}

//----------------------------------------------------------------------

void server(int *pipefd, char *filename)
{
    static const char * const logformat = "%s: %s\n"; // don't worry, we won't have double \n at the end.

    // Parent reads from pipe
    // Close unused write end
    close(pipefd[1]);

    FILE *log = stdout;
    if(filename[0] != '\0') // empty filename
        log = fopen(filename, "w");

    char buf1[BUFFER_SIZE]; // read buffer
    char buf2[BUFFER_SIZE]; // result buffer

    int size;
    while((size = read(pipefd[0], buf1, BUFFER_SIZE)) > 0) // totally wrong! this may read a message and half. fingers crossed.
    {
        buf1[size] = '\0'; // first make it a valid C string.

        // there may be more than one message queued in the pipe. I really can't hope for this one not to happen, 'cause it actually did!
        char *message = strtok(buf1, "\n");
        while(message)
        {
            time_t curtime;
            char *strtime;
#if 0 // CS306
            time(&curtime);
            strtime = ctime(&curtime);
            strtime[strlen(strtime) - 1] = '\0'; // deletes that annoying \n at the end
#else // CS491
            char buf3[BUFFER_SIZE];
            char buf4[BUFFER_SIZE];
            struct timeval tv;
            gettimeofday(&tv, NULL);
            curtime = tv.tv_sec;
            strftime(buf3, BUFFER_SIZE, "%m-%d-%Y %T.", localtime(&curtime));
            snprintf(buf4, BUFFER_SIZE, "%s%ld", buf3, tv.tv_usec);
            strtime = buf4;
#endif
            size = snprintf(buf2, BUFFER_SIZE, logformat, strtime, message);
            fputs(buf2, log);
            fflush(log);

            message = strtok(NULL, "\n");
        }
    }

    close(pipefd[0]);
    if(log != stdout)
        fclose(log);
}

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
#if 0
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }
#endif

    int pipefd[2];
    if( pipe(pipefd) == -1 )
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    int first_child = -1;

    for(int i = 0; i < NUMBER_CLIENTS; i++)
    {
        pid_t cpid = fork();

        if(cpid == -1)
        {
            perror("fork");
            return EXIT_FAILURE;
        }

        if(cpid == 0)
        {
            client(pipefd);
            return EXIT_FAILURE + 1; // just in case, it's worse than failure if it happens.
        }
        else
        {
            if(first_child < 0) first_child = cpid;
            /*
             * If pgid == pid or pgid == 0 then this creates a new process group with process group leader pid.
             * Otherwise, this puts pid into the already existing process group pgid.
             * A zero pid refers to the current process.
            */
            setpgid(cpid, first_child);
        }
    }

    server(pipefd, argc > 1 ? argv[1] : "");

    //wait(NULL); // Wait for childs
    killpg(first_child, SIGKILL);
    //exit(EXIT_SUCCESS);
    return EXIT_FAILURE;
}
