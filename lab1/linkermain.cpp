#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <cstdlib>
#include <map>
#include <utility>   
#include <iterator>
#include <cstdio>
#include <iomanip>
using namespace std;

struct TokOffLine {
    string token;
    int lineNumber;
    int lineOffset;
};
struct defToken {
    string symbol;
    int relAdd;
    int modNum;
};
struct defTokenAndAdd {
    string sym;
    int address;
};
struct offsetandlinenum {  //for identifying last eof
    int offset;
    int linenum;
};

//Function Prototypes & Variable Declarations
vector<TokOffLine> tokenParser(const char *s);
string readSymbol(string s);
void parseError(int errcode, int index);
int readInt(string s);
char readIAER(string s) ;
int checkInstr(string s);
int passOne(const char *input);
void passTwo(const char *input);
map<string,int> symbolTable; //maps symbol->absolute address 
offsetandlinenum getOffAndLine(const char *s);


void passTwo(const char *input) {
    int j = 0;
    bool flag = false; //for initiating printing of "Memory Map"
    bool set = false; //if the file is blank this will still print a Memory Map
    int moduleNum = 1;
    int baseAddressMod = 0; //base address of each module
    int absAdd = 0; //absolute address keys for memory map
    int memSize = 0;  //should not exceed addressable memory of 512 words
    vector<string> useTokens;   //  collects the usetokens.  cleared after each module 
    vector<TokOffLine> tokBuffer;  //holds tokens from the parser
    vector<bool> checkSymUse;  //rule 7  cleared after each module
    vector<string> symdefnotused;  //rule 4 holds tokens from all modules 
    map<string, int> symbolandmod; //for rule 4 -- to collect all the definition symbols and mod nums
    map<int, int> baseAddMod;  //maps module # -> absolute address
    map<int,string> memMap; //memory map

    tokBuffer = tokenParser(input);  // why not pass in &input? b/c then it'd be pointer to point to char
    if (tokBuffer.size() == 0) {
        set = true;
    }
    while (j < tokBuffer.size()) {
        baseAddMod.insert(pair<int, int> (moduleNum, baseAddressMod));  
        int defCount = readInt(tokBuffer[j].token);
        j++;
        for (int i = 0; i < defCount; i++) {
            string sym = readSymbol(tokBuffer[j].token);
            if (!symbolandmod.count(sym)) {
                symbolandmod.insert(pair<string,int> (sym,moduleNum));
            }
            j++;
            int temp = readInt(tokBuffer[j].token);
            j++;
        }
        int useCount = readInt(tokBuffer[j].token);
        
        j++;
        for (int i = 0; i < useCount; i++) {
            string sym = readSymbol(tokBuffer[j].token);
            symdefnotused.push_back(sym);
            useTokens.push_back(sym);
            checkSymUse.push_back(false);
            j++;
        }
        
        int codeCount = readInt(tokBuffer[j].token);
        memSize += codeCount;
        
        j++;
        for (int i = 0; i < codeCount;i++) {
            char type = readIAER(tokBuffer[j].token);  // take in a IEAR
            j++;
           
            int res = checkInstr(tokBuffer[j].token);
            int opcode = res / 1000;
            int operand = res % 1000;
            
            if (!flag) {
                cout << "Memory Map" << endl;
                flag = true; 
            }
            switch (type) {
                case 'I': 
                   if (res > 9999) {
                        cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << "9999 Error: Illegal immediate value; treated as 9999"<<endl;
                        memMap.insert(pair<int, string> (absAdd, "9999 Error: Illegal immediate value; treated as 9999"));
                        absAdd++;
                    } else {
                        cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                            << setw(4) << to_string(opcode*1000+operand)<<endl;
                        memMap.insert(pair<int, string> (absAdd, to_string(opcode*1000+operand)));
                        absAdd++;
                    }
                    break;
                case 'A':
                    if (opcode >= 10) {
                         cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << "9999 Error: Illegal opcode; treated as 9999" <<endl;
                        memMap.insert(pair<int, string> (absAdd, "9999 Error: Illegal opcode; treated as 9999"));
                        absAdd++;
                    }
                    if (operand <= 512) {
                        cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << to_string(opcode*1000+ operand) <<endl;
                        memMap.insert(pair<int,string> (absAdd, to_string(opcode*1000+ operand)));
                        absAdd++;
                    } else {  
                        cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << to_string(opcode*1000) + " Error: Absolute address exceeds machine size; zero used" <<endl;
                        memMap.insert(pair<int,string> (absAdd, to_string(opcode*1000) + " Error: Absolute address exceeds machine size; zero used"));
                        absAdd++;
                    }
                    break;
                case 'E':
                    if (opcode >= 10) {
                         cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << "9999 Error: Illegal opcode; treated as 9999" <<endl;
                        memMap.insert(pair<int, string> (absAdd, "9999 Error: Illegal opcode; treated as 9999"));
                        absAdd++;
                    } else {
                        //error-external address too large to reference an entry in use list
                        int s = useTokens.size();  //size returns size_t how does this casting work
                        if (operand > s-1) {
                             cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                                << setw(4) << to_string(opcode*1000+operand) + " Error: External address exceeds length of uselist; treated as immediate" <<endl;
                            memMap.insert(pair<int, string> (absAdd, to_string(opcode*1000+operand) + " Error: External address exceeds length of uselist; treated as immediate"));
                            absAdd++;
                        }

                        //if this block is accessed , this means a useToken position is accessible
                        //error-symbol used in E instruction but not defined
                        else if (operand <= s-1) {
                            string getToken = useTokens.at(operand);
                            checkSymUse.at(operand) = true;
                            if (!symbolTable.count(getToken)) {
                                cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                                << setw(4) << to_string(opcode*1000) + " Error: " + getToken + " is not defined; zero used" << endl;
                                memMap.insert(pair<int, string> (absAdd, to_string(opcode*1000) + " Error: " + getToken + " is not defined; zero used"));
                                absAdd++;
                            } else {
                                int address = symbolTable.find(getToken)->second;
                                cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                                << setw(4) << to_string(opcode*1000+ address) << endl;
                                memMap.insert(pair<int,string> (absAdd, to_string(opcode*1000+ address)));
                                absAdd++;
                            }
                        }
                        
                    }
                    break;
                case 'R':
                    if (opcode >= 10) {
                         cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << "9999 Error: Illegal opcode; treated as 9999" << endl;
                        memMap.insert(pair<int, string> (absAdd, "9999 Error: Illegal opcode; treated as 9999"));
                        absAdd++;
                        break;
                    }
                    if (operand >= codeCount) {
                        operand = baseAddressMod;
                         cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << to_string(opcode*1000+ operand) + " Error: Relative address exceeds module size; zero used" << endl;
                        memMap.insert(pair<int,string> (absAdd, to_string(opcode*1000+ operand)+ " Error: Relative address exceeds module size; zero used"));
                        absAdd++;
                    } else {
                        //no errors
                         cout << setfill('0') << setw(3) << absAdd << ": " << setfill('0') 
                        << setw(4) << to_string(opcode*1000+baseAddressMod +operand) << endl;
                        memMap.insert(pair<int,string> (absAdd, to_string(opcode*1000+baseAddressMod +operand)));
                        absAdd++;
                    }
                    break;
            }
            j++;
        }
        // Rule 7: if a symbol in use list but not used in module
        for (int i = 0; i < checkSymUse.size(); i++) {
            if (checkSymUse.at(i) == false) {
                printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n",moduleNum,useTokens.at(i).c_str());
            }
        }
        checkSymUse.clear();   
        useTokens.clear();
        moduleNum++;
        baseAddressMod += codeCount;

    }
    if (set) {
        cout << "Memory Map" << endl;
        cout << "\n";
    }
    
    cout << "\n";
    //Rule 4: If a symbol is defined but not used, print a warning and continue
    map<string, int>:: iterator it;
    
    for (it = symbolandmod.begin(); it != symbolandmod.end(); it++) {
        bool flag = true;
        for (int i = 0; i < symdefnotused.size(); i++) {
            if (it->first == symdefnotused.at(i)) {
                flag = false;
                break;
            } 
            
        }
        if (!flag) {
            continue;
        }
        printf("Warning: Module %d: %s was defined but never used\n",it->second,it->first.c_str());
    }
}


string readSymbol(string s) {
    if (s.length() >= 16) return "lenerr";
    if (!((s.at(0) <= 'z' && s.at(0) >= 'a') || (s.at(0) <= 'Z' && s.at(0) >= 'A'))) {
        return "symerr";   //if the first char is not alpha return false, not a symbol
    }

    for (int i = 1; i < s.size(); i++) {
        if (!((s.at(i) <= 'z' && s.at(i) >= 'a') 
        || (s.at(i) <= 'Z' && s.at(i) >= 'A') || (s.at(i) <= '9'
        && s.at(i) >= '0'))) {
            return "symerr";   //if the rest of chars are not alphanumerical return false, not a symbol
        } 
    }
    return s;
}
void parseError(int errcode, int index, vector<TokOffLine> buffer) {   
    static const char* errstr[] = {
        "NUM_EXPECTED", 
        "SYM_EXPECTED",
        "ADDR_EXPECTED",
        "SYM_TOO_LONG",
        "TOO_MANY_DEF_IN_MODULE",
        "TOO_MANY_USE_IN_MODULE",
        "TOO_MANY_INSTR"
    };
    printf("Parse Error line %d offset %d: %s\n", buffer[index].lineNumber, buffer[index].lineOffset, errstr[errcode]);
}
int readInt(string s) {
    for (int i = 0; i < s.length(); i++) {
        if ((s.at(i) <= 'z' && s.at(i) >= 'a') || (s.at(i) <= 'Z' && s.at(i) >= 'A'))
            return -1;  //check to see if string contains any alphabet
    }
    int res = atoi(s.c_str());
    return res;
}
int passOne(const char *input) {
    int j = 0;
    int moduleNum = 1;
    int baseAddressMod = 0;
    int memSize = 0;  //should not exceed addressable memory of 512 words
    map<int, int> baseAddMod;  //maps module # -> absolute address
    vector<TokOffLine> tokBuffer;  //holds tokens  
    vector<defToken> defBuffer;//holds only the unique symbols,rel add, and mod num  in definitions
    map<string, bool> uniqueToken;//keeps track of which token were deemed unique for Rule 2
    vector<defTokenAndAdd> buf;  //collects tokens and rel add in each module then dump then repeat

    tokBuffer = tokenParser(input);  
    while (j < tokBuffer.size()) {
        baseAddMod.insert(pair<int, int> (moduleNum, baseAddressMod));  
        int defCount = readInt(tokBuffer[j].token);
        if (defCount == -1) {
            parseError(0,j,tokBuffer);  //Num was expected
            return -1;
        } else if (defCount > 16) {
            parseError(4,j,tokBuffer);   //too many defs
            return -1;
        }
        j++;
        
       
        for (int i = 0; i < defCount; i++) {
            /*If j = tokBuffer's size, then it is premature since we expect something after */
            if (j == tokBuffer.size() && defCount != 0 ) {
                    offsetandlinenum temp = getOffAndLine(input);  //struct
                    printf("Parse Error line %d offset %d: %s\n", temp.linenum, temp.offset, "SYM_EXPECTED");
                    return -1;  //stop processing 
            }
            string sym = readSymbol(tokBuffer[j].token);
            if (sym == "symerr") {
                parseError(1,j,tokBuffer);  //Sym was expected
                return -1;
            } else if (sym == "lenerr") {  //Sym was too long
                parseError(3,j,tokBuffer);
                return -1;
            } 
            
            j++;
            if (j == tokBuffer.size() && defCount != 0 ) {
                    offsetandlinenum temp = getOffAndLine(input);  //struct
                    printf("Parse Error line %d offset %d: %s\n", temp.linenum, temp.offset, "NUM_EXPECTED");
                    return -1;  //stop processing 
            }
            int temp = readInt(tokBuffer[j].token);
            if (temp == -1) {
                parseError(0,j,tokBuffer); //Num was expected
                return -1;
            }
            
            j++;
            buf.push_back( {sym, temp} );  
             //first check if it's stored in defBuffer
            
             bool check = true;
            for (vector<defToken>::iterator it = defBuffer.begin(); it != defBuffer.end(); it++) {
                if ( (*it).symbol == sym) {
                    check = false;
                    //check if the duplicated symbol is in the map here. if so set to false.
                    uniqueToken.find(sym) -> second = false;
                    break;
                }
            }
            if (check) {
                defBuffer.push_back( {sym,temp,moduleNum} );
                uniqueToken.insert(pair<string,bool> (sym,true));
            }
            
        }
        int useCount = readInt(tokBuffer[j].token);
        if (useCount == -1) {
            parseError(0,j,tokBuffer);  //Num was expected
            return -1;
        } else if (useCount > 16) {
            parseError(5,j,tokBuffer);  //too many use
            return -1;
        }
        j++;
        
        for (int i = 0; i < useCount; i++) {
            /*If j = tokBuffer's size, then it is premature since we expect something after */
            if (j == tokBuffer.size() && useCount != 0 ) {
                    offsetandlinenum temp = getOffAndLine(input);  //struct
                    printf("Parse Error line %d offset %d: %s\n", temp.linenum, temp.offset, "SYM_EXPECTED");
                    return -1;  //stop processing 
            }
            string sym = readSymbol(tokBuffer[j].token);
            if (sym == "symerr") {
                parseError(1,j,tokBuffer);  //Sym was expected
                return -1;
            } else if (sym == "lenerr") {  //Sym was too long
                parseError(3,j,tokBuffer);
                return -1;
            }
            j++;

        }
        
        int codeCount = readInt(tokBuffer[j].token);
        if (codeCount == -1) {
            parseError(0,j,tokBuffer);  //Num was expected
            return -1;
        }
        memSize += codeCount;
        if (memSize > 512) {
            parseError(6,j,tokBuffer);  //too many instr
            return -1;
        }
        
        j++;

        
        for (int i = 0; i < codeCount;i++) {
            /*If j = tokBuffer's size, then it is premature since we expect something after */
            if (j == tokBuffer.size() && codeCount != 0 ) {
                    offsetandlinenum temp = getOffAndLine(input);  //struct
                    printf("Parse Error line %d offset %d: %s\n", temp.linenum, temp.offset, "ADDR_EXPECTED");
                    return -1;  //stop processing 
            }
            char c = readIAER(tokBuffer[j].token);
            if (c == '@') {
                parseError(2,j,tokBuffer);  //address expected
                return -1;
            }
            j++;
            /*If j = tokBuffer's size, then it is premature since we expect something after */
            if (j == tokBuffer.size() && codeCount != 0 ) {
                    offsetandlinenum temp = getOffAndLine(input);  //struct
                    printf("Parse Error line %d offset %d: %s\n", temp.linenum, temp.offset, "NUM_EXPECTED");
                    return -1;  //stop processing 
            }
            int res = checkInstr(tokBuffer[j].token);
            if (res == -1) {
                parseError(0,j,tokBuffer);  //expect a num
                return -1;
            } 
            j++;
        }
        //check if address in definition exceeds size of module, print warning message after each module
        for (int i = 0; i < buf.size(); i++) {
            
            if (uniqueToken.find(buf[i].sym) -> second == false) {
                break;
            }
            if (buf[i].address >= codeCount) {
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", 
                moduleNum, buf[i].sym.c_str(), buf[i].address, codeCount-1);
                for (int j = 0; j < defBuffer.size(); j++) {
                    if (buf[i].sym == defBuffer[j].symbol) {
                        defBuffer[j].relAdd = 0;
                    }
                }
            }
        }
        buf.clear();  //clear contents for next module 
        moduleNum++;
        baseAddressMod += codeCount;

    }
    //Printing out Symbol Table  -- Comes after all modules processed because how would you account for multiple definitions?
    // use a map for indicating unique tokens defined multiple times  <string, bool>  xy true, z true, initially  
    //then modify the bool as you  go to other modules and find duplicates
    //uniqueToken and defBuffer should have a 1 to 1 correspondence

    map<int, int>::iterator iter;  //to identify the base address of appropriate module
    cout << "Symbol Table" << endl;
    for (int i = 0; i < defBuffer.size(); i++) {
        if (uniqueToken.find(defBuffer[i].symbol) -> second == false) {

            iter = baseAddMod.find(defBuffer[i].modNum);//holds only the unique symbols,rel add, and mod num  in definitions
            int res = iter->second + defBuffer[i].relAdd;
            symbolTable.insert(pair<string,int> (defBuffer[i].symbol,res));
            cout << defBuffer[i].symbol << "=" << res << " Error: This variable is multiple times defined; first value used" << endl; //Needs to be on same line
             
        } else {
            iter = baseAddMod.find(defBuffer[i].modNum);//holds only the unique symbols,rel add, and mod num  in definitions
            int res = iter->second + defBuffer[i].relAdd;
            symbolTable.insert(pair<string,int> (defBuffer[i].symbol,res));
            cout << defBuffer[i].symbol << "=" << res << "\n";
        }
    }
    cout << "\n";
    return 0;   
    
}
int checkInstr(string s) {
    //check if string s is a number 
    int temp = readInt(s);
    if (temp == -1) {
        return -1;
    }
    return temp;
}
    
char readIAER(string s) {
    char c = s.at(0);
    if (c != 'I' && c != 'A' && c != 'E' && c != 'R') {
        return '@';
    }
    return c;
    
}
//for identifying eof and when we expect something but we are out of tokens
offsetandlinenum getOffAndLine(const char *s) {
    ifstream file(s);
    string store;
    offsetandlinenum res;
    vector<int> listLeng;
    int lineNum = 0;
    int lineOffset =1;
    while (getline(file,store)) {
        lineNum++;
        listLeng.push_back(store.length());
    }
    lineOffset = listLeng.back()+1;
    res.linenum = lineNum;
    res.offset = lineOffset;
    return res;
}

// To create vectors containing corresponding tokens, offset info and line num info
vector<TokOffLine> tokenParser(const char *s) {
    ifstream file(s);
    string store;
    int lineNum = 0;
    int nextTokPos = 0;
    int lineOffset = 1;
    vector<int> listLeng;
    vector<int> rVec;   //records the line number of each token
    vector<int> cVec;   //records the offset of each token
    vector<string> tokVec; //records the accumulation of tokens
    vector<TokOffLine> packet;  //holds token,offset, and line num information
    
    while (getline(file,store)) {
        lineNum++;
        nextTokPos = 0;
        stringstream line(store);
        string token;
        listLeng.push_back(store.length());
        while (line >> token) {
            tokVec.push_back(token);
            rVec.push_back(lineNum);
            lineOffset = store.find(token,nextTokPos);  //returns index of first encounter of token
            lineOffset++;
            cVec.push_back(lineOffset);
            packet.push_back( {token,lineNum,lineOffset} );  // use -std=c++11
         // cout << "Token " << token << " is at line " << lineNum << " with offset position " << lineOffset << endl;
            nextTokPos = lineOffset + token.length();  //it will be at least after the pos of token
        }
        
    }

    return packet;
} 

int main(int argc, char* argv[]) {
    ifstream file;
    if (argc == 2) {   //first argument is the name of compiled file, second one is the input file
        char *inputFile = argv[1];
        file.open(inputFile);
        if (!file.is_open()) {
            cout << "Not able to open file: " << argv[1] << endl;
            return 0;
        }
        int status = passOne(argv[1]); 
        if (status == -1) {  //if 1st pass detects any parse errors stop processing and quit
            return 0;
        }
        passTwo(argv[1]);
        file.close();
        return 0;
    } else {
        cout << "argv[0] should be compiled file name and argv[1] should be path of input file" << endl;
        return 0;
    }
}

