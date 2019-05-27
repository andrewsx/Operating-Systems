#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <unistd.h>
#include <map>
#include <climits>
#include <algorithm>
using namespace std;

bool verbose = false;
int clockTime = 1;
int totalTime = 0;
int totalMovement = 0;
double avgTurnAround = 0.0;
double avgWaitTime = 0.0;
int maxWaitTime = INT_MIN;
bool wait = false;  //when there are no io requests in the queue, track is not moving and waiting for a new request to come in when arrivaltime = currenttime
//bool flag = false; // for the wait, when we need to decide whether to enter setHiLo or not


struct IORequest {
    int index;
    int arrivalTime;
    int position;
    int startTime;
    int endTime;
    IORequest() :  arrivalTime(-1), position(-1), startTime(-1), endTime(-1), index(-1) {}
    IORequest(int arrive, int pos, int start, int end, int i) : arrivalTime(arrive), position(pos),startTime(-1), endTime(-1), index(i) {}
};

//Globals
vector<IORequest> requests;

class Base {

public:
    int track = 0;
    vector<IORequest> active;
    virtual bool is_empty() = 0;
    virtual void insertRequest(int index) = 0;
    virtual void processRequest() = 0;
    
};

class FIFO: public Base {
private: 
    
public:
    virtual bool is_empty() override;
    virtual void insertRequest(int index) override;
    virtual void processRequest() override;
    IORequest *findRequest(int id);
};
bool FIFO::is_empty() {
    if (active.size() == 0) {
        return true;
    } else {
        return false;
    }
}

void FIFO::insertRequest(int index)  { 
    IORequest req = requests[index];
    active.push_back(req);
    if (verbose) {
        cout << clockTime << ":\t" << index << " add " << requests[index].position << endl;
    }
}

IORequest* FIFO::findRequest(int id) {
    for (int i = 0; i < requests.size(); i++) {
        if (id == requests[i].index) {
            return &(requests[i]);
        }
    }
}

void FIFO::processRequest() {
    IORequest *req = nullptr;
    IORequest *cur = nullptr;
    int reqID = -1;
    while (active.size() != 0) {
        req = &(active[0]);
        reqID = req->index;
        cur = findRequest(reqID);
        if (verbose) {
            cout << clockTime << ":\t" << cur->index << " issue " << cur->position << " " << track << endl;
        }
        if (req->startTime == -1) {
            req->startTime = clockTime;
           cur->startTime = clockTime;
        }
        if (req->position > track) {
            track++;
            totalMovement++;
            return;
        } else if (req->position < track) {
            track--;
            totalMovement++;
            return;
        } else {   //if track == req->position
            req->endTime = clockTime;
          cur->endTime = clockTime;
          if (verbose) {
              cout << clockTime << ":\t" << cur->index << " finish " << cur->endTime - cur->arrivalTime << endl;
          }
            active.erase(active.begin());
        }
    }
}

class SSTF: public Base {
private: 
    int idx = 0;
public:
    virtual bool is_empty() override;
    virtual void insertRequest(int index) override;
    virtual void processRequest() override;
    int getShortest();
    IORequest *findRequest(int id);
};
bool SSTF::is_empty() {
    if (active.size() == 0) {
        return true;
    } else {
        return false;
    }
}
int SSTF::getShortest() {
    int shortestIndex = -1;
    int shortestDist = INT_MAX;
    for (int i = 0; i < active.size(); i++) {
        int dist = abs(active[i].position - track);
        if (shortestDist > dist) {
            shortestDist = dist;
            shortestIndex = i;
        }
    }
    return shortestIndex;
}


IORequest* SSTF::findRequest(int id) {
    for (int i = 0; i < requests.size(); i++) {
        if (id == requests[i].index) {
            return &(requests[i]);
        }
    }
}
void SSTF::insertRequest(int index)  { 
    IORequest req = requests[index];
    active.push_back(req);
    if (verbose) {
        cout << clockTime << ":\t" << index << " add " << requests[index].position << endl;
    }
}

void SSTF::processRequest() {
    IORequest *req = nullptr;
    IORequest *cur = nullptr;
    int reqID = -1;
    while (active.size() != 0) {
        if (idx == -1) {
            idx = getShortest();
        }
        req = &(active[idx]);
        reqID = req->index;
        cur = findRequest(reqID);
         if (verbose) {
            cout << clockTime << ":\t" << cur->index<< " issue " << cur->position << " " << track << endl;
        }
        if (req->startTime == -1) {
            cur->startTime = clockTime;
            req->startTime = clockTime;
        }
        if (req->position > track) {
            track++;
            totalMovement++;
            return;
        } else if (req->position < track) {
            track--;
            totalMovement++;
            return;
        } else {   //if track == req->position
            req->endTime = clockTime;
            cur->endTime = clockTime;
            if (verbose) {
              cout << clockTime << ":\t" << cur->index << " finish " << cur->endTime - cur->arrivalTime << endl;
          }
            active.erase(active.begin() + idx);
            idx = getShortest();
        }
    }
}



class LOOK: public Base {
private:
    IORequest *activeReq = nullptr;
    IORequest *activeFromGlobal = nullptr;
    bool direction = true;
    int highest = INT_MIN;
    int lowest = INT_MAX;
    
public:
    virtual bool is_empty() override;
    virtual void insertRequest(int index) override;
    virtual void processRequest() override;
    void setHiLo();
    IORequest *getNextInstruc();
    IORequest *findRequest(int id);

LOOK() {
    active.reserve(1000);
}
};

IORequest *LOOK::getNextInstruc() {
    int min = INT_MAX;  // current smallest distance 
    IORequest *req = nullptr;
    int diff = -1;  //dist between current track and track request position 
    
    setHiLo();

    //cout << "HIGHEST: " << highest << " LOWEST: " << lowest << endl;
    if (active.size() > 1) {
        if (track >= highest) {
            direction = false;
        }
        if (track <= lowest) {
            direction = true;
        }
    } else if (active.size() == 1) {
        if (active[0].position > track) {
            direction = true;
        } else if (active[0].position < track) {
            direction = false;
        }
    } 
    for (int i = 0; i < active.size(); i++) {
        if (direction) {
            diff = abs(active[i].position - track);
        //    cout << "DIR: " << direction << "POS: " << active[i].position << " TRK: " << track << " DIFF: " << diff << " MIN: " << min << endl;
            if (active[i].position >= track && diff < min) {
                
                req = &(active[i]);
                min = diff;
            } 

        } else {
             
            diff = abs(active[i].position - track);
      //      cout << "DIR: " << direction << "POS: " << active[i].position << " TRK: " << track << " DIFF: " << diff << " MIN: " << min << endl;
            if (active[i].position <= track && diff < min) {
                //lastOne = i;
                // cout << "POS: " << active[i].position << " TRK: " << track << " DIFF: " << diff << " MIN: " << min << endl;
                req = &(active[i]);
                min = diff;
            } 
        }
    }
    //cout << "SIZE OF ACTIVE: " << active.size() << endl;
    
    return req;
}

/*Set the highest and lowest with index of IO request */
void LOOK::setHiLo() {

    int high = INT_MIN;
    int low = INT_MAX;
    for (int i = 0; i < active.size(); i++) {
        if (active[i].position > high) {
            high = active[i].position;
        }
    }
    for (int i = 0; i < active.size(); i++) {
        if (active[i].position < low) {
            low = active[i].position;
        }
    }

    highest = high;
    lowest = low;
    
    
}

bool LOOK::is_empty() {
     if (active.size() == 0) {
        return true;
    } else {
        return false;
    }
}
IORequest* LOOK::findRequest(int id) {
    for (int i = 0; i < requests.size(); i++) {
        if (id == requests[i].index) {
            return &(requests[i]);
        }
    }
}
void LOOK::insertRequest(int index)  { 
    IORequest req = requests[index];
    
    active.push_back(req);
    
    if (verbose) {
        cout << clockTime << ":\t" << index << " add " << requests[index].position << endl;
    }
}



void LOOK::processRequest() {
    IORequest *req = nullptr;
    IORequest *cur = nullptr;
    int reqID = -1;
    while (active.size() != 0) {
        if (activeReq == nullptr) {
           req = getNextInstruc();
           if (req == nullptr)               
                return;
            activeReq = req;
            reqID = req->index;
            cur = findRequest(reqID);
            activeFromGlobal = cur;
            if (verbose) {
                cout << clockTime << ":\t" << cur->index << " NEW issue " << cur->position << " " << track << endl;
            }
        } 
        req = activeReq;
        cur = activeFromGlobal;
        if (verbose) {
                cout << clockTime << ":\t" << cur->index << " OLD issue " << cur->position << " " << track << endl;
        }
        reqID = req->index;
        if (req->startTime == -1) {
            cur->startTime = clockTime;
            req->startTime = clockTime;
        }
       
        if (req->position > track) {
            track++;
            totalMovement++;
           return;
        } else if (req->position < track) {
            track--;
            totalMovement++;
            return;
        } else {   
            req->endTime = clockTime;
            cur->endTime = clockTime;
            if (verbose) {
              cout << clockTime << ":\t" << cur->index << " finish " << cur->endTime - cur->arrivalTime << endl;
            }
         
            for (int i = 0; i < active.size(); i++) {
                if (active[i].index == reqID) {
                    active.erase(active.begin() + i);
                    break;
                }
                
            }
           

            activeReq = nullptr;
            activeFromGlobal = nullptr;
           
            
        }

    }
}


class CLOOK: public Base {
private: 
    int highest = INT_MIN;
    int lowest = INT_MAX;
    bool direction = true;
    IORequest *activeReq = nullptr;
    IORequest *activeFromGlobal = nullptr;
    int timeDifference = -1;
public:
    virtual bool is_empty() override;
    virtual void insertRequest(int index) override;
    virtual void processRequest() override;
    void setHiLo();
    IORequest *getNextInstruc();
    IORequest *findRequest(int id);
CLOOK() {
    active.reserve(1000);
}
};

bool CLOOK::is_empty() {
    if (active.size() == 0) {
        return true;
    } else {
        return false;
    }
}

/*Set the highest  index of IO request */
void CLOOK::setHiLo() {
    int high = INT_MIN;
    int low = INT_MAX;
    for (int i = 0; i < active.size(); i++) {
        if (active[i].position > high) {
            high = active[i].position;
        }
    }
    for (int i = 0; i < active.size(); i++) {
        if (active[i].position < low) {
            low = active[i].position;
        }
    }

    highest = high;
    lowest = low;
    
}
IORequest *CLOOK::getNextInstruc() {
    // cout << "GETNEXT INSTRUC" << endl;
    int min = INT_MAX;  // current smallest distance 
    IORequest *req = nullptr;
    int diff = -1;  //dist between current track and track request position 

    setHiLo();
    if (track >= highest) {
       // cout << "CHG Direction to false" << endl;
        //timeDifference = highest - lowest;
        direction = false;
    } else if (track <= lowest) {
       // cout << "CHG Direction to true" << endl;
        direction = true;
        //if track is neither greater than or lower than lowest/highest then check if there is something greater than it. if so, set direction to true
    } else { 
        for (int i = 0; i < active.size(); i++) {
            if (track < active[i].position) {
                direction = true;
                break;
            }
        }
    }
   // cout << "ARRIVE BEFORE GETTING MIN REQ" << endl;
    for (int i = 0; i < active.size(); i++) {
        if (direction) {
          //  cout << "DIR IS TRUE" << endl;
            diff = abs(active[i].position - track);
            if (active[i].position >= track && diff < min) {
                req = &(active[i]);
                min = diff;
            } 
        
        } else {   // retrun lowest position request
          //  cout << "DIR IS FALSE" << endl;
            if (active[i].position == lowest) {
                req = &(active[i]);
                break;
            }
            
        }
    }
    return req;
}

void CLOOK::insertRequest(int index) { 
    IORequest req = requests[index];
    active.push_back(req);
    if (verbose) {
        cout << clockTime << ":\t" << index << " add " << requests[index].position << endl;
    }
}

IORequest* CLOOK::findRequest(int id) {
    for (int i = 0; i < requests.size(); i++) {
        if (id == requests[i].index) {
            return &(requests[i]);
        }
    }
}


void CLOOK::processRequest() {
  //  cout << "PROCESS REQ" << endl;
    IORequest *req = nullptr;
    IORequest *cur = nullptr;
    int reqID = -1;
    while (active.size() != 0) {
      //  cout << "ENTER WHILE, active has something" << active.size() << endl;
        if (activeReq == nullptr) {
         //   cout << "ACTIVE REQ HAS SOMETHING" << endl;
            req = getNextInstruc();
            
            if (req == nullptr) {
                return;
            }
            activeReq = req;
            reqID = req->index;
            cur = findRequest(reqID);
            activeFromGlobal = cur;
            if (verbose) {
                cout << clockTime << ":\t" << cur->index << " issue " << cur->position << " " << track << endl;
            }
        } 
        req = activeReq;
        cur = activeFromGlobal;
        if (verbose) {
            cout << clockTime << ":\t" << cur->index << " issue " << cur->position << " " << track << endl;
        }
        reqID = req->index;
        if (req->startTime == -1) {
            cur->startTime = clockTime;
            req->startTime = clockTime;
        }
        if (req->position > track) {
        //    cout << "ACTIVEREQ POSITION > TRACK " << endl;
            track++;
            totalMovement++;
           return;
        } else if (req->position < track) {
        //    cout << "ACTIVEREQ POSITION < TRACK " << endl;
            track--;
            totalMovement++;
            return;
        } else {   //if track == req->position
      //  cout << "ACTIVEREQ POSITION == TRACK " << endl;
            req->endTime = clockTime;
            cur->endTime = clockTime;
            if (verbose) {
              cout << clockTime << ":\t" << cur->index << " finish " << cur->endTime - cur->arrivalTime << endl;
            }


            // for (int i = 0; i < active.size(); i++) {
            //     cout << "BEFORE DEL:" << " Index:" << active[i].index << " ArrivalTime: " << active[i].arrivalTime
            //     << " StartTime " << active[i].startTime << " End Time " << active[i].endTime << endl;
            // }
            for (int i = 0; i < active.size(); i++) {
              
                if (active[i].index == reqID) {
                    active.erase(active.begin() + i);
                    break;
                }
                
            }

            // for (int i = 0; i < active.size(); i++) {
            //     cout << "AFTER DEL:" << " Index:" << active[i].index << " ArrivalTime: " << active[i].arrivalTime
            //     << " StartTime " << active[i].startTime << " End Time " << active[i].endTime << endl;
            // }
            
            activeReq = nullptr;
            activeFromGlobal = nullptr;
        }

    }
}


/* all incoming requests go into wait. when run is empty, swap. you finish
all the requests from run before processing requests from wait  */

class FLOOK: public Base {
private: 
    int run = 1;   
    int highest = INT_MIN;
    int lowest = INT_MAX;
    vector<IORequest> queues[2];
    bool direction = true;
    IORequest *activeReq = nullptr;
    IORequest *activeFromGlobal = nullptr;

public:
     virtual bool is_empty() override;
     virtual void insertRequest(int index) override;
     virtual void processRequest() override;
     void setHiLo();
     IORequest* findRequest(int id);
     IORequest* getNextInstruc();

FLOOK() {
    queues[0].reserve(1000);
    queues[1].reserve(1000);
}
    

};


bool FLOOK::is_empty() {
    return (queues[run].empty() && queues[!run].empty());
}
void FLOOK::insertRequest(int index)  { 
  //  cout << "Inside insertRequest() " << endl;
    IORequest req = requests[index];
    queues[!run].push_back(req);
    if (verbose) {
        cout << clockTime << ":\t" << index << " add " << requests[index].position << endl;
    }
}

IORequest* FLOOK::findRequest(int id) {
  //  cout << "Inside finerequest() " << endl;
    for (int i = 0; i < requests.size(); i++) {
        if (id == requests[i].index) {
            return &(requests[i]);
        }
    }
}


IORequest *FLOOK::getNextInstruc() {
    //cout << "Inside getNextInstruc() " << endl;
    IORequest *req;
    int min = INT_MAX;  // current smallest distance 
    int diff = -1;  //dist between current track and track request position 

    setHiLo();
    if (queues[run].size() > 1) {
        if (track >= highest) {
            direction = false;
        }
        if (track <= lowest) {
            direction = true;
        }
    } else if (queues[run].size() == 1) {
        if (queues[run][0].position > track) {
            direction = true;
        } else if (queues[run][0].position < track) {
            direction = false;
        }
    }
    for (int i = 0; i < queues[run].size(); i++) {
        if (direction) {
            //process requests from run before process requests from wait
            if (queues[run].size() == 0) {
                if (queues[!run].empty()) {
                    return nullptr;
                } else {
                    run = !run;
                }
            } 
            diff = abs(queues[run][i].position - track);
            if (queues[run][i].position >= track && diff < min) {
                req = &(queues[run][i]);
                min = diff;
            } 
        } else {
            if (queues[run].size() == 0) {
                if (queues[!run].empty()) {
                    return nullptr;
                } else {
                    run = !run;
                }
            } 
            diff = abs(queues[run][i].position - track);
            if (queues[run][i].position <= track && diff < min) {
            
                req = &(queues[run][i]);
                min = diff;
            } 
        }
            
    }
   
    return req;
}
void FLOOK::setHiLo() {
   // cout << "Inside Set HILO " << endl;
    int high = INT_MIN;
    int low = INT_MAX;
    for (int i = 0; i < queues[run].size(); i++) {
        if (queues[run][i].position > high) {
            high = queues[run][i].position;
        }
    }
    for (int i = 0; i < queues[run].size(); i++) {
        if (queues[run][i].position < low) {
            low = queues[run][i].position;
        }
    }

    highest = high;
    lowest = low;
}

void FLOOK::processRequest() {
    IORequest *req = nullptr;
    IORequest *cur = nullptr;
    int reqID = -1;
    // cout << "Inside processRequest() " << endl;
    // cout << "Track: " << track << "ClockTime: " << clockTime << endl;
    //in first iteration, there won't be anything in runqueue so swap
    if (queues[run].size() == 0) {
        run = !run;
    }
    while (queues[run].size() != 0) {
        //cout << "inside While Loop" << endl;
        if (activeReq == nullptr) {
            req = getNextInstruc();
            
            if (req == nullptr) {
                return;
            }
            activeReq = req;
            reqID = req->index;
            cur = findRequest(reqID);
            activeFromGlobal = cur;
            if (verbose) {
                cout << clockTime << ":\t" << cur->index << " issue " << cur->position << " " << track << endl;
            }
        }
        req = activeReq;
        cur = activeFromGlobal;
        if (verbose) {
                cout << clockTime << ":\t" << cur->index << " issue " << cur->position << " " << track << endl;
        }
        reqID = req->index;
        if (req->startTime == -1) {
            cur->startTime = clockTime;
            req->startTime = clockTime;
        }
        if (req->position > track) {
            track++;
            totalMovement++;
            return;
        } else if (req->position < track) {
            track--;
            totalMovement++;
            return;
        } else {   //if track == req->position
            req->endTime = clockTime;
            cur->endTime = clockTime;
            if (verbose) {
              cout << clockTime << ":\t" << cur->index << " finish " <<  cur->endTime - cur->arrivalTime << endl;
            }
            for (int i = 0; i < queues[run].size(); i++) {
                if (queues[run][i].index == reqID) {
                    queues[run].erase(queues[run].begin() + i);
                }
            }
            //check if queues[run] is empty. if so swap.
            if (queues[run].empty()) {
                run = !run;
            }
            
            activeReq = nullptr;
            activeFromGlobal = nullptr;

        }
    }
}



void readInputFile(char *input) {
    ifstream file;
    file.open(input);
    string store;
    int arrival = 0;
    int position = 0;
    int i = 0;
    
    while (getline(file,store)) {
        if (store[0] == '#') {
            continue;
        } else {
            stringstream stream(store);
            stream >> arrival >> position;
            IORequest req = IORequest(arrival,position,-1,-1,i++);
            requests.push_back(req);
        }
    }

    //print out the requests for checking 
    /*
    for (int i = 0; i < requests.size(); i++) {
        cout << "Request Index" << requests[i].index << endl << 
        "Request arrive time to system: " <<
        requests[i].arrivalTime << endl <<
         "Request track position: " << requests[i].position << endl 
        << "Request start time: " <<
        requests[i].startTime << endl <<
        "Request end time: " << requests[i].endTime << endl;
    }*/
    

    file.close();
}

void simulate(Base *algo) {
    int totalWaitTime = 0;
    int totalTurnAround = 0;
    int index = 0;
    IORequest *request = nullptr;
    
    while (!(algo->is_empty()) || !(index == requests.size())) {
        request = &(requests[index]);
        if (request->arrivalTime == clockTime) {
            if (wait) {
                wait = false;
            }
            algo->insertRequest(request->index); //pass in the io request index
            index++;
        }
        if (!wait) {
            
            algo->processRequest();
        }
        
        if (algo->is_empty() && !(index == requests.size())) {
            wait = true;
        }
        clockTime++;
        //cout << "CLOCKTIME" << clockTime << endl;
    }
    
    totalTime = clockTime - 1;
    //For each request
    for (int i = 0; i < requests.size(); i++) {
        printf("%5d: %5d %5d %5d\n",i,requests[i].arrivalTime,requests[i].startTime,requests[i].endTime);
        int requestWaitTime = requests[i].startTime - requests[i].arrivalTime;
        totalWaitTime += requests[i].startTime - requests[i].arrivalTime;
        totalTurnAround += requests[i].endTime - requests[i].arrivalTime;
        if (maxWaitTime < requestWaitTime) {
            maxWaitTime = requestWaitTime;
        }
    }

    //Summary 
    avgWaitTime = totalWaitTime / (double)requests.size();
    avgTurnAround = totalTurnAround / (double)requests.size();
    printf("SUM: %d %d %.2lf %.2lf %d\n", totalTime,totalMovement,avgTurnAround,avgWaitTime,maxWaitTime);
    
    
}

int main(int argc, char* argv[]) {
    int option;
    Base *algo;
    string choix;
    string str;
    
    while ((option = getopt(argc,argv,"s:v")) != -1) {
        switch (option) {
            case 's':
                {
                    choix = optarg; 
                    if (choix == "i") 
                        algo = new FIFO();
                    else if (choix == "j") 
                        algo = new SSTF();
                    else if (choix == "s") 
                        algo = new LOOK();
                    else if (choix == "c") 
                        algo = new CLOOK();
                    else if (choix == "f")
                        algo = new FLOOK();
                    
                }
                break;
            case 'v':
                {
                    verbose = true;
                    
                }
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
    char* input = argv[optind];   
 
    readInputFile(input);
    
    simulate(algo);

    delete algo;
    return 0;
}

