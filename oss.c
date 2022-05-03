#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BILLION 1000000000UL //1 second in nanoseconds
#define VERBOSE 1 //1 is on, 0 is off
#define MAXIMUM_PROCESSES 18

pid_t childPid;
FILE *file;

unsigned int *sharedNS = 0;
unsigned int *sharedSecs = 0;
struct Stats *statistics = {0};
struct Pager *pageTbl = {0};
int shmid_NS, shmid_Secs, shmid_Page, shmid_Stat;

struct Stats {
    int immediateRequests, delayedRequests, deadlockTerminations, naturalTerminations, deadlockRuns;
    float deadlockConsiderations, deadlockTerminationAverage;
};

struct Pager {
    pid_t pidArray[MAXIMUM_PROCESSES];
};

//Safely terminates program after error, interrupt, or Alarm timer
static void handle_sig(int sig) {
    int errsave, status;
    errsave = errno;

    //Print notification
    printf("Program finished or interrupted. Cleaning up...\n");

    //Output statistics
    // fseek(file, 0, SEEK_CUR);
    // fflush(file);
    // fseek(file, 0, SEEK_CUR);

    //Close file
    fclose(file);

    //End children
    for (int k = 0; k < MAXIMUM_PROCESSES; k++) {
        if (pageTbl->pidArray[k] != 0) {
            kill(pageTbl->pidArray[k], SIGTERM);
            waitpid(pageTbl->pidArray[k], &status, 0);
        }
    }
    sleep(1);

    //Detach shared memory
    if (shmdt(sharedNS) == -1) {
        perror("./oss: sigShmdtNS");
    }

    if (shmdt(sharedSecs) == -1) {
        perror("./oss: sigShmdtSecs");
    }

    if (shmdt(pageTbl) == -1) {
        perror("./oss: sigShmdtRsrc");
    }

    if (shmdt(statistics) == -1) {
        perror("./oss: sigShmdtStat");
    }

    //Remove shared memory
    if (shmctl(shmid_NS, IPC_RMID, 0) == -1) {
        perror("./oss: sigShmctlNS");
    }

    if (shmctl(shmid_Secs, IPC_RMID, 0) == -1) {
        perror("./oss: sigShmctlSecs");
    }

    if (shmctl(shmid_Page, IPC_RMID, 0) == -1) {
        perror("./oss: sigShmctlRsrc");
    }

    if (shmctl(shmid_Stat, IPC_RMID, 0) == -1) {
        perror("./oss: sigShmctlStat");
    }

    printf("Cleanup complete!\n");

    //Exit program
    errno = errsave;
    exit(0);
}

static int setupinterrupt(void) {
    struct sigaction act;
    act.sa_handler = handle_sig;
    act.sa_flags = 0;
    return (sigemptyset(&act.sa_mask) || sigaction(SIGALRM, &act, NULL));
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sig);
    signal(SIGALRM, handle_sig);
    signal(SIGABRT, handle_sig);

    key_t keyNS = ftok("./README.txt", 'Q');
    key_t keySecs = ftok("./README.txt", 'b');
    key_t keyRsrc = ftok("./user_proc.c", 'r');
    key_t keyStat = ftok("./user_proc.c", 't');
    char iNum[3];
    int iInc = 0, status;
    int maxProcsHit = 0, requestCount = 0;

    unsigned long initialTimeNS, initialTimeSecs;
    unsigned int randomTimeNS = 0, hundredProcs = 0;
    int initSwitch = 1, endCheck = 0;

    //Format "perror"
    char* title = argv[0];
    char report[30] = ": shm";
    char* message;

    //Set up interrupts
    if (setupinterrupt() == -1) {
        strcpy(report, ": setupinterrupt");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    //Get shared memory
    shmid_NS = shmget(keyNS, sizeof(sharedNS), IPC_CREAT | 0666);
    if (shmid_NS == -1) {
        strcpy(report, ": shmgetNS");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    shmid_Secs = shmget(keySecs, sizeof(sharedSecs), IPC_CREAT | 0666);
    if (shmid_Secs == -1) {
        strcpy(report, ": shmgetSecs");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    shmid_Page = shmget(keyRsrc, sizeof(pageTbl), IPC_CREAT | 0666);
    if (shmid_Page == -1) {
        strcpy(report, ": shmgetRsrc");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    shmid_Stat = shmget(keyStat, sizeof(statistics), IPC_CREAT | 0666);
    if (shmid_Stat == -1) {
        strcpy(report, ": shmgetStat");
        message = strcat(title, report);
        perror(message);
        abort();
    }
    
    //Attach shared memory
    sharedNS = shmat(shmid_NS, NULL, 0);
    if (sharedNS == (void *) -1) {
        strcpy(report, ": shmatNS");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    sharedSecs = shmat(shmid_Secs, NULL, 0);
    if (sharedSecs == (void *) -1) {
        strcpy(report, ": shmatSecs");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    pageTbl = shmat(shmid_Page, NULL, 0);
    if (pageTbl == (void *) -1) {
        strcpy(report, ": shmatRsrc");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    statistics = shmat(shmid_Stat, NULL, 0);
    if (statistics == (void *) -1) {
        strcpy(report, ": shmatStat");
        message = strcat(title, report);
        perror(message);
        abort();
    }

    /********************************************************************************

    Start doing things here

    *********************************************************************************/
    //End program after 2 seconds
    alarm(2);

    //Open file (closed in handle_sig())
    file = fopen("LOGFile.txt", "w");

    int totP = 0;

    //Loop until alarm rings
    for (;;) {
        //Get a random time interval for process creation (only if it is not already set)
        if (initSwitch == 1) {
            initialTimeSecs = *sharedSecs;
            initialTimeNS = *sharedNS;

            randomTimeNS = (rand() % 499000000) + 1000000; //Between 1 and 500 milliseconds, inclusive
            initSwitch = 0;
        }

        //Add ambient time to the clock
        *sharedNS += 100000; //0.1 milliseconds
        if (*sharedNS >= BILLION) {
            *sharedSecs += 1;
            *sharedNS -= BILLION;
        }

        /********************************************************************************************************************
        If the clock has hit the random time, make a new process
        *********************************************************************************************************************/
        //Before creating child, reset iInc value to the first available empty slot in table (and check if max processes has been hit)
        // for (int j = 0; j < MAXIMUM_PROCESSES; j++) {
        //     if (pageTbl->pidArray[j] == 0) {
        //         iInc = j;
        //         maxProcsHit = 0;
        //         break;
        //     }
        //     maxProcsHit = 1;
        // }

        //If all processes have been terminated and new processes cannot be created, end program
        if (hundredProcs >= 100) {
            handle_sig(2);
        }

        //Create child if able
        if (hundredProcs <= 100 && maxProcsHit == 0 && (((*sharedSecs * BILLION) + *sharedNS) > ((initialTimeSecs * BILLION) + initialTimeNS + randomTimeNS))) {
            //Add time to the clock
            *sharedNS += 500000; //0.5 milliseconds
            if (*sharedNS >= BILLION) {
                *sharedSecs += 1;
                *sharedNS -= BILLION;
            }
            
            //Create a user process
            childPid = fork();
            if (childPid == -1) {
                strcpy(report, ": childPid");
                message = strcat(title, report);
                perror(message);
                abort();
            }

            // Allocate and execute
            if (childPid == 0) {
                sprintf(iNum, "%i", iInc);
                execl("./user_proc", iNum, NULL);
            } else {
                // Store childPid
                pageTbl->pidArray[iInc] = childPid;

                // Log to file
                fprintf(file, "Master creating new Process P%i(%i) at clock time %li:%09li\n", iInc, ++totP, (long)*sharedSecs, (long)*sharedNS);
            }

            //Add to total process count
            ++hundredProcs;

            //Reset switch
            initSwitch = 1;
        }
    }

    /**********************************************************************************
   
    End doing things here

    ***********************************************************************************/

    return 0;
}