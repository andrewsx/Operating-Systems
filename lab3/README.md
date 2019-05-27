Lab 3: Virtual Memory Management

ran code through GCC Compiler 4.8.5 on linserv1 CIMS Machine

To run this program:

$ make
$ ./mmu [-a<algo>] [-o<options>] [-f<num_frames>] inputfile randfile

OR to remove file:

$ make clean 


Key:
<algo> 
FIFO = 'f'
Random = 'r'
Clock = 'c'
Enhanced Second Chance/NRU = 'e'
Aging = 'a'
Working Set = 'w'

<options> 
Standard output = 'O'
Page Table option = 'P'
Frame Table option = 'F'
Summary line = 'S'
Prints current process's page table after each instruction = 'x'
Prints page table of all processes after each instruction = 'y'
Prints frame table after each instruction = 'f'

<num_frames>
any integer from 0 - 128