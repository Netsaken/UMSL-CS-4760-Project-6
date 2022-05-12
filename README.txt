How to Run:
1. Navigate to project folder
2. Run "make"
3. Invoke program using "./oss"
4. Wait up to 2 seconds
5. Examine "LOGFile.txt" for run output. Some statistics at the end of the file.

Git Repository: https://github.com/Netsaken/UMSL-CS-4760-Project-6

Notes:
- Semaphore used for notifications to OSS from User_proc.

Problems:
- FIFO scheme is very basic. It only iterates through the frames and does not take
into account empty frames. For instance, if the next scheduled frame is 15, but 7
is empty because a process terminated, it will still overwrite frame 15.
- Average memory access speed is not recorded.
- Memory references are not checked for validity, nor are process permissions.
- I feel like I overused the page table struct for other variables I needed to have shared.

Other Issues Encountered:
- Processes check for termination after total memory references, rather than local memory
references. Not sure if that's wrong, but when I used local references, the program went for
hundreds of thousands of lines.
- The last process in the system does not cause OSS to print a "terminated" message, due to
how and when I'm checking for the 100-process limit.