#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BILLION 1000000000UL //1 second in nanoseconds

unsigned int *sharedNS;
unsigned int *sharedSecs;
struct Stats *statistics = {0};
struct Pager *pageTbl = {0};

struct Stats {
    int immediateRequests, delayedRequests, deadlockTerminations, naturalTerminations, deadlockRuns;
    float deadlockConsiderations, deadlockTerminationAverage;
};

struct Pager {
    pid_t pidArray[18];
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
        perror("./user_proc: endShmdtRsrc");
    }

    if (shmdt(statistics) == -1) {
        perror("./user_proc: endShmdtStat");
    }

    exit(0);
}

int main(int argc, char *argv[])
{
    int shmid_NS, shmid_Secs, shmid_Page, shmid_Stat;
    unsigned int creationTimeSecs, creationTimeNS, initialSharedSecs, initialSharedNS, initialTermSecs, initialTermNS, newTimeNS, termTimeNS;
    int initSwitch = 1, ownedSwitch = 0, termSwitch = 1;
    int randomRsrcPos, randomRsrcStorage;
    int resourcesObtained[10] = {0};
    int requestChance = 40, luckyRelease = 40, terminationChance = 10;

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
        strcpy(report, ": shmgetRsrc");
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
        strcpy(report, ": shmatRsrc");
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

    //Set time interval bounds
    int B = 15000000; //15 milliseconds
    int D = 250000000; //250 milliseconds

    //Set creation time, so process stays alive for at least 1 second
    creationTimeSecs = *sharedSecs;
    creationTimeNS = *sharedSecs;

    // for (;;) {
    // }

    //I would put a message here, but this is just a test... so I don't get to blow up your console like old times. :(

    endProcess();

    /**********************************************************************************
   
    End doing things here

    ***********************************************************************************/

    return 0;
}