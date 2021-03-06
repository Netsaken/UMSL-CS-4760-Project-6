#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
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
    float numberAccesses, numberOfPageFaults;
};

struct Pager {
    pid_t pidArray[MAXIMUM_PROCESSES];
    int page[18][32];
    int mAddressReq[18];
    int dirtyBit[256];

    int referenceChecks;
    int waiting[18];
    int waitQueue[18];
};

union semun {
    int val;                /* value for SETVAL */
    struct semid_ds *buf;   /* buffer for IPC_STAT & IPC_SET */
    unsigned short *array;  /* array for GETALL & SETALL */
    struct seminfo *__buf;  /* buffer for IPC_INFO */
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
    int pageNum, offset, termTime, termTimeRefs;
    int initSwitch = 1, ownedSwitch = 0, termSwitch = 1, pageFilled = 0;
    int readChance = 70, terminationChance = 50;

    int semid;
    union semun arg;
    int i = atoi(argv[0]);

    key_t keyNS = ftok("./README.txt", 'Q');
    key_t keySecs = ftok("./README.txt", 'b');
    key_t keySem = ftok("./user_proc.c", 'e');
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

    //Get semaphore set
    if ((semid = semget(keySem, 18, 0)) == -1) {
        strcpy(report, ": semget");
        message = strcat(title, report);
        perror(message);
        return 1;
    }

    /********************************************************************************

    Start doing things here

    *********************************************************************************/
    //Initialize RNG
    srand(getpid() * time(NULL));

    //Initialize sembuf
    struct sembuf sb = {i, -1, 0};

    for (;;) {
        //Wait until OSS responds
        sb.sem_op = 0;
        if(semop(semid, &sb, 1) == -1) {
            strcpy(report, ": semop(wait)");
            message = strcat(title, report);
            perror(message);
            return 1;
        }

        //Find the next period for self-termination check
        if (termSwitch == 1) {
            termTimeRefs = (rand() % 201) - 100; //Between -100 and 100
            termTime = pageTbl->referenceChecks + 1000 + termTimeRefs;
            termSwitch = 0;
        }

        /********************************************************************************************************************
        At random memory references, decide whether to DIE DRAMATICALLY
        *********************************************************************************************************************/
        if (pageTbl->referenceChecks >= termTime) {
            //Do a russian roulette dice roll
            if ((rand() % 101) < terminationChance) {
                //Send notification to OSS and reset pidArray
                pageTbl->mAddressReq[i] = -2;
                pageTbl->pidArray[i] = 0;

                //Free memory
                for (int f = 0; f < 32; f++) {
                    pageTbl->page[i][f] = 0;
                }

                endProcess();
            }

            termSwitch = 1;
        }

        /********************************************************************************************************************
        Generate memory references
        *********************************************************************************************************************/
        //Get a random page
        pageNum = rand() % 32;

        //Check if that page is already filled
        if (pageTbl->page[i][pageNum] > 0) {
            pageFilled = 1;
        } else {
            pageFilled = 0;
        }

        //If page isn't filled, set new value
        if (pageFilled == 0) {
            offset = rand() % 1024;
            pageTbl->mAddressReq[i] = (pageNum * 1024) + offset;
            pageTbl->page[i][pageNum] = pageTbl->mAddressReq[i];
        } else {
            //Otherwise, pass existing value from the page
            pageTbl->mAddressReq[i] = pageTbl->page[i][pageNum];
        }

        //Decide read/write and send decision to OSS
        if ((rand() % 101) < readChance) {
            //Increment own semaphore for a READ
            sb.sem_op = 1;
            if(semop(semid, &sb, 1) == -1) {
                strcpy(report, ": semop(++1)");
                message = strcat(title, report);
                perror(message);
                return 1;
            }
        } else {
            //Increment own semaphore for a WRITE
            sb.sem_op = 2;
            if(semop(semid, &sb, 1) == -1) {
                strcpy(report, ": semop(++2)");
                message = strcat(title, report);
                perror(message);
                return 1;
            }
        }
        
        ++statistics->numberAccesses;
    }

    /**********************************************************************************
   
    End doing things here

    ***********************************************************************************/

    endProcess();

    return 0;
}