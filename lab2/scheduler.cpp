#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <unistd.h>

using namespace std;

enum previous_state {  
        STATE_RUNNING, STATE_BLOCKED, STATE_CREATED, STATE_READY, STATE_PREEMPT, STATE_FINISH
};

class Event {
public:
    int timeStamp;  //time at which process is making the state transition
    int pid;  //process identifier
    int startTime; //event creation timepoint
    int prevState; //previous state 
    int newState;  //status of new state
    Event();
    Event(int id, int time, int state);  //state is the transition to state, and time is the new timestamp
    string getState(int i);
    
};
Event::Event(int id, int time, int state) {
    timeStamp = time;
    pid = id;
    newState = state;
    startTime=0; //event creation timepoint
    prevState=0; //previous state 
}
Event::Event() {
    timeStamp=0;  //time at which process is making the state transition
    pid=0;  //process identifier
    startTime=0; //event creation timepoint
    prevState=0; //previous state 
    newState=0;  //status of new state
}
string Event::getState(int i) {
    switch (i) {
        case 0: return "RUNNG"; break;
        case 1: return "BLOCK"; break;
        case 2: return "CREATED"; break;
        case 3: return "READY"; break;
        case 4: return "READY"; break;
        case 5: return "FINISH"; break;

    }
}

class Process {
public:
    int pid; //process identifier
    int arrivalTime;
    int totalCPU;
    int CPUBurst;
    int IOBurst;
    int dynamicPrio;
    int staticPrio;
    int processRunStartTime; //each process object will have this which is a timestamp when process enters Run state
    int finishTime;
    int turnAroundTime;  // finish time - arrival time
    int IOTime; //time in blocked state 
    int CPUWaitingTime;  //time in ready state
    int CPUTime; //accumulated CPU time so far
    int ready_commence;  //for getting total time in ready state
    int burstRemaining;
    void setArrival(int time);
    void setTotalCPU(int cpu);
    void setCPUBurst(int burst);
    void setIOBurst(int burst);
    int getArrival();
    int getTotalCPU();
    int getCPUBurst();
    int getIOBurst();
    Process();
    Process(int id, int stat, int dyn);
};
Process::Process(int id, int stat, int dyn) {
    pid = id;
    staticPrio = stat;
    dynamicPrio = dyn;
    arrivalTime = 0;
    totalCPU=0;
    CPUBurst=0;
    IOBurst=0;
    finishTime=0;
    turnAroundTime=0;  // finish time - arrival time
    IOTime=0; //time in blocked state 
    CPUWaitingTime=0;  //time in ready state
    CPUTime=0; //accumulated CPU time so far
    ready_commence=0;  //for getting total time in ready state
    burstRemaining=0;
}
Process::Process() {
    pid = 0;
    arrivalTime = 0;
    totalCPU=0;
    CPUBurst=0;
    IOBurst=0;
    dynamicPrio=0;
    staticPrio=0;
    finishTime=0;
    turnAroundTime=0;  // finish time - arrival time
    IOTime=0; //time in blocked state 
    CPUWaitingTime=0;  //time in ready state
    CPUTime=0; //accumulated CPU time so far
    ready_commence=0;  //for getting total time in ready state
    burstRemaining=0;
}

void Process::setArrival(int time) {
    arrivalTime = time;
}
void Process::setTotalCPU(int cpu) {
    totalCPU = cpu;
} 
void Process::setCPUBurst(int burst) {
    CPUBurst = burst;
}
void Process::setIOBurst(int burst) {
    IOBurst = burst;
}


int Process::getArrival() {
    return arrivalTime;
}
int Process::getTotalCPU() {
    return totalCPU;
}
int Process::getCPUBurst() {
    return CPUBurst;
}
int Process::getIOBurst() {
    return IOBurst;
}
Process* curRunningProcess = nullptr;
Process* curBlockedProcess = nullptr;
class Discrete_Sim {
private:
    vector<Event> events;  //when this contains an event with pid = -1, the simulation is finished

public:
    
    Event getEvent();
    void setEvent(Event e);
    void removeEvent();
    int rmEventTime();
    int getNextTime();
    Discrete_Sim();
};

Discrete_Sim::Discrete_Sim() {
    Event e(-1,-1,-1);  //always put one of these in the DES to indicate the end of the simulation
    events.push_back(e);
}

Event Discrete_Sim::getEvent() {
    Event e = events.front();
    events.erase(events.begin());
    return e;
}

void Discrete_Sim::setEvent(Event e) {
    if (events.size() == 1) {   //size is one due to initial event (-1,-1,-1)
        events.insert(events.begin(),e);
        return;
    }
    for (int m = 0; m < events.size(); m++) {
        if (e.timeStamp > events.at(m).timeStamp && events.at(m).timeStamp >= 0) {  //events.at(m).time >= 0 to account for last event's timestamp== -1
            continue;
        } else if (e.timeStamp < events.at(m).timeStamp && events.at(m).timeStamp >= 0){
            events.insert(m+events.begin(),e);
            break;
        } else if (e.timeStamp == events.at(m).timeStamp && events.at(m).timeStamp >= 0) {
            int k = m+1;
            while (events[k].timeStamp == e.timeStamp) {
                ++k;  //same thing as k = k + 1;
            }
            events.insert(k+events.begin(),e);
            break;
        } else {  //when comparing with the last initial event in event queue. 
            events.insert(m+events.begin(),e);
            break;
        }
    }
}

//Remove running process's event which has to be in blocked or finished
void Discrete_Sim::removeEvent() {
    int id = curRunningProcess->pid;
    for (int i = 0; i < events.size(); i++) {
        if (events[i].pid == id) {
            events.erase(events.begin()+i);
            return;
        }
    }

}
int Discrete_Sim::rmEventTime() {   //to get the current running process's next event to see if it's equal to current event timestamp
    int id = curRunningProcess->pid;
    for (int i = 0; i < events.size(); i++) {
        if (events[i].pid == id) {
            return events[i].timeStamp;
            
        }
    }
    return -5;
}

int Discrete_Sim::getNextTime() {
    
    return events.front().timeStamp;
}

//Global variables 
Discrete_Sim sim; 
int currentTime=-1;
int currentPrio = 0; 

class Scheduler {
protected:
    vector<Process *> queue;
public:
    virtual void add_to_queue(Process *p) = 0;
    virtual Process* get_next_process() = 0;
};

class FCFS: public Scheduler {
public:
    void add_to_queue(Process *p) override;
    Process* get_next_process() override;

};
//override keyword added to method signature is error?
void FCFS::add_to_queue(Process *p)  {
    queue.push_back(p);
}
Process* FCFS::get_next_process()  {
    if (queue.empty())
        return nullptr;
    vector<Process *>::iterator it = queue.begin();
    Process *proc = *it;
    queue.erase(it);
    return proc;
}

class LCFS: public Scheduler {
public:
    void add_to_queue(Process *p) override;
    Process* get_next_process() override;

};
void LCFS::add_to_queue(Process *p)  {
    queue.push_back(p);
}
Process* LCFS::get_next_process()  {
    if (queue.empty())
        return nullptr;
    vector<Process *>::iterator it = queue.end()-1;
    Process *proc = *it;
    queue.erase(it);
    return proc;
}

class SRTF: public Scheduler {
public:
    void add_to_queue(Process *p) override;
    Process* get_next_process() override;
};
void SRTF::add_to_queue(Process *p)  {
      queue.push_back(p);
}

Process* SRTF::get_next_process()  {
    int minimum;
    int index=0;
    if (queue.empty())
        return nullptr;
    Process* proc = queue.at(0);
    minimum = queue.at(0)->totalCPU - queue.at(0)->CPUTime;
    for (int i = 1; i < queue.size(); i++) {
        if (minimum > queue.at(i)->totalCPU - queue.at(i)->CPUTime) {
            minimum = queue.at(i)->totalCPU - queue.at(i)->CPUTime;
            proc = queue.at(i);
            index = i;
        }
    }
    queue.erase(queue.begin()+index);
    return proc;
    
}
class RoundRobin: public Scheduler {
public:
    void add_to_queue(Process *p) override;
    Process* get_next_process() override;
};
void RoundRobin::add_to_queue(Process *p)  {
    queue.push_back(p);
}
Process* RoundRobin::get_next_process()  {
    if (queue.empty())
        return nullptr;
    vector<Process *>::iterator it = queue.begin();
    Process *proc = *it;
    queue.erase(it);
    return proc;
}
class PrioSched: public Scheduler {
private:
    vector<vector<Process *>> active;
    vector<vector<Process *>> expire;
    int priorite;
    
public:
    void add_to_queue(Process *p) override;
    Process* get_next_process() override;
    PrioSched(int priority);
    bool checkAllActiveEmpty();
    bool checkAllExpireEmpty();
    
};
//Constructor to create a 2D vector at runtime
PrioSched::PrioSched(int priority) : priorite(priority) {
   for (int i = 0; i < priorite; i++) {
       vector<Process *> temp;
       active.push_back(temp);
       expire.push_back(temp);
   }
}
void PrioSched::add_to_queue(Process *p)  {
   
    if (p->dynamicPrio == -1) {
        p->dynamicPrio = p->staticPrio-1; 
        expire[p->dynamicPrio].push_back(p);
    } else {
        active[p->dynamicPrio].push_back(p);
        //p->dynamicPrio--;
    } 
    
}
bool PrioSched::checkAllActiveEmpty() {
    for (int d = 0; d < active.size(); d++) {
        if (!active[d].empty()) {
            return false;
        } else {
            continue;
        }
    }
    return true;

}
bool PrioSched::checkAllExpireEmpty() {
    for (int d = 0; d < expire.size(); d++) {
        if (!expire[d].empty()) {
            return false;
        } else {
            continue;
        }
    }
    return true;

}
Process* PrioSched::get_next_process()  {
    Process *proc;
    if (checkAllActiveEmpty() && checkAllExpireEmpty()) {
        return nullptr;
    }
 
    if (checkAllActiveEmpty()) {
        for (int i = 0; i < active.size(); i++) {
            active[i].swap(expire[i]);
        }
    }
    for (int i = priorite-1; i >= 0; i--) {
        if (!active[i].empty()) {
            proc = active[i].front();
            active[i].erase(active[i].begin());
            return proc;
        }
    }

}
class PreemptSched: public Scheduler {
public:
    void add_to_queue(Process *p) override;
    Process* get_next_process() override;
    PreemptSched(int priority,int quantum);
    bool checkAllActiveEmpty();
    bool checkAllExpireEmpty();

private:
    vector<vector<Process *>> active;
    vector<vector<Process *>> expire;
    int priorite;  //initialize this with the maxprios (levels of the 2d vector)
    int quant;
};
//Constructor to create a 2D vector at runtime
PreemptSched::PreemptSched(int priority, int quantum) : priorite(priority), quant(quantum) {
   for (int i = 0; i < priorite; i++) {
       vector<Process *> temp;
       active.push_back(temp);
       expire.push_back(temp);
   }
}
bool PreemptSched::checkAllActiveEmpty() {
    for (int d = 0; d < active.size(); d++) {
        if (!active[d].empty()) {
            return false;
        } else {
            continue;
        }
    }
    return true;

}
bool PreemptSched::checkAllExpireEmpty() {
    for (int d = 0; d < expire.size(); d++) {
        if (!expire[d].empty()) {
            return false;
        } else {
            continue;
        }
    }
    return true;

}


void PreemptSched::add_to_queue(Process *p)  {
   if (p->dynamicPrio == -1) {
        p->dynamicPrio = p->staticPrio-1; 
        expire[p->dynamicPrio].push_back(p);
    } else {
        active[p->dynamicPrio].push_back(p);
        //p->dynamicPrio--;
    } 

    if (curRunningProcess != nullptr && p->dynamicPrio > curRunningProcess->dynamicPrio &&
    currentTime != sim.rmEventTime()) {
        curRunningProcess->burstRemaining = curRunningProcess->burstRemaining +quant-(currentTime - curRunningProcess->processRunStartTime);//when preempted, this process in the future gets a full quantum
        curRunningProcess->CPUTime -= quant;
        curRunningProcess->CPUTime = curRunningProcess->CPUTime + (currentTime - curRunningProcess->processRunStartTime);
        sim.removeEvent();
        Event event(curRunningProcess->pid,currentTime,STATE_PREEMPT);   
        event.startTime = currentTime;
        event.prevState = STATE_RUNNING;
        sim.setEvent(event);
        
        return;  
    }
}

Process* PreemptSched::get_next_process()  {
   Process *proc;
    if (checkAllActiveEmpty() && checkAllExpireEmpty()) {
        return nullptr;
    }
    if (checkAllActiveEmpty()) {
        for (int i = 0; i < active.size(); i++) {
            active[i].swap(expire[i]);
        }
    }
    for (int i = priorite-1; i >= 0; i--) {
        if (!active[i].empty()) {
            proc = active[i].front();
            active[i].erase(active[i].begin());
            return proc;
        }
    }
    
}
//Global Variables
vector<int> randomNumbers;
vector<Process> p;
ifstream file;
int offset = -1;
bool vflag = false;

int myRandom(int burst) {
    offset += 1;
     //wrap-around
    if (offset == randomNumbers.size()) {
        offset = 0;
    }
    return 1 + (randomNumbers[offset] % burst);
}

void readInputFile(char* fileName,int maxprios) {
    file.open(fileName);
    string store;
    int id = 0;  
    while (getline(file,store)) {
        int stat = myRandom(maxprios);  //res is the static priority
        int dyn = stat - 1; //initial dynamic priority for the process
        Process process(id,stat,dyn);
        id++;
        stringstream stream(store);
        string token;
        int i = 1;

        while (stream >> token) {
            if (i == 1) { 
                process.setArrival(stoi(token));
                i++;
            } else if (i == 2) {
                process.setTotalCPU(stoi(token));
                i++;
            } else if (i == 3) {
                process.setCPUBurst(stoi(token));
                i++;
            } else if (i == 4) {
                process.setIOBurst(stoi(token));
                i++;
            }
        }
        p.push_back(process);
    }

    file.close();
}
void readRandomFile(char* fileName) {
    file.open(fileName);
    string store;
    getline(file,store); //skip the first line because it just contains count of values in randfile
    while (getline(file,store)) {
        int temp = stoi(store);
        randomNumbers.push_back(temp);
    }
    file.close();
}
// argv contains ./executable -v -sR5 inputfile randfile, argc=5
int main(int argc, char* argv[]) {
    int option;
    Scheduler* sched = nullptr;
    char* s = nullptr;  //argument passed for -s
    int quantum;
    int maxprios = 4; //default is 4
    string schedulerType;


    while ((option = getopt(argc,argv,"vs:")) != -1) {
        switch (option) {
            case 'v':
                vflag = true; 
                break;
            case 's':
                s = optarg;
                break;
            case '?': 
                if (optopt == 's') {
                    fprintf(stderr,"Option -%s requires an argument. \n",optopt);
                
                } else if (isprint (optopt)) {
                    fprintf(stderr,"Unknown option `-%c'.\n",optopt);

                } else {
                    fprintf(stderr,"Unknown option character `\\x%x'.\n",optopt);
                    return 1;
                }
                default: 
                    abort();
        }
    }
    
    string str(s);
    int colonIndex = str.find(":");
    colonIndex -= 1;
    string q = str.substr(1,colonIndex); //extract quantum info? Not sure if this is correct
    string plvl = str.substr(colonIndex+2,string::npos); //extract num of priority levels (maxprio)
    int colonIndex1 = str.find(":");
    //the previous stuff only works if there is a colon with quant and prio info
    if (colonIndex1 == string::npos) {  //no prio info
        q = str.substr(1);
        plvl = "4";
    }

    if (s[0] == 'F') {
        schedulerType = "FCFS";
        quantum = 10000;
        maxprios = 4;
        sched = new FCFS();  
    } else if (s[0] == 'L') {
        schedulerType = "LCFS";
        quantum = 10000;
        maxprios = 4;
        sched = new LCFS();
    } else if (s[0] == 'S') {
        schedulerType = "SRTF";
        quantum = 10000;
        maxprios = 4;
        sched = new SRTF();
    } else if (s[0] == 'R') {
        schedulerType = "RR";
        quantum = stoi(q);
        maxprios = 4;
        sched = new RoundRobin();
    } else if (s[0] == 'P') {
        schedulerType = "PRIO";
        quantum = stoi(q);
        maxprios = stoi(plvl); 
        sched = new PrioSched(maxprios);  
    } else if (s[0] == 'E') {
        schedulerType = "PREPRIO";
        quantum = stoi(q);
        maxprios = stoi(plvl);
        sched = new PreemptSched(maxprios,quantum);
    }

    char* input = argv[optind];
    char* randinput = argv[optind+1];
    readRandomFile(randinput);
    readInputFile(input,maxprios);
    

    
    for (int i = 0; i < p.size(); i++) {
        Event event(p.at(i).pid, p.at(i).arrivalTime, STATE_READY);
        event.prevState = STATE_CREATED;
        event.startTime = p.at(i).arrivalTime;
        sim.setEvent(event);
    }
    
    bool callScheduler = false;
    int lastEventFinishingTime = 0;
    int burst = -1; //cpu burst
    int ioburst = -1;
    int remTime = -1;
    int id = 0;
    int totalTurnaroundTime = 0;  //total turn around time where TT =finishing time - arrival time
    int cpuWaitingTime = 0; //total time in ready state
    int totalCPUTime = 0; 
    int totalIOTime = 0; // total time in blocked state for a process 
    /* IO related variables */
    bool ioStatus = false;  //this is 0 if there is no I/O going on at all, 1 if at least one process is in I/O 
    int counterIO = 0; //counter for num processes that are in I/O
    int ioStart = 0; //time when a first process enters I/O
    int ioUtilization = 0; //for calculating I/O Utilization (total time that any one process is performing I/O)
   

    
    for (Event e = sim.getEvent(); e.pid != -1; e = sim.getEvent()) {
        currentTime = e.timeStamp;
        id = e.pid;

        //from created,blocked
        if (e.newState == STATE_READY) {
            if (vflag) {
                cout << e.timeStamp << " " << e.pid << " " << currentTime - e.startTime
                 << ": " << e.getState(e.prevState) << " " << "->" << " " <<  "READY" << endl;
            }
            if (curBlockedProcess == &p[id]) {
                curBlockedProcess = nullptr;
            }
            p[id].ready_commence=currentTime;
            
            if (e.prevState == STATE_BLOCKED) {
                counterIO = counterIO - 1;
                if (counterIO == 0) {
                    ioStatus = false;
                    ioUtilization += currentTime - ioStart;
                }
            }

            
            sched->add_to_queue(&p[id]);
            callScheduler = true;

        } else if (e.newState == STATE_RUNNING) {

            

            p[id].CPUWaitingTime += currentTime - p[id].ready_commence;
            curRunningProcess = &p[id];
            // create new global for storing the process's start time
            p[id].processRunStartTime = currentTime;
            
            if (p[id].burstRemaining > 0) {
                burst = p[id].burstRemaining;
                p[id].burstRemaining = 0;  
            } else {
                burst = myRandom(p[id].CPUBurst);
            }
            remTime = p[id].totalCPU - p[id].CPUTime;

            if (burst >= remTime) {
                burst = remTime;
            }
            //create preempt event
            if (burst > quantum) {
                p[id].burstRemaining = burst - quantum;
                p[id].CPUTime += quantum;
                Event event(id,currentTime + quantum, STATE_PREEMPT);
                event.startTime = currentTime;
                event.prevState = STATE_RUNNING;
                sim.setEvent(event);
                
                

             } else if (p[id].CPUTime + burst == p[id].totalCPU) {  //won't ever be greater than totalcpu because remtime accounted for 
                
                p[id].CPUTime += burst;
                Event event(id,currentTime + burst, STATE_FINISH);
                event.startTime = currentTime;
                event.prevState = STATE_RUNNING;
                sim.setEvent(event);

            } else if (p[id].CPUTime + burst < p[id].totalCPU) { //create blocking
                p[id].CPUTime += burst;
                Event event(id,currentTime + burst, STATE_BLOCKED);
                event.startTime = currentTime;
                event.prevState = STATE_RUNNING;
                sim.setEvent(event);
            }
              
           if (vflag) {
                cout << e.timeStamp << " " << e.pid << " " << currentTime - p[id].ready_commence
                 << ": " << e.getState(e.prevState) << " " << "-> " <<  "RUNNG " << "cb=" << burst 
                 << " " << "rem=" << remTime 
                 << " " << "prio=" << p[id].dynamicPrio << endl;
            }
              //delete and see if still works  (45/64 not as gd)
            //curRunningProcess->dynamicPrio--;

        } else if (e.newState == STATE_BLOCKED) {

            curBlockedProcess = &p[id];
            curRunningProcess = nullptr;
           
            counterIO++;
            if (ioStatus == 0) {
                ioStatus = true;
                ioStart = currentTime;
            }
            

            ioburst = myRandom(p[id].IOBurst);
            p[id].IOTime += ioburst;
            remTime = p[id].totalCPU - p[id].CPUTime;



            //after IO finished, reset dynamic priority to static priority - 1   not sure

             //delete and see if still works  (45/64 not as good)
           p[id].dynamicPrio = p[id].staticPrio - 1;



            //creating event for when process is ready again
            Event event(id,currentTime + ioburst,STATE_READY);
            event.startTime = currentTime;
            event.prevState = STATE_BLOCKED;
            sim.setEvent(event);
           
            if (vflag) {
                cout << e.timeStamp << " " << e.pid << " " << currentTime - e.startTime
                 << ": " << e.getState(e.prevState) << " -> " <<  "BLOCK "  << "ib=" << ioburst 
                 << " " << "rem=" << remTime << endl;
            }

            callScheduler = true;
        } else if (e.newState == STATE_PREEMPT) {
            p[id].ready_commence = currentTime;
           
            remTime = p[id].totalCPU - p[id].CPUTime;
            if (vflag) {
                cout << e.timeStamp << " " << e.pid << " " << currentTime - p[id].processRunStartTime  //or e.startTime?
                << ": " << e.getState(e.prevState) << " -> " <<  "READY " << "cb=" 
                << p[id].burstRemaining 
                << " rem=" << remTime 
                << " prio=" << p[id].dynamicPrio << endl;
            }

            

            curRunningProcess->dynamicPrio--;
            curRunningProcess = nullptr;
            
            
            sched->add_to_queue(&p[id]);

                

            callScheduler = true;
        } else if (e.newState == STATE_FINISH) {
            p[id].finishTime = currentTime;
            p[id].turnAroundTime = p[id].finishTime - p[id].arrivalTime;
            lastEventFinishingTime = currentTime;
            curRunningProcess = nullptr;
            if (vflag) {
                cout << e.timeStamp << " " << e.pid << " " << currentTime - e.startTime
                 << ": " << e.getState(e.prevState) << " -> " <<  " FINISH" << endl;
            }
            callScheduler = true;
        }
    
        if (callScheduler) {
            if (sim.getNextTime() == currentTime) {
                continue;
            }
            callScheduler = false;
            if (curRunningProcess == nullptr) {
                curRunningProcess = sched->get_next_process();
                
                if (curRunningProcess == nullptr) {
                    continue;
                }
                
                Event event(curRunningProcess->pid,currentTime,STATE_RUNNING);
                event.prevState = STATE_READY;
                event.startTime = currentTime;
                sim.setEvent(event);
                

            }
        }
    }

    //Printing of results
    if (schedulerType == "RR" || schedulerType == "PRIO" || schedulerType == "PREPRIO") 
        cout << schedulerType << " " << q << endl;
    else
        cout << schedulerType << endl;
    for (int k = 0; k < p.size(); k++) {
        totalTurnaroundTime += p[k].turnAroundTime;
        cpuWaitingTime += p[k].CPUWaitingTime;
        totalCPUTime += p[k].CPUTime;
        totalIOTime += p[k].IOTime;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", p[k].pid,p[k].arrivalTime,p[k].totalCPU,
        p[k].CPUBurst,p[k].IOBurst,p[k].staticPrio,p[k].finishTime,p[k].turnAroundTime,p[k].IOTime,p[k].CPUWaitingTime);
    }

    //CPU Utilization
    double cpuUtil = ((double)totalCPUTime/lastEventFinishingTime) * 100;
    //IO Utilization
    double ioUtil = ((double)ioUtilization/lastEventFinishingTime) * 100;
    //  Average Turnaround time among processes
    double avgturnaround = (double)totalTurnaroundTime/p.size();
    //Average cpu waiting time among processes
    double avgcpuwait = (double)cpuWaitingTime/p.size();
    //Throughput of number processes per 100 time units
    double throughput = ((double)p.size()/lastEventFinishingTime) * 100;

    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",lastEventFinishingTime
    ,cpuUtil,ioUtil,avgturnaround,avgcpuwait,throughput);


    delete sched; 
    return 0;
}

