Lab 2: Scheduler/Dispatcher

ran code through GCC Compiler 4.8.5 on linserv1 CIMS Machine

To run this program:

$ make
$ ./scheduler -v -s<schedspec> inputfile randfile
An Example: ./scheduler -v -sL input0 rfile  ,where L denotes LCFS scheduler on input0 file

OR to remove file:

$ make clean 


Key:
FCFS = First Come First Serve (-sF)
LCFS = Last Come First Serve (-sL)
SRTF = Shortest Remaining Time First (-sS)
RR = Round Robin (-sR)
PRIO = Priority Scheduler (-sP(quantum):(maxprios))
PREPRIO = Preemptive Priority Scheduler (-sE(quantum):(maxprios))
-v is verbose option (optional)
-s is scheduler type (required)

**If not specified, maxprios is 4.
