#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BILLION 1000000000UL //1 second in nanoseconds
#define MAXIMUM_PROCESSES 18

unsigned int *sharedNS;
unsigned int *sharedSecs;
struct Stats *statistics = {0};
struct Pager *pageTbl = {0};

struct Stats {
    int immediateRequests, delayedRequests, deadlockTerminations, naturalTerminations, deadlockRuns;
    float deadlockConsiderations, deadlockTerminationAverage;
};

struct Pager {
    pid_t pidArray[MAXIMUM_PROCESSES];
    int page[32];
    int mAddressReq[18];
};

void endProcess() {
    //Detach shared memory
    if (shmdt(sharedNS) == -1) {
        perror("./user_proc: endShmdtNS");
    }

    if (shmdt(sharedSecs) == -1) {
        perror("./user_proc: endShmdtSecs");
    }

    if (shmdt(pageTbl) == -1) {
        perror("./user_proc: endShmdtPage");
    }

    if (shmdt(statistics) == -1) {
        perror("./user_proc: endShmdtStat");
    }

    exit(0);
}

int main(int argc, char *argv[])
{
    int shmid_NS, shmid_Secs, shmid_Page, shmid_Stat;
    unsigned int initialSharedSecs, initialSharedNS;
    int pageNum, offset, termTime, termTimeRefs, referenceChecks = 0;
    int initSwitch = 1, ownedSwitch = 0, termSwitch = 1;
    int resourcesObtained[10] = {0};
    int readChance = 70, terminationChance = 10;

    int msqid;
    int i = atoi(argv[0]);

    key_t keyNS = ftok("./README.txt", 'Q');
    key_t keySecs = ftok("./README.txt", 'b');
    key_t keyRsrc = ftok("./user_proc.c", 'r');
    key_t keyStat = ftok("./user_proc.c", 't');

    //Format "perror"
    char* title = argv[0];
    char report[30] = ": shm";
    char* message;

    //Get shared memory
    shmid_NS = shmget(keyNS, sizeof(sharedNS), IPC_CREAT | 0666);
    if (shmid_NS == -1) {
        strcpy(report, ": shmgetNS");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    shmid_Secs = shmget(keySecs, sizeof(sharedSecs), IPC_CREAT | 0666);
    if (shmid_Secs == -1) {
        strcpy(report, ": shmgetSecs");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    shmid_Page = shmget(keyRsrc, sizeof(pageTbl), IPC_CREAT | 0666);
    if (shmid_Page == -1) {
        strcpy(report, ": shmgetPage");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    shmid_Stat = shmget(keyStat, sizeof(statistics), IPC_CREAT | 0666);
    if (shmid_Stat == -1) {
        strcpy(report, ": shmgetStat");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    //Attach shared memory
    sharedNS = (unsigned int*)shmat(shmid_NS, NULL, 0);
    if (sharedNS == (void *) -1) {
        strcpy(report, ": shmatNS");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    sharedSecs = (unsigned int*)shmat(shmid_Secs, NULL, 0);
    if (sharedSecs == (void *) -1) {
        strcpy(report, ": shmatSecs");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    pageTbl = shmat(shmid_Page, NULL, 0);
    if (pageTbl == (void *) -1) {
        strcpy(report, ": shmatPage");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    statistics = shmat(shmid_Stat, NULL, 0);
    if (statistics == (void *) -1) {
        strcpy(report, ": shmatStat");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    /********************************************************************************

    Start doing things here

    *********************************************************************************/
    //Initialize RNG
    srand(getpid() * time(NULL));

    for (;;) {
        //Spin until OSS responds
        //REPLACE WITH SEMAPHORE. I SUSPECT IT'S FASTER
        while (pageTbl->mAddressReq[i] > -1) {}

        //Find the next period for self-termination check
        if (termSwitch == 1) {
            termTimeRefs = (rand() % 201) - 100; //Between -100 and 100
            termTime = 1000 + termTimeRefs;
            termSwitch = 0;
        }

        /********************************************************************************************************************
        At random memory references, decide whether to DIE DRAMATICALLY
        *********************************************************************************************************************/
        if (referenceChecks >= termTime) {
            //Do a russian roulette dice roll
            if ((rand() % 101) < terminationChance) {
                //FREE MEMORY
                pageTbl->pidArray[i] = 0;
                endProcess();
            }

            termSwitch = 1;
            referenceChecks = 0;
        }

        /********************************************************************************************************************
        Generate memory references
        *********************************************************************************************************************/
        //Get a random page
        pageNum = rand() % 32;
        offset = rand() % 1024;
        pageTbl->mAddressReq[i] = (pageNum * 1024) + offset;

        if ((rand() % 101) < readChance) {
            //READ
            //Send address and whether read/write to OSS
        } else {
            //WRITE
        }
        
        ++referenceChecks;
    }

    /**********************************************************************************
   
    End doing things here

    ***********************************************************************************/

    endProcess();

    return 0;
}