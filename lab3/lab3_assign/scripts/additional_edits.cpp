#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <unistd.h>
#include <map>
#include <algorithm>
using namespace std;

typedef unsigned long long ull;
typedef unsigned long ul;
const int numVirtualPages = 64;
bool out_output = false;
bool out_pagetable = false;
bool out_frametable = false;
bool out_summary = false;
vector<int> randomNumbers;
int numProcesses = 0;
int numFrames = 0;
int offset = -1;
ul instructionCount = 0;

struct Frame {
    int index = -1;  //refers to frame from PTE
    unsigned int age = 0;  
    int time = -1;
    Frame() : age(0), index(0), time(-1) {};
    Frame(int a, int c, int t) : age(a), index(c), time(t) {}
};

//how can i keep globals together in c++
vector<Frame> freeList;//don't read too much into the name. it is a list of fixed frame indices--- ask Nisarg?
map<int, pair<int,int>> frameMap;  //maps from frame index to a entry containing virtual page and process id
vector<Frame> insertionOrder;  //lists order of insertion of frames by index for FIFO algorithm (This is a set of "Active" frames)

struct Instruction {
    char command;
    int num;
    Instruction() : command(' '), num(-1) {}
    Instruction(char c, int i) : command(c), num(i) {}
    
};



class Pager {
    
public:
    virtual Frame getVictimFrame() = 0;
    virtual void setFrame(int time, int age) = 0;
    virtual void removeFrame() = 0;
};



//I might not need this struct any more
//I try to initialize/save into the freeList and frameMap within the struct but it doesn't seem to save..is it possible
struct FrameTable {
    
    int maxSize;   // this is user input num of frames
    FrameTable() : maxSize(-1) {}
    FrameTable(int size) : maxSize(size) {
        numFrames = size;
    } //do I need to initialize the vectors?

    void initializeFreeList() {
        for (int i = 0; i < maxSize; i++) {
            Frame frame(0,i,-1);
            freeList.push_back(frame);
        }
    }

    void initializeMap() {
        for (int i = 0; i < maxSize; i++) {
            //frameTable.frameMap[frame.index] = std::make_pair(currentProcess.pid, vpage);
            frameMap[i] = std::make_pair(-1,-1);
        }
    }

    Frame getFrame(Pager *alg) {   //I altered this to return a pointer so I can modify frame info inside Simulation method
        Frame frame;
        if (freeList.empty()) {
            frame = alg->getVictimFrame();
            return frame;
        }
        frame = freeList.front();
        freeList.erase(freeList.begin());
        return frame;
    }

} frameTable;

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
    //Constructor with initializer list for ProcessInfo
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
    if (offset == randomNumbers.size()-1) { //altered this line
        offset = 0;
    }
    return (randomNumbers[offset] % size);
}


class FIFO: public Pager {
private: 
    int removeLoc = 0;
    int frameIndex = -1;
public:
    virtual Frame getVictimFrame() override;
    virtual void setFrame(int time, int age) override;
    virtual void removeFrame() override;

};
Frame FIFO::getVictimFrame()  { 
    Frame frame = insertionOrder.front();
    frameIndex = frame.index;
    //insertionOrder.erase(insertionOrder.begin());
    return frame;
    
}
void FIFO::setFrame(int time, int age) {
    Frame frame(frameIndex, time, age);
    insertionOrder.push_back(frame);
}
void FIFO::removeFrame() {
    insertionOrder.erase(insertionOrder.begin());
}

class Random: public Pager {
private:
    int removeLoc = -1;
    int frameIndex = -1;
public:
    virtual Frame getVictimFrame() override;
    virtual void setFrame(int time, int age) override;
    virtual void removeFrame() override;
};

Frame Random::getVictimFrame()  { 
    frameIndex = myRandom(numFrames);
    for (int i = 0; i < insertionOrder.size(); i++) {
        if (frameIndex == insertionOrder[i].index) {
            Frame frame = insertionOrder[i];
            removeLoc = i;
            //insertionOrder.erase(insertionOrder.begin() + i);
            return frame;
        }
    }
    
}
void Random::setFrame(int time, int age) {
     Frame frm(frameIndex,time,age);
    insertionOrder.push_back(frm);
}
void Random::removeFrame() {
    insertionOrder.erase(insertionOrder.begin() + removeLoc);
}
struct VMA {
    int startVirtualPage;
    int endVirtualPage;
    int writeProtect;   //should these be unsigned int
    int fileMapped;
    VMA() : startVirtualPage(-1), endVirtualPage(-1), writeProtect(-1), fileMapped(-1) {}
    VMA(int start, int end, int write, int fileMap) {
        startVirtualPage = start;
        endVirtualPage = end;
        writeProtect = write;  //set this first to false?
        fileMapped = fileMap;  //set these first to false? 
    }

};
//how is this different from initializing constructor using this -> 
struct Process {
    int pid;
    int numVMA;
    vector<VMA> vmaList;
    PerProcessInfo info;
    //PTE pageTable[numVirtualPages];
    vector<PTE> pageTable = vector<PTE>(numVirtualPages);   //how to search this for the page number
   
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
    virtual Frame getVictimFrame() override;
    virtual void setFrame(int time, int age) override;
    virtual void removeFrame() override;
};
Frame WorkSet::getVictimFrame()  { 
    Frame *frame;
    PTE *pte;
    int frameIndex;
    for (int i = 0; insertionOrder.size(); i++) {
        frame = &(insertionOrder[hand]);
        frameIndex = frame->index;
        int procID = frameMap[frameIndex].first;
        int page = frameMap[frameIndex].second;
        for (int j = 0; j < processes.size(); j++) {
            if (processes[j].pid == procID) {
                pte = &(processes[i].pageTable[page]);
            }
        }
        
        if (pte->refer == 1) {
            pte->refer = 0;
            frame->time = (unsigned long)instructionCount;
        } else {
            if ((unsigned long)instructionCount - frame->time >= tau) {   //casting of ul?????
                return *frame;
            }
        }
        hand++;
        if (hand == insertionOrder.size()) {
            hand = 0;
        }
    }
}
void WorkSet::setFrame(int time, int age) {

}
void WorkSet::removeFrame() {
    
}


class Aging: public Pager {
private:
    int hand = 0;
    int frameIndex=-1;
    int removeLoc=-1;
public:
    virtual Frame getVictimFrame() override;
    virtual void setFrame(int time, int age) override;
    virtual void removeFrame() override;
};

Frame Aging::getVictimFrame()  {
    Frame frame; //changed to a pointer ??
    
    Process proc;  //should i make this a pointer

    for (int i = 0; i < insertionOrder.size(); i++) {
        frame = insertionOrder[hand];  //examine the first frame
        frameIndex = frame.index;
        int procID = frameMap[frameIndex].first;
        int vpage = frameMap[frameIndex].second;
        for (int i = 0; i < processes.size(); i++) {
            if (procID == processes[i].pid) {
                proc = processes[i];
            }
        }
        //move the bits one over
        frame.age = (frame.age >> 1);
        if (proc.pageTable[vpage].refer == 1) {
            frame.age = (0x80000000 | frame.age);  //set first bit
        }
        hand++;
        if (hand == insertionOrder.size()) {
            hand = 0;
        }
    }

    //get the minimum value of all bit strings and return the frame
    Frame fr = insertionOrder[0];
    int index = 0;
    int min = fr.age;
    for (int i = 1; i < insertionOrder.size(); i++) {
        if (insertionOrder[i].age < min) {
            min = insertionOrder[i].age;
            index = i;
        }
    }
    frame = insertionOrder[index];
    frameIndex = frame.index;
    removeLoc = index;
    hand = index + 1;
    // insertionOrder.erase(insertionOrder.begin() + index);
    return insertionOrder[index];
}

void Aging::setFrame(int time, int age) {
     Frame frm(frameIndex,time,age);
    if (hand == 0) {
        insertionOrder.push_back(frm);
    } else {
        insertionOrder.insert(insertionOrder.begin() + removeLoc, frm);
    }
}
void Aging::removeFrame() {
    insertionOrder.erase(insertionOrder.begin() + removeLoc);
}
class Clock: public Pager {
private: 
    int hand = 0; 
    int frameIndex = -1;  // frame index is saved here
    int removePos = -1; // (different from frame index) index inside insertionOrder where the newly mapped frame must be placed
public:
    virtual Frame getVictimFrame() override;
    virtual void setFrame(int time, int age) override;
    virtual void removeFrame() override;
};
Frame Clock::getVictimFrame()  { 
    Frame frame;
    PTE *pte;  //create pointer b/c modifying its values and it needs to go outside of scope
    frame = insertionOrder[hand];  //examine the first frame
    frameIndex = frame.index;
    int procID = frameMap[frameIndex].first;
    int pageNum = frameMap[frameIndex].second;
    for (int i = 0; i < numProcesses; i++) {
        if (processes[i].pid == procID) {
            pte = &(processes[i].pageTable[pageNum]);
        }
    }
    if (pte->refer == 0) {
        //insertionOrder.erase(insertionOrder.begin() + hand);
        removePos = hand;
        hand++;            
        if (hand == insertionOrder.size()) {
            hand = 0;   //save state of hand for next time the clock algo is called 
        }
        return frame;
    }
   
    while (pte->refer == 1) {
        pte->refer = 0;
        hand++;            
        if (hand == insertionOrder.size()) {
            hand = 0;
        }
        frame = insertionOrder[hand];
        frameIndex = frame.index;
        procID = frameMap[frameIndex].first;
        pageNum = frameMap[frameIndex].second;
        for (int i = 0; i < numProcesses; i++) {
            if (processes[i].pid == procID) {
                pte = &(processes[i].pageTable[pageNum]);
            }
        }
        
    }
    
    removePos = hand;
    //insertionOrder.erase(insertionOrder.begin() + hand);
    hand++;           

    if (hand == insertionOrder.size()) {
        hand = 0;   //save state of hand for next time the clock algo is called 
    }
    return frame;
}

void Clock::setFrame(int time, int age) {
    Frame frm(frameIndex,time,age);
    if (hand == 0) {
        insertionOrder.push_back(frm);
    } else {
        insertionOrder.insert(insertionOrder.begin() + removePos, frm);
    }
    
}
void Clock::removeFrame() {
     insertionOrder.erase(insertionOrder.begin() + removePos);
}
class NRU: public Pager {
private:
    int hand = 0;
    map<int, pair<int,int>> classToFrameIndex;  //maps class index to pair (frameindex, hand)
    int removeLoc = -1;
    int frameIndex= -1 ;
    ul prevInstruc = 0;  
      // class 0= R0 M0, class 1= R0 M1, class 2= R1 M0, class 3= R1 M1 
public:
    virtual Frame getVictimFrame() override;
    virtual void setFrame(int time, int age) override;
    virtual void removeFrame() override;
};

Frame NRU::getVictimFrame()  { 
    Frame frame;
    PTE *pte;
    
    int classIndex=-1;
    
    for (int i =0; i < insertionOrder.size(); i++) {    //frame map size should always have a 1 to 1 corresponendece with insertion order size???
        frame = insertionOrder[hand];
        frameIndex = frame.index;
        int procID = frameMap[frameIndex].first;
        int pageNum = frameMap[frameIndex].second;
        for (int i = 0; i < processes.size(); i++) {
            if (processes[i].pid == procID) {
                pte = &(processes[i].pageTable[pageNum]);
            }
        }
        classIndex = 2*pte->refer + pte->modify;
        
        if (classIndex == 0) {
            removeLoc = hand;
            hand++;
            if (hand == insertionOrder.size()) {
                hand = 0;
            }
            return frame;
        } else if (classIndex == 1) {
            if (classToFrameIndex.count(classIndex)) {  //go through the rest of the frames until class 1, 2,3 each have something 
                hand++;
                if (hand == insertionOrder.size()) {
                    hand = 0;
                }
                continue;
            }
            classToFrameIndex[classIndex] = make_pair(frameIndex,hand) ;
            hand++;
            if (hand == insertionOrder.size()) {
                hand = 0;
            }
            
        } else if (classIndex == 2) {
            if (classToFrameIndex.count(classIndex)) {  //go through the rest of the frames until class 1, 2,3 each have something 
                hand++;
                if (hand == insertionOrder.size()) {
                    hand = 0;
                }
                continue;
            }


            classToFrameIndex[classIndex] = make_pair(frameIndex,hand) ;
            hand++;
            if (hand == insertionOrder.size()) {
                hand = 0;
            }
        } else if (classIndex == 3) {
            if (classToFrameIndex.count(classIndex)) {  //go through the rest of the frames until class 1, 2,3 each have something 
                hand++;
                if (hand == insertionOrder.size()) {
                    hand = 0;
                }
                continue;
            }
            classToFrameIndex[classIndex] = make_pair(frameIndex,hand) ;
            hand++;
            if (hand == insertionOrder.size()) {
                hand = 0;
            }
        }

        if ((int)instructionCount - (int)prevInstruc >= 50) {
            for (int i = 0; i < insertionOrder.size(); i++) {
                int index = insertionOrder[i].index;
                int proc = frameMap[index].first;
                int vpage = frameMap[index].second;
                for (int i = 0; i < numProcesses; i++) {
                    if (processes[i].pid == proc) {
                        pte = &(processes[i].pageTable[vpage]);
                    }
                }
                if (pte->present == 1) {
                    pte->refer = 0;
                    prevInstruc = instructionCount;
                }
            }
        } 

        //cycle through the map classToFrameIndex to find the first class that has a value
        for (int i = 1; i <= 3; i++) {
            if (classToFrameIndex.count(i)) {
                removeLoc =  classToFrameIndex.find(i)->second.second;
                hand = classToFrameIndex.find(i)->second.second + 1; //set hand to one position after victim frame
                frame = insertionOrder[classToFrameIndex.find(i)->second.first];
                frameIndex = frame.index;
                //insertionOrder.erase(insertionOrder.begin() + classToFrameIndex.find(i)->second);
                return frame;
            }
        }
    }
    
}
void NRU::setFrame(int time, int age) {
    Frame frm(frameIndex, time, age);
    if (hand == 0) {
        insertionOrder.push_back(frm);
    } else {
        insertionOrder.insert(insertionOrder.begin() + removeLoc, frm);
    }
}
void NRU::removeFrame() {
    insertionOrder.erase(insertionOrder.begin() + removeLoc);
}
struct VMABits {   //return this data type after checkInsideVMA
    bool insideVMA;
    int writeProtect;
    int fileMapped;
    VMABits() : insideVMA(false), writeProtect(-1), fileMapped(-1) {}
    VMABits(bool in, int write, int map) : insideVMA(in), writeProtect(write), fileMapped(map) {}
}; 

VMABits *checkInsideVMA(int vpage, Process *proc) {   //do i need to delete proc inside this method at some point
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
            startVirtualPage = stoi(token);  //does this convert to bool?
            
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
                    currentProcess = &processes[i];  //do we still need struct ProcessList? b/c we have global
                }
            }
            
        } else if (instruc.command == 'e') {
            int procID;
            totalCost += 175;
            processExitCount++;
            instructionCount++;
            for (int i = 0; i < processes.size(); i++) {
                if (processes[i].pid == instruc.num) {   //here num is procid
                    currentProcess = &processes[i];  //do we still need struct ProcessList? b/c we have global vector containing all the processes
                    
                }
            }

            for (int i = 0; i < numVirtualPages; i++) {
                if (currentProcess->pageTable[i].present == 1) {
                    currentProcess->info.unmaps++;
                    totalCost += 400;
                    if (out_output) {
                        cout << " UNMAP" << endl;
                    }
                    //loop over processes vector and for each element that belongs to currentProcess, check if it is inside the VMA, then extract the bits
                    
                    VMABits *vma = checkInsideVMA(numVirtualPages,currentProcess);

                    if (currentProcess->pageTable[i].modify == 1 && vma->fileMapped == 1) {
                        currentProcess->info.fouts++;
                        totalCost += 2500;
                        if (out_output) {
                            cout << " FOUT" << endl;
                        }
                    }
                    
                   
                    Frame frame;
                    int frameIndex = currentProcess->pageTable[i].frame;
                    frame = insertionOrder[frameIndex];
                    
                     //Reset frame table
                    frameMap[frameIndex] = std::make_pair(-1,-1);

                    //reset PTE
                    currentProcess->pageTable[i].modify=0;
                    currentProcess->pageTable[i].frame=0;
                    currentProcess->pageTable[i].pageout=0;
                    currentProcess->pageTable[i].present=0;
                    currentProcess->pageTable[i].refer=0;
                    currentProcess->pageTable[i].write_protect=0;
                    delete vma;

                    
                    freeList.push_back(frame);
                }

            }


        } else {   //read or write command
            totalCost += 1;
            instructionCount++;
            int vpage = instruc.num;
            char directive = instruc.command;
            //PTE pte = currentProcess->pageTable[vpage];  //num here is a vpage and pte represents previous entry

            if (currentProcess->pageTable[vpage].present != 1) {   //or just (!pte.present)?
                //page fault exception
                //check if the vpage can be accessed
                VMABits *bits = checkInsideVMA(vpage, currentProcess);
                if (bits == nullptr) {
                    totalCost += 240;
                    currentProcess->info.segv++;
                    if (out_output) {
                        cout << " SEGV" << endl;
                    }
                    continue;  //continue to next instruction
                }
               
                Frame frame = frameTable.getFrame(algorithm);  //make a pointer here? 

                //insertionOrder.push_back(frame);
                
                //if page is file_mapped, indicate so
                if (bits->fileMapped == 1) {   //But how do we know if this vma is file mapped
                    currentProcess->pageTable[vpage].file_map = 1;
                }
                
                //to see if frame table entry at this index is mapped
                if (frameMap.count(frame.index) && frameMap[frame.index].first != -1 && frameMap[frame.index].second != -1) {
                    Process *oldProcess;
                    for (int i = 0; i < processes.size(); i++) {
                        if (processes[i].pid == frameMap[frame.index].first) {
                            oldProcess = &(processes[i]);
                        }
                    }

                     
                    oldProcess->info.unmaps++;
                    totalCost += 400;
                    if (out_output) {
                        cout << " UNMAP " << frameMap[frame.index].first << 
                        ":" << frameMap[frame.index].second << endl;
                    }
                   //update insertionOrder -- erase the frame    not sure if this is the right place for it
                    algorithm->removeFrame();



                    if (oldProcess->pageTable[frameMap[frame.index].second].modify == 1 
                    && oldProcess->pageTable[frameMap[frame.index].second].file_map == 1) {
                        oldProcess->info.fouts++;
                        totalCost += 2500;
                        
                        if (out_output) {
                            cout << " FOUT" << endl;
                        }
                    } else if (oldProcess->pageTable[frameMap[frame.index].second].modify == 1 
                    && oldProcess->pageTable[frameMap[frame.index].second].file_map == 0) {
                        oldProcess->info.outs++;
                        totalCost += 3000;
                        oldProcess->pageTable[frameMap[frame.index].second].pageout = 1;  //check with TA, won't be changing for rest of program
                        if (out_output) {
                            cout << " OUT" << endl;
                        }
                    } 
                    oldProcess->pageTable[frameMap[frame.index].second].present = 0;
                    oldProcess->pageTable[frameMap[frame.index].second].refer = 0;
                    oldProcess->pageTable[frameMap[frame.index].second].modify = 0;  
                   
                    //if first time page is accessed, we zero     //zero is only for memory mapped pages so check paged out and valid bit. if the page is modified, then do out and fout
                    
                }
                
                //zero is to reduce the data movement that happens from memory to hard disk. zer out when page is not file mapepd and not paged out. if it's valid and file mapped what happens is FIN, then if not Filemapped then chcedk pagedout bit. if pagedoutbit is set, give FIN, .if not pagedout and notfilemapped then ZERO. 


                currentProcess->pageTable[vpage].frame = frame.index;  //map PTE to new frame
                frameMap[frame.index] = std::make_pair(currentProcess->pid, vpage);  //add entry into frame table
                
                if (currentProcess->pageTable[vpage].file_map == 0 && currentProcess->pageTable[vpage].pageout == 0) {
                    totalCost += 150;
                    currentProcess->info.zeros++;
                    //currentProcess->pageTable[vpage].not_first_access = 1;
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
                frame.age = 0;  //for aging algorithm
                frame.time = instructionCount;  //for working set algo

                 //index,age,time    Update insertionorder here:
                algorithm->setFrame(frame.age, frame.time);
                if (out_output) {
                    cout << " MAP " << frame.index << endl;
                }
                


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
        for (auto it = frameMap.begin(); it != frameMap.end(); ++it) {
            if (it->second.first == -1 && it->second.second == -1) {
                cout << "* ";
            } else {
                cout << it->second.first << ":" << it->second.second << " ";
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
                    if (choix == "f")
                        algo = new FIFO();
                    else if (choix == "r")
                        algo = new Random();
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
    char* input = argv[optind];   //argv[optind]
    char* randinput = argv[optind + 1];  //argv[optind + 1]
    readRandomFile(randinput);
    readInputFile(input);
    
    
    ftable->initializeFreeList();  

    //initialize map inside frame table
    ftable->initializeMap();

    simulate(ftable,algo);

    delete algo, ftable;
    return 0;
}
/* was in main method 
    for (int i = 0; i < randomNumbers.size(); i++) {
        cout << randomNumbers[i] << endl;
    }

    for (int i = 0; i < instructions.size(); i++) {
        cout << "Instruction: " << instructions[i].command << endl;
        cout << "Page or Process Num: " << instructions[i].num << endl;
    }

    for (int i = 0; i < processes.size(); i++) {
        cout << "Process ID: " << processes[i].pid << endl 
        <<"Number of VMAs: " << processes[i].numVMA <<endl<< "Starting Virtual Page: " << processes[i].vma.startVirtualPage
        << endl << "Last Virtual Page: " << processes[i].vma.endVirtualPage << endl << "Write Protect: " << processes[i].vma.writeProtect << endl <<
        "File Mapped: " << processes[i].vma.fileMapped << endl;
    }
    */

/*
for (int i = 0; i < processes.size(); i++) {
        cout << "Process ID: " << processes[i].pid << endl 
        <<"Number of VMAs: " << processes[i].numVMA <<endl<< "Starting Virtual Page: " << processes[i].vma.startVirtualPage
        << endl << "Last Virtual Page: " << processes[i].vma.endVirtualPage << endl << "Write Protect: " << processes[i].vma.writeProtect << endl <<
        "File Mapped: " << processes[i].vma.fileMapped << endl << "PageTable: " 
        << processes[i].pageTable[0].present << endl;
    }
*/

/*

void readInputFile(char* fileName) {
    file.open(fileName);
    string store;
    int numProcesses = -1;
    int pid = 0;
    bool numProcessObtained = false,flag=false;
    int numVMA = -1;
    int startVirtualPage = -1, endVirtualPage = -1;
    bool writeProtect = false, fileMapped = false;

      
    while (getline(file,store)) {
        if (flag && store[0] == '#') {
            pid=pid+1;
           
            flag = false;
        }
        if (store[0] == '#' || isalpha(store[0])) {
            continue;
        } if (!numProcessObtained && store.length() == 1 && store[0] != '#') {
            numProcesses = stoi(store);
            numProcessObtained = true;
            continue;
        } if (numProcessObtained && store.length() == 1) {
            numVMA = store[0] - '0'; //get num of VMAs   can this be greater than 9? otherwise, change!
            continue;
        } if (numProcessObtained && store.length() > 1) {
            stringstream stream(store);
            string token;
            Process process(pid,numVMA,startVirtualPage,endVirtualPage,writeProtect,fileMapped);
            process.setPID(pid);
            stream >> token;  // get specs for each VMA
            startVirtualPage = stoi(token);
            process.setStart(startVirtualPage);
            stream >> token;
            endVirtualPage = stoi(token);
            process.setEnd(endVirtualPage);
            stream >> token;
            if (token == "1")
               process.setProtect(true);
            else 
                process.setProtect(false);
            stream >> token;
            if (token == "1")
                process.setMapped(true);
            else 
               process.setMapped(false);
            
            processes.push_back(process);
            flag = true;
        }
       
    }

    file.close();
}

struct Process {
    int pid;
    PTE pageTable[numVirtualPages];
    map<pid, VMA> areas;
    int numberVMA,startVirtualPage, endVirtualPage;
    bool writeProtect, fileMapped; 
    void setPID(int procid) {
        pid = procid;
    }
    void setVMA(int vma) {
        numberVMA = vma;
    }
    void setStart(int start) {
        startVirtualPage = start;
    }
    void setEnd(int end) {
        endVirtualPage = end;
    }
    void setProtect(bool protect) {
        writeProtect = protect;
    }
    void setMapped(bool map) {
        fileMapped = map;
    }
    Process() {
        pid = -1; 
        numberVMA = -1;
        startVirtualPage = -1;
        endVirtualPage = -1;
        writeProtect = false;
        fileMapped = false;
    }
    Process(int pid, int numVMA, int startVirtualPage, int endVirtualPage, bool writeProtect, bool fileMapped) {
        pid = pid;
        numberVMA = numVMA;
        startVirtualPage = startVirtualPage;
        endVirtualPage = endVirtualPage;
        writeProtect = writeProtect;
        fileMapped = fileMapped;
    }
   
    
};

  if (out_pagetable) {
        int numPages = 0;
        char ref = ' ';
        char mod = ' ';
        char out = ' ';
        for (int i = 0; i < numProcesses; i++) {
            cout << "PT[" << processes[i].pid << "]: ";
            while (numPages < numVirtualPages) {

                if (processes[i].pageTable[numPages].present == 0 && processes[i].pageTable[numPages].pageout == 1) {
                    cout << '#';
                }
                
                else if (processes[i].pageTable[numPages].present == 0 && processes[i].pageTable[numPages].pageout == 0) {
                    cout << '*';
                }

                if (processes[i].pageTable[numPages].refer == 1) {
                    ref = 'R';
                }

                if (processes[i].pageTable[numPages].refer == 0) {
                    ref = '-';
                }


                if (processes[i].pageTable[numPages].modify == 1) {
                    mod = 'M';
                }

                if (processes[i].pageTable[numPages].modify == 0) {
                    ref = '-';
                }
                if (processes[i].pageTable[numPages].pageout == 1) {
                    out = 'S';
                }

                if (processes[i].pageTable[numPages].pageout == 0) {
                    ref = '-';
                }

                cout << numPages << ":" << ref << mod << out;
                numPages = numPages + 1;
            }
             
        }
    }
*/


/*
if (out_pagetable) {
        int numPages = 0;
        //string output;  concatenate ?
        for (int i = 0; i < numProcesses; i++) {
            cout << "PT[" << processes[i].pid << "]: ";
            while (numPages < numVirtualPages) {
                if (processes[i].pageTable[numPages].present == 0 && processes[i].pageTable[numPages].pageout == 1) {
                    cout << "# ";
                    numPages = numPages + 1;
                    continue;
                }
                if (processes[i].pageTable[numPages].present == 0 && processes[i].pageTable[numPages].pageout == 0) {
                    cout << "* ";
                    numPages = numPages + 1;
                    continue;
                }
                cout << numPages << ":";
                if (processes[i].pageTable[numPages].present == 1 && processes[i].pageTable[numPages].refer == 1) {
                    cout << "R";
                } else {
                    cout << "-";
                }
                if (processes[i].pageTable[numPages].present == 1 && processes[i].pageTable[numPages].modify == 1) {
                    cout << "M";
                } else {
                    cout << "-";
                }
                if (processes[i].pageTable[numPages].present == 1 && processes[i].pageTable[numPages].pageout == 1) {
                    cout << "S ";
                } else {
                    cout << "- ";
                }
                     
                numPages = numPages + 1;
            }
             
        }
        cout << endl;
    }*/

/*
    Frame Clock::getVictimFrame()  { 
    Frame frame;
    int frameIndex;
    PTE *pte;  //create pointer b/c modifying its values and it needs to go outside of scope
    frame = insertionOrder[hand];  //examine the first frame
    frameIndex = frame.index;
    int procID = frameMap[frameIndex].first;
    int pageNum = frameMap[frameIndex].second;
    for (int i = 0; i < numProcesses; i++) {
        if (processes[i].pid == procID) {
            pte = &(processes[i].pageTable[pageNum]);
        }
    }
    if (pte->refer == 0) {
        hand++;
        if (hand == insertionOrder.size()) 
            hand = 0;   //save state of hand for next time the clock algo is called 
        }
        return frame;
    }
    while (pte->refer == 1) {
        pte->refer = 0;
        hand++;
        if (hand == insertionOrder.size()) {
            hand = 0;
        }
        frame = insertionOrder[hand];
        frameIndex = frame.index;
        procID = frameMap[frameIndex].first;
        pageNum = frameMap[frameIndex].second;
        for (int i = 0; i < numProcesses; i++) {
            if (processes[i].pid == procID) {
                pte = &(processes[i].pageTable[pageNum]);
            }
        }
    }
    hand++;
    if (hand == insertionOrder.size()) {
        hand = 0;   //save state of hand for next time the clock algo is called 
    }
    return frame;
}*/