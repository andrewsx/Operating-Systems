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
using namespace std;

typedef unsigned long long ull;
typedef unsigned long ul;
const int numVirtualPages = 64;
bool out_output = false;
bool out_pagetable = false;
bool out_frametable = false;
bool out_summary = false;
bool out_ft_eachinstruc = false;
bool out_currentPT = false;
bool out_allprocesses = false;

vector<int> randomNumbers;
int numProcesses = 0;
int numFrames = 0;
int offset = -1;
int instructionCount = 0;

struct Frame {
    int index; 
    unsigned int age;  
    int time;
    int vpage;
    int procid;
    Frame() : index(0), age(0), time(-1), vpage(-1), procid(-1) {}
    Frame(int c, int a, int t, int v,int p) : index(c), age(a), time(t), vpage(v), procid(p) {}
};


vector<Frame> freeList;
vector<Frame> frameTable;  

struct Instruction {
    char command;
    int num;
    Instruction() : command(' '), num(-1) {}
    Instruction(char c, int i) : command(c), num(i) {}
    
};

class Pager {
    
public:
    virtual int getVictimFrame() = 0;
};

struct FrameTable {
    
    int maxSize;   // this is user input num of frames
    FrameTable() : maxSize(-1) {}
    FrameTable(int size) : maxSize(size) {
        numFrames = size;
    } 

    void initializeFreeList() {
        for (int i = 0; i < maxSize; i++) {
            Frame frame(i,0,-1,-1,-1);
            freeList.push_back(frame);
        }
    }

    void initializeTable() {
        for (int i = 0; i < maxSize; i++) {
            Frame frame(i,0,-1,-1,-1);
            frameTable.push_back(frame);
        }
    }

    int getFrame(Pager *alg) {   
        int frame;
        if (freeList.empty()) {
            frame = alg->getVictimFrame();
            return frame;
        }
        frame = freeList.front().index;
        freeList.erase(freeList.begin());
        return frame;
    }

} fTable;

struct PerProcessInfo {
    ul unmaps;
    ul maps;
    ul ins;
    ul outs;
    ul fins;
    ul fouts;
    ul zeros;
    ul segv;
    ul segprot;
    
    PerProcessInfo() : unmaps(0), maps(0), ins(0), outs(0), fins(0), fouts(0), zeros(0), segv(0), segprot(0) {}
};

struct PTE {
    unsigned int present:1;
    unsigned int write_protect:1;
    unsigned int modify:1; 
    unsigned int refer:1;
    unsigned int pageout:1;
    unsigned int frame:7;
    unsigned int file_map:1;
    unsigned int not_first_access:1;
    unsigned int leftover:18;   
    PTE() {
        present = 0;
        write_protect = 0;
        modify = 0;
        refer = 0;
        pageout = 0;
        frame = 0;
        file_map = 0;
        not_first_access = 0;
        leftover = 0;
    }
};
int myRandom(int size) {
    offset += 1;
     //wrap-around
    if (offset == randomNumbers.size() - 1) { 
        offset = 0;
    }
    return (randomNumbers[offset] % size);
}


class FIFO: public Pager {
private: 
    int hand = 0;
public:
    virtual int getVictimFrame() override;
};
int FIFO::getVictimFrame()  { 
    int frame = frameTable[hand].index;
    hand = (hand + 1) % numFrames;
    return frame;
}

class Random: public Pager {
public:
    virtual int getVictimFrame() override;
};

int Random::getVictimFrame()  { 
    int frameIndex = myRandom(numFrames);
    for (int i = 0; i < numFrames; i++) {
        if (frameIndex == frameTable[i].index) {
            int frame = frameTable[i].index;
            return frame;
        }
    }
    
}

struct VMA {
    int startVirtualPage;
    int endVirtualPage;
    int writeProtect;   
    int fileMapped;
    VMA() : startVirtualPage(-1), endVirtualPage(-1), writeProtect(-1), fileMapped(-1) {}
    VMA(int start, int end, int write, int fileMap) {
        startVirtualPage = start;
        endVirtualPage = end;
        writeProtect = write;  
        fileMapped = fileMap;  
    }

};

struct Process {
    int pid;
    int numVMA;
    vector<VMA> vmaList;
    PerProcessInfo info;
    vector<PTE> pageTable = vector<PTE>(numVirtualPages);   
    Process() {
        pid = -1; 
        numVMA = -1;
    }
    Process(int id, int num, vector<VMA> areas) {
        pid = id;
        numVMA = num;
        vmaList = areas;
    }
};

//Global vars
ifstream file;
vector<Instruction> instructions;
vector<Process> processes;  //pid info, num of vmas, vma info

class WorkSet: public Pager {
private:
    int hand = 0; 
    const int tau = 50;
public:
   virtual int getVictimFrame() override;

};
int WorkSet::getVictimFrame()  { 
    Frame *frame;
    PTE *pte;
    int frameIndex = hand;
    int procID = -1;
    int vpage = -1;
    int curHand = hand;
    int minimum = INT_MAX;
    
    for (int i = 0; i < numFrames; i++) {
        frame = &(frameTable[hand]);
        curHand = hand;
        hand = (hand + 1) % numFrames;
        procID = frameTable[curHand].procid;
        vpage = frameTable[curHand].vpage;
        
        pte = &(processes[procID].pageTable[vpage]);
        
        if (pte->refer == 1) {
            pte->refer = 0;
            frame->time = instructionCount;  
        } else if (pte->refer == 0 && (instructionCount - frame->time >= tau)) {
           return curHand;
        } else if (pte->refer == 0 && (instructionCount - frame->time < tau)) { 
            if (frame->time < minimum) {
                minimum = frame->time;
                frameIndex = curHand;
            }
        }
    }
    hand = (frameIndex + 1) % numFrames;
    return frameIndex;
}

class Aging: public Pager {
private:
    int hand = 0;
public:
    virtual int getVictimFrame() override;
};

int Aging::getVictimFrame()  {
    Frame *frame; 
    int procID = -1;
    int vpage = -1;
    PTE *pte;
    int index = -1;
    unsigned int min = 0xFFFFFFFF;
    for (int i = 0; i < numFrames; i++) {
        frame = &(frameTable[hand]);    
        procID = frameTable[hand].procid;
        vpage = frameTable[hand].vpage;
        pte = &(processes[procID].pageTable[vpage]);
        //move the bits one over
        frame->age = (frame->age >> 1);
        if (pte->refer == 1) {
            frame->age = (0x80000000 | frame->age);  //set first bit
            pte->refer = 0;
        }
        if(frame->age < min){
            min = frame->age;
            index = hand;
        }
        hand = (hand + 1) % numFrames;
    }
    hand = (index + 1) % numFrames;
    return index;
}

class Clock: public Pager {
private: 
    int hand = 0; 
public:
    virtual int getVictimFrame() override;
};
int Clock::getVictimFrame()  { 
   int frame = -1;
   int procid = -1;
   int pageNum = -1;
   PTE *pte;
   while (1) {
       frame = frameTable[hand].index;
       procid = frameTable[hand].procid;
       pageNum = frameTable[hand].vpage;
       hand = (hand + 1) % numFrames;
       pte = &(processes[procid].pageTable[pageNum]);
       if (pte->refer == 0) {
           return frame;
       }
       pte->refer = 0;
   }
}

class NRU: public Pager {
private:
    int hand = 0;
    int prevInstruc = 0; 
    bool checkInstructionCount();
    
      // class 0= R0 M0, class 1= R0 M1, class 2= R1 M0, class 3= R1 M1 
public:
    virtual int getVictimFrame() override;
};

bool NRU::checkInstructionCount() {
    if (instructionCount - prevInstruc > 49) {
        return true;
    } else {
        return false;
    }
}
int NRU::getVictimFrame() {
    PTE *pte;
    int k = 0;
    int procid = -1;
    int pageNum = -1;
    int classIndex = -1;
    int curHand = 0;
    int classes[4] = {-1,-1,-1,-1};   //hand is the frame index
    
    bool resetRefBit = checkInstructionCount(); 

    while (k < numFrames) {
        procid = frameTable[hand].procid;
        pageNum = frameTable[hand].vpage;
        curHand = hand;
        hand = (hand + 1) % numFrames;

        pte = &(processes[procid].pageTable[pageNum]);

        classIndex = 2*pte->refer + pte->modify;
    
        if (classes[classIndex] == -1) {
            classes[classIndex] = curHand;
        }
        if ((classIndex == 0) && (!resetRefBit)) {
            return curHand;
        }
        if (resetRefBit) {
            prevInstruc = instructionCount;
            pte->refer = 0;
        }
        k++;
    }
        
    for (int i = 0; i < 4; i++) {
        if (classes[i] != -1) {
            hand = (classes[i] + 1) % numFrames;  //set hand to next position after victim frame
            return classes[i];
        }
    }
}
struct VMABits {   
    bool insideVMA;
    int writeProtect;
    int fileMapped;
    VMABits() : insideVMA(false), writeProtect(-1), fileMapped(-1) {}
    VMABits(bool in, int write, int map) : insideVMA(in), writeProtect(write), fileMapped(map) {}
}; 

VMABits *checkInsideVMA(int vpage, Process *proc) {   
    int pIndex = proc->pid;
    VMABits *entry;
 
    for (int i = 0; i < processes.size(); i++) {
        if (pIndex == processes[i].pid) {
            for (int j = 0; j < processes[i].numVMA; j++) {
                if (vpage >= processes[i].vmaList[j].startVirtualPage && vpage <= processes[i].vmaList[j].endVirtualPage) {
                    entry = new VMABits(true,processes[i].vmaList[j].writeProtect,processes[i].vmaList[j].fileMapped);
                    return entry;
                } 
            }
        }
    }
    return nullptr;
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

void readInputFile(char* fileName) {
    file.open(fileName);
    string store;
    
    int pid = 0;
    int count = 0; //keep local track of processes count
    bool numProcessObtained = false, nextProcessFlag = false, VMADoneParseFlag = false;
    int numVMA = -1;
    int numVMALeft = -1;
    int startVirtualPage = -1, endVirtualPage = -1;
    bool writeProtect = false, fileMapped = false;
    vector<VMA> vmas;

    while (getline(file,store)) {
        if (nextProcessFlag) {  //if true then this means that we up the pid number next process
            pid = pid + 1;
            vmas.clear();
            nextProcessFlag = false;
        }
        if (store[0] == '#') {
            continue;
        }
        if (!VMADoneParseFlag && !numProcessObtained && store.length() == 1) {  //get number of processes and store in global
            numProcesses = stoi(store);
            count = stoi(store);
            numProcessObtained = true;
            continue;
        } if (!VMADoneParseFlag && numProcessObtained && store.length() == 1) {
            numVMA = store[0] - '0'; //get number of VMAs  
            numVMALeft = store[0] - '0';
            continue;
        } if (!VMADoneParseFlag && numProcessObtained && store.length() > 1) {
            stringstream stream(store);
            string token;
            
            stream >> token;  // get specs for each VMA
            startVirtualPage = stoi(token);  
            
            stream >> token;
            endVirtualPage = stoi(token);
            
            stream >> token;
            writeProtect = stoi(token);

            stream >> token;
            fileMapped = stoi(token);
            
            VMA vma(startVirtualPage, endVirtualPage, writeProtect, fileMapped);
            vmas.push_back(vma);    
            
            
            numVMALeft = numVMALeft - 1;
            if (numVMALeft != 0) {
                continue;
            } 
            else if (numVMALeft == 0 && count > 1) {
                count = count - 1;
                Process process(pid,numVMA,vmas);
                processes.push_back(process);
                nextProcessFlag = true;
                continue;
            } else if (count == 1) {
                Process process(pid,numVMA,vmas);
                processes.push_back(process);
                VMADoneParseFlag = true;
            }
            
        } if (VMADoneParseFlag && isalpha(store[0])) {
            stringstream stream(store);
            string token;
            stream >> token;
            char temp = token[0];
            stream >> token;
            int num = stoi(token);
            Instruction instr(temp, num);
            instructions.push_back(instr);
        }

       
    }

    file.close();
}

bool getNextInstruction() {
    if (instructions.size() != 0)
        return true;
    else 
        return false;
}

void simulate(FrameTable *table, Pager *algo) {
    Instruction instruc;
    Pager *algorithm = algo;
    ull totalCost = 0;
    ul contextSwitchCount = 0;
    ul processExitCount = 0;
    Process *currentProcess;

    while (getNextInstruction()) {
        instruc = instructions.front();
        instructions.erase(instructions.begin());
        
        if (out_output) {
            cout << instructionCount << ": ==> " << instruc.command << " " << instruc.num << endl;
        }
        
        if (instruc.command == 'c') {
            totalCost += 121;
            contextSwitchCount++;
            instructionCount++;
            for (int i = 0; i < processes.size(); i++) {
                if (processes[i].pid == instruc.num) {   //here num is procid
                    currentProcess = &processes[i];  
                }
            }
            
        } else if (instruc.command == 'e') {
            cout << "EXIT current process" << " " << currentProcess->pid << endl;
            totalCost += 175;
            processExitCount++;
            instructionCount++;
            for (int i = 0; i < processes.size(); i++) {
                if (processes[i].pid == instruc.num) {   
                    currentProcess = &processes[i];  
                    
                }
            }
            
            for (int i = 0; i < numVirtualPages; i++) {

                int cur_vpage = frameTable[currentProcess->pageTable[i].frame].vpage;
                if (currentProcess->pageTable[i].present == 1) {
                    currentProcess->info.unmaps++;
                    totalCost += 400;
                    if (out_output) {
                        cout << " UNMAP " << frameTable[currentProcess->pageTable[i].frame].procid << 
                            ":" << frameTable[currentProcess->pageTable[i].frame].vpage << endl;
                    }
                    
                    
                    VMABits *vma = checkInsideVMA(cur_vpage,currentProcess);

                    if (currentProcess->pageTable[i].modify == 1 && vma->fileMapped == 1) {
                        currentProcess->info.fouts++;
                        totalCost += 2500;
                        if (out_output) {
                            cout << " FOUT" << endl;
                        }
                    }
                    
                   
                    Frame *frame;
                    int frameIndex = currentProcess->pageTable[i].frame;
                    
                    for (int j = 0; j < numFrames; j++) {
                        if (frameIndex == j) {
                            frame = &(frameTable[j]);
                            frame->age = 0;
                            frame->procid = -1;
                            frame->time = 0;
                            frame->vpage = -1;
                        }
                    }
                    

                    //reset PTE
                   
                    currentProcess->pageTable[i].modify = 0;
                    currentProcess->pageTable[i].frame = 0;
                    currentProcess->pageTable[i].pageout = 0;  
                    currentProcess->pageTable[i].present = 0;
                    currentProcess->pageTable[i].refer = 0;
                    currentProcess->pageTable[i].write_protect = 0;
                    delete vma;

                    
                    freeList.push_back(*frame);

                } else {
                    currentProcess->pageTable[i].modify = 0;
                    currentProcess->pageTable[i].frame = 0;
                    currentProcess->pageTable[i].pageout = 0;  
                    currentProcess->pageTable[i].refer = 0;
                    currentProcess->pageTable[i].write_protect = 0;
                }
            }


        } else {   //read or write command
            totalCost += 1;
            instructionCount++;
            int vpage = instruc.num;
            char directive = instruc.command;

            if (currentProcess->pageTable[vpage].present != 1) {   
                //page fault exception
                //check if the vpage can be accessed
                VMABits *bits = checkInsideVMA(vpage, currentProcess);
                if (bits == nullptr) {
                    totalCost += 240;
                    currentProcess->info.segv++;
                    if (out_output) {
                        cout << " SEGV" << endl;
                    }
                    continue;  
                }
               
                int frame = fTable.getFrame(algo);  
                
                
                //if page is file_mapped, indicate so
                if (bits->fileMapped == 1) {   
                    currentProcess->pageTable[vpage].file_map = 1;
                }
                
                
                Process *oldProcess;
                
                //to see if frame table entry at this index is mapped
                for (int i = 0; i < numFrames; i++) {
                    if (frameTable[i].index == frame && frameTable[i].procid != -1 && frameTable[i].vpage != -1) {
                        
                        for (int j = 0; j < numProcesses; j++) {
                            if (processes[j].pid == frameTable[i].procid) {
                                oldProcess = &(processes[j]);
                            }
                        }
                        oldProcess->info.unmaps++;
                        totalCost += 400;
                        if (out_output) {
                            cout << " UNMAP " << frameTable[i].procid << 
                            ":" << frameTable[i].vpage << endl;
                        }
                        if (oldProcess->pageTable[frameTable[i].vpage].modify == 1 
                        && oldProcess->pageTable[frameTable[i].vpage].file_map == 1) {
                            oldProcess->info.fouts++;
                            totalCost += 2500;
                        
                            if (out_output) {
                                cout << " FOUT" << endl;
                            }
                        } else if (oldProcess->pageTable[frameTable[i].vpage].modify == 1 
                        && oldProcess->pageTable[frameTable[i].vpage].file_map == 0) {
                            oldProcess->info.outs++;
                            totalCost += 3000;
                            
                            //should this be currentProcess
                            oldProcess->pageTable[frameTable[i].vpage].pageout = 1;  //check with TA, won't be changing for rest of program
                            
                            if (out_output) {
                                cout << " OUT" << endl;
                            }
                        } 
                        oldProcess->pageTable[frameTable[i].vpage].present = 0;
                        oldProcess->pageTable[frameTable[i].vpage].refer = 0;
                        oldProcess->pageTable[frameTable[i].vpage].modify = 0;  
                    }
                    
                }

                  
                currentProcess->pageTable[vpage].frame = frame;  //map PTE to new frame
                for (int i = 0; i < numFrames; i++) {
                    if (frameTable[i].index == frame) {
                        frameTable[i].vpage = vpage;
                        frameTable[i].procid = currentProcess->pid;
                    }
                }
               
                
                if (currentProcess->pageTable[vpage].file_map == 0 && currentProcess->pageTable[vpage].pageout == 0) {
                    totalCost += 150;
                    currentProcess->info.zeros++;
                    if (out_output) {
                        cout << " ZERO" << endl;
                    }
                } 
                else if (currentProcess->pageTable[vpage].pageout == 1) {
                    totalCost += 3000;
                    currentProcess->info.ins++;
                    if (out_output) {
                        cout << " IN" << endl;
                    }
                } else if (currentProcess->pageTable[vpage].file_map == 1) {
                    totalCost += 2500;
                    currentProcess->info.fins++;
                    if (out_output) {
                        cout << " FIN" << endl;
                    }
                } 
                
                totalCost += 400;    //modify bit is reset when you do out or fout. 
                currentProcess->info.maps++;
                if (out_output) {
                    cout << " MAP " << frame << endl;
                }

                frameTable[frame].time = instructionCount;  //working algo
                frameTable[frame].age = 0; //aging algo

                //This section is modifying the bits in PTE
                currentProcess->pageTable[vpage].refer = 1;  //refer means written or read 
                currentProcess->pageTable[vpage].present = 1;   //present meaning it's mapped to a page frame
               
                if (bits->writeProtect == 1) {
                    currentProcess->pageTable[vpage].write_protect = 1;
                }
                delete bits;
                
                if (directive == 'w' && currentProcess->pageTable[vpage].write_protect == 1) {
                    totalCost += 300;
                    currentProcess->info.segprot++;
                    if (out_output) {
                        cout << " SEGPROT" << endl;
                    }
                }
                if (directive == 'w' && currentProcess->pageTable[vpage].write_protect == 0) {
                    currentProcess->pageTable[vpage].modify = 1;
                }
               
            } else {  //when the vpage is mapped to a frame, set REFERENCED/MODIFIED bits appropriately
                currentProcess->pageTable[vpage].refer = 1;
                if (directive == 'w' && currentProcess->pageTable[vpage].write_protect == 1) {
                    totalCost += 300;
                    currentProcess->info.segprot++;
                    if (out_output) {
                        cout << " SEGPROT" << endl;
                    }
                } else if (directive == 'w' && currentProcess->pageTable[vpage].write_protect == 0) {
                    currentProcess->pageTable[vpage].modify = 1;
                }
                
                
            }
            
        }  //end of read/write section
        
        if (out_currentPT) {
            cout << "PT[" << currentProcess->pid << "]: ";
            for (int j = 0; j < numVirtualPages; j++) {
                if (currentProcess->pageTable[j].present == 1) {
                    cout << j << ":";
                    if (currentProcess->pageTable[j].refer ==1) {
                        cout << "R"; 
                    
                    } else {
                        cout << "-";
                    } if (currentProcess->pageTable[j].modify ==1) {
                        cout << "M"; 
                    
                    } else {
                        cout << "-";
                    } if (currentProcess->pageTable[j].pageout ==1) {
                        cout << "S ";
                    } else {
                        cout << "- ";
                    }
                } else {
                    if (currentProcess->pageTable[j].pageout ==1) {
                        cout << "# ";
                    } else {
                        cout << "* ";
                    }
                }
            }
            cout << endl;
        }
        //print out page tables for all processes (y option)
        if (out_allprocesses) {
            for (int i = 0; i < numProcesses; i++) {
                cout << "PT[" << processes[i].pid << "]: ";
                for (int j = 0; j < numVirtualPages; j++) {
                    if (processes[i].pageTable[j].present == 1) {
                        cout << j << ":";
                        if (processes[i].pageTable[j].refer ==1) {
                            cout << "R"; 
                        
                        } else {
                            cout << "-";
                        } if (processes[i].pageTable[j].modify ==1) {
                            cout << "M"; 
                        
                        } else {
                            cout << "-";
                        } if (processes[i].pageTable[j].pageout ==1) {
                            cout << "S ";
                        } else {
                            cout << "- ";
                        }
                    } else {
                        if (processes[i].pageTable[j].pageout ==1) {
                            cout << "# ";
                        } else {
                            cout << "* ";
                        }
                    }
                }
                cout << endl;
            }
        }


            
            
            
        //print frametable after each instruction 
        if (out_ft_eachinstruc) {
            cout << "FT: ";
            for (auto it = frameTable.begin(); it != frameTable.end(); ++it) {
                if (it->procid == -1 && it->vpage == -1) {
                    cout << "* ";
                } else {
                    cout << it->procid << ":" << it->vpage << " ";
                }
                
            }
            cout << endl;
        }

    }//end of while

    if (out_pagetable) {
        
        for (int i = 0; i < numProcesses; i++) {
            cout << "PT[" << processes[i].pid << "]: ";
            for (int j = 0; j < numVirtualPages; j++) {
                if (processes[i].pageTable[j].present == 1) {
                    cout << j << ":";
                    if (processes[i].pageTable[j].refer ==1) {
                        cout << "R"; 
                    
                    } else {
                        cout << "-";
                    } if (processes[i].pageTable[j].modify ==1) {
                        cout << "M"; 
                    
                    } else {
                        cout << "-";
                    } if (processes[i].pageTable[j].pageout ==1) {
                        cout << "S ";
                    } else {
                        cout << "- ";
                    }
                } else {
                    if (processes[i].pageTable[j].pageout ==1) {
                        cout << "# ";
                    } else {
                        cout << "* ";
                    }
                }
            }
            cout << endl;
        }
        
    }

    if (out_frametable) {
        cout << "FT: ";
        for (auto it = frameTable.begin(); it != frameTable.end(); ++it) {
            if (it->procid == -1 && it->vpage == -1) {
                cout << "* ";
            } else {
                cout << it->procid << ":" << it->vpage << " ";
            }
            
        }
        cout << endl;
    }


    if (out_summary) {
        for (int i = 0; i < numProcesses; i++) {
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                processes[i].pid, processes[i].info.unmaps, processes[i].info.maps, processes[i].info.ins,
                processes[i].info.outs, processes[i].info.fins, processes[i].info.fouts, processes[i].info.zeros,
                processes[i].info.segv, processes[i].info.segprot);
            
        }
        printf("TOTALCOST %lu %lu %lu %llu\n", instructionCount, contextSwitchCount, processExitCount, totalCost);
    }

}


int main(int argc, char* argv[]) {
    int option;
    Pager *algo;
    FrameTable *ftable;
    string choix;
    string str;
    
    while ((option = getopt(argc,argv,"a:o:f:")) != -1) {
        switch (option) {
            case 'a':
                {
                    choix = optarg; 
                    if (choix == "f") {
                        algo = new FIFO();
                    }
                    else if (choix == "r") {
                        algo = new Random();
                    }
                    else if (choix == "c") 
                        algo = new Clock();
                    else if (choix == "e") 
                        algo = new NRU();
                    else if (choix == "a")
                        algo = new Aging();
                    else
                        algo = new WorkSet();
                }
                break;
            case 'o':
                {
                    str = optarg;
                    int i = 0;
                    while (i < str.size()) {
                        if (str[i] == 'O') {
                            out_output= true;
                            i++;
                        } else if (str[i] == 'P') {
                            out_pagetable = true;
                            i++;
                        } else if (str[i] == 'F') {
                            out_frametable = true;
                            i++;
                        } else if (str[i] == 'S') {
                            out_summary = true;
                            i++;
                        } else if (str[i] == 'f') {
                            out_ft_eachinstruc = true;
                            i++;
                        } else if (str[i] == 'x') {
                            out_currentPT = true;
                            i++;
                        } else if (str[i] == 'y') {
                            out_allprocesses = true;
                            i++;
                        } else {
                            cout << "Incorrect options---select only O,P,F,S" << endl;
                            return 1;
                        }
                    }
                }
                break;
            case 'f':
                {
                    int temp = atoi(optarg);
                    ftable = new FrameTable(temp);
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
    char* randinput = argv[optind + 1];  
    readRandomFile(randinput);
    readInputFile(input);
    
    
    ftable->initializeFreeList();  

    //initialize map inside frame table
    ftable->initializeTable();

    simulate(ftable,algo);

    delete algo, ftable;
    return 0;
}

             
     