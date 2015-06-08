/*
This software has been developed by Ralph M. Butler for the 
sole use of his students.  No one else may copy any part 
of it for any reason, except with explicit permission.

 Student: Peter Corazao
 Class: OSD Jan 29th 2011
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
 
#define NOP      0
#define L        1
#define LR       2
#define LA       3
#define LN       4
#define LX       5
#define ST		 6
#define STN      7
#define STX      8
#define ADD      9
#define ADDR    10
#define INC     11
#define INCR    12
#define SUB     13
#define SUBR    14
#define CMP     15
#define CMPR    16
#define J	    17
#define JEQ     18
#define JEQR    19
#define JNE     20
#define JNER    21
#define JLT     22
#define JLTR    23
#define JGT     24
#define JGTR    25
#define JLE     26
#define JLER    27
#define JGE     28
#define JGER    29
#define JR      30
#define BLKIOR  31
#define BLKIO   32
#define DUMPR   88
#define DUMPI   89
#define HALT    99

#define MAXOPCDS  50
struct
{
    char opcdtxt[9];
    int opcode;
} opcode_table[MAXOPCDS] = { 
						 "NOP",     NOP,
	                     "L",       L,
	                     "LR",      LR,
	                     "LA",      LA,
	                     "LN",      LN,
	                     "LX",      LX,
	                     "ST",		ST,
	                     "STN",		STN,
	                     "STX",		STX,
	                     "ADD",		ADD,
	                     "ADDR",	ADDR,
	                     "INC",		INC,
	                     "INCR",	INCR,
	                     "SUB",		SUB,
	                     "SUBR",	SUBR,
	                     "CMP",		CMP,
	                     "CMPR",	CMPR,
	                     "J",		J,
	                     "JEQ",		JEQ,
	                     "JEQR",	JEQR,
	                     "JNE",		JNE,
	                     "JNER",	JNER,
	                     "JLT",		JLT,
	                     "JLTR",	JLTR,
	                     "JGT",		JGT,
	                     "JGTR",	JGTR,
	                     "JLE",		JLE,
	                     "JLER",	JLER,
	                     "JGE",		JGE,
	                     "JGER",	JGER,
	                     "JR",		JR,
	                     "BLKIOR",  BLKIOR,
	                     "BLKIO",   BLKIO,
	                     "DUMPR",   DUMPR,
	                     "DUMPI",   DUMPI,
	                     "HALT",    HALT };

struct
{
    char symbol[9];
    int loc;
} symbol_table[50];

int realmem[16*1000];

#define BLOCKSIZE 128
int IOWaitQueue[4][5];//pid;cmd;memaddr;disknum;status;
int run_vdisk_mgr;
int IOError;

#define NREGS 32
int regs[NREGS];

#define PC    regs[NREGS-1] //Program Counter
#define FLAGS regs[NREGS-2] //Refered to as CC
#define XREG  regs[NREGS-3] //Index Register

int stidx, rmidx, loc_ctr;

char input_line[100];
char label[9], opcdtxt[9], opndtxt1[32], opndtxt2[32];

FILE *infile;

main(int argc, char *argv[])
{
    int i;

	for(i=0;i<4;i++)
		IOWaitQueue[i][0] = -1;

	//no op realmem
	for( i = 0; i < (16*1000); i++)	
		realmem[i] = 0;
	
    stidx = 0;
    rmidx = 0;
    loc_ctr = 0;

    if (argc != 2)
    {
	printf("usage: %s filename\n",argv[0]);
	exit(99);
    }

    if ((infile = fopen(argv[1],"r")) == NULL)
    {
	printf("open failed for file: %s\n",argv[1]);
	exit(99);
    }
    
    asm_pass1();

    rewind(infile);
    asm_pass2();
    fclose(infile);

    interpret();
}

asm_pass1()
{
    int i;

    while (fgets(input_line,100,infile) != NULL)
    {
	tokenize();
	if (!opcdtxt[0])  /* ignore the line, e.g. empty line */
	    continue;
	if (label[0])
	{
	    strcpy(symbol_table[stidx].symbol,label);
	    symbol_table[stidx].loc = loc_ctr;
	    stidx++;
	}
	if (strcmp(opcdtxt,"WORDS") == 0)
	{
	    loc_ctr += opnd_atoi(opndtxt1);
	}
	else if (strcmp(opcdtxt,"INT") == 0)
	{
	    loc_ctr++;
	}
	else
	{
	    loc_ctr += 3;
	}
    }
}

asm_pass2()
{
    int i;

    while (fgets(input_line,100,infile) != NULL)
    {
	tokenize();
	if (!opcdtxt[0])  /* ignore the line, e.g. empty line */
	    continue;
	if (strcmp(opcdtxt,"WORDS") == 0)
	{
	    rmidx += opnd_atoi(opndtxt1);
	}
	else if (strcmp(opcdtxt,"INT") == 0)
	{
	    realmem[rmidx] = opnd_atoi(opndtxt1);
	    rmidx++;
	}
	else
	{
	    realmem[rmidx]   = xlate_opcode(opcdtxt);
	    realmem[rmidx+1] = opnd_atoi(opndtxt1);
	    realmem[rmidx+2] = opnd_atoi(opndtxt2);
	    rmidx += 3;
	}
    }
}




tokenize()
{
    int i;
    char *s, *delimeter = " \t,";

    label[0]    = '\0';
    opcdtxt[0]  = '\0';
    opndtxt1[0] = '\0';
    opndtxt2[0] = '\0';

    /* ignore comments and blank lines */
    for (i=0; input_line[i]; i++)
    {
	if (input_line[i] == '#'  ||  input_line[i] == '\n')
	{
	    input_line[i] = '\0';
	    break;
	}
    }
    for (i=0; input_line[i]; i++)
	if (!isspace(input_line[i]))
	    break;
    if (i >= strlen(input_line))  /* it was empty */
	return;

    /* do the actual tokenization */
    for (i=0; input_line[i]; i++)
	input_line[i] = toupper(input_line[i]);
    if (isalpha(input_line[0]))
    {
	strcpy(label,strtok(input_line,delimeter));
	strcpy(opcdtxt,strtok(NULL,delimeter));
    }
    else
	strcpy(opcdtxt,strtok(input_line,delimeter));
    if (s = strtok(NULL,delimeter))
	strcpy(opndtxt1,s);
    if (s = strtok(NULL,delimeter))
	strcpy(opndtxt2,s);
	
	//printf("label:%s",label);
	//printf(" opcdtxt:%s",opcdtxt);
    //printf(" opndtxt1:%s",opndtxt1);
    //printf(" opndtxt2:%s\n",opndtxt2);
}

/* must be enhanced to support negative numbers as operands to INT */
opnd_atoi(char *opndtxt)
{
    int i;

    if (isdigit(*opndtxt))        /* register or address */
	return(atoi(opndtxt));
    for (i=0; i < stidx; i++)
    {
	if (strcmp(opndtxt,symbol_table[i].symbol) == 0)
	    return(symbol_table[i].loc);
    }
    return(0);
}

xlate_opcode(char *opcdtxt)
{
    int i;

    for (i=0; i < MAXOPCDS; i++)
    {
	if (strcmp(opcdtxt,opcode_table[i].opcdtxt) == 0)
	    return(opcode_table[i].opcode);
    }
    return(-1);
}


void *vdisk_mgr(void *pnum)
{
	int *test = (int *)pnum;
	int i,buf;
	int cmd,memaddr,disknum,status;
	
	FILE *vDISK;
	//IOWaitQueue[4][5];//pid;cmd;memaddr;disknum;status;
	//IOError
	
	if((vDISK = fopen("VDISK","r+d"))==NULL)
    {
    	printf("Unable to mount VDISK, please run p3fmt\n");
		exit(99);    	
    }
	
	while(run_vdisk_mgr !=0)
	{
		if(IOWaitQueue[0][0] != -1)
		{
			//get cmd
			cmd = IOWaitQueue[0][1];
			memaddr = IOWaitQueue[0][2];
			disknum = IOWaitQueue[0][3];
			status = IOWaitQueue[0][4];
		
			//****************************************************
			//seek
			//printf("cmd:%d memaddr:%d diskNum:%d status:%d\n",realmem[cmd],realmem[realmem[memaddr]],realmem[disknum],realmem[status]);
			int x = 0; //slow down write thread a bit
			for(i=0;i<100;i++)
			{
				x++;	
			}
			
			if(realmem[disknum]>=0 && realmem[disknum] <=127){
				fseek(vDISK,BLOCKSIZE*realmem[disknum],SEEK_SET);
			}else if(realmem[disknum]==999){
				fseek(vDISK,0,SEEK_CUR);
			}else{
				printf("Bad Block:%d\n",realmem[disknum]);	
			}
			
			if(realmem[cmd] == 0){
				//read
				i = fread(&buf,sizeof(buf),1,vDISK);
				realmem[realmem[memaddr]] = buf;
				//printf("read:%d\n",i);	
			}else if(realmem[cmd]==1){
				//write				
				buf = realmem[realmem[memaddr]];
   				i = fwrite(&buf,sizeof(buf),1,vDISK);
   				//printf("wrote:%d\n",i);				
			}else if(realmem[cmd]==2)
			{
				//seek
				i=1;	
			}
			
			if(i==1)
			{
				//IO COMPLETE
				//printf("IOComplete for cmd:%d\n",realmem[cmd]);
				realmem[status] = 1;
							
				//dequeue
				IOWaitQueue[0][0] = -1;//pid
				IOWaitQueue[0][1] = -1;//cmd
				IOWaitQueue[0][2] = -1;//memaddr
				IOWaitQueue[0][3] = -1;//disknum
				IOWaitQueue[0][4] = -1;//status	
			}else{
				//failed IO
				printf("i:%d\n");
				IOError =1;	
			}		
			//****************************************************
		}
	}
	
	fclose(vDISK);
	pthread_exit(0);
}

interpret()
{
    int i, opcd, opnd1, opnd2, buf;
    int cmd,memaddr,disknum,status; 
	int r1=1;
	pthread_t diskmgr;
	
	IOError = 0;
	run_vdisk_mgr = 1;
	pthread_create(&diskmgr,NULL,vdisk_mgr,(void *)&r1);
		
    PC = 0; //nregs -1 a register
    //printf("\n");
    for (;;)
    {
	
	/* check for and handle interrupts (e.g. I/O) here if necessary */
	if(IOError==1)
	{
		printf("IOError PC=%d\n", PC);
		run_vdisk_mgr = 0;
		pthread_join(diskmgr,NULL);
		exit(99);	
	}
		
	//fetch
	opcd = realmem[PC];
	opnd1 = realmem[PC+1];
	opnd2 = realmem[PC+2];
	PC += 3;
	//printf("opcd:%d, opnd1:%d, opnd2:%d\n",opcd,opnd1,opnd2);
	//sleep(1);	
	switch(opcd)
	{
	    case NOP:
	    	//No op
	    	break;
	    case L:
	    	 // L	r,n
	   		 //load reg r with contents of memory loc n (n may be label)
			regs[opnd1] = realmem[opnd2];
			
			break;
	    case LR:
	    	// LR 	r1,r2
	    	//load contents of r2 into r1
			regs[opnd1] = regs[opnd2];
			
			break;
	    case LA:
	    	//LA	r,n
	    	//load reg r with address of memory loc n (n may be label)
			regs[opnd1] = opnd2;
			
			break;
	    case LN:
	    	//LN	r1,r2
			//load into r1 the fullword at the address in r2
			regs[opnd1] = realmem[regs[opnd2]];
			
			break;
	    case LX:
	    	//LX	r1,n
			//load reg r1 with contents of memory loc n; indexed
			regs[opnd1] = realmem[opnd2+XREG];
			
			break;	    
		case ST:
			//ST	r,n
			//store req r at loc n (n may be label)
			realmem[opnd2] = regs[opnd1];
			
			break;
		case STN:
			//STN   r1,r2
			//store reg r1 in the word at the addr in r2
			realmem[regs[opnd2]] = regs[opnd1];
			
			break;
		case STX:
			//STX   r,n
			//store contents of r at loc n (indexed)
			realmem[opnd2+XREG] = regs[opnd1];
			
			break;
		case ADD:
			//ADD r,n
			//add contents of loc n to reg r (n may be label)
			regs[opnd1] += realmem[opnd2];
			
			break;
		case ADDR:
			//ADDR  r1,r2
			//add contents of reg r2 to reg r1
			regs[opnd1] += regs[opnd2];
			
			break;
		case INC:
			//INC   n
			//incr contents of loc n by 1
			realmem[opnd1] += 1;
			
			break;
		case INCR:
			//INCR r
			//incr r by 1
			regs[opnd1] += 1;
			break;
		case SUB:
			//SUB r,n
			//subtract contents of loc n from reg r (n may be label)
			regs[opnd1] -= realmem[opnd2];
			
			break;
		case SUBR:
			//SUBR r1, r2
			//subtract contents of reg r2 from reg r1
			regs[opnd1] -= regs[opnd2];
			
			break;
		case CMP:
			//CMP r,n
			//compare reg r to contents of loc n and set CC (n may be label)
			if(regs[opnd1] == realmem[opnd2])
			 	FLAGS = 0;
			
			if(regs[opnd1] < realmem[opnd2])
				FLAGS = -1;
				
			if(regs[opnd1] > realmem[opnd2])
				FLAGS = 1; 
			
			break;
		case CMPR:
			//CMPR r1,r2
			//compare reg r1 to contents of reg2 and set CC
			if(regs[opnd1] == regs[opnd2])
			 	FLAGS = 0;
			
			if(regs[opnd1] < regs[opnd2])
				FLAGS = -1;
				
			if(regs[opnd1] > regs[opnd2])
				FLAGS = 1;
			
			break;
		case J:
			//J  n 
			//jump to location n (n may be label)
			PC = opnd1;
			
			break;
		case JEQ:
			//JEQ n
			//jump to location n(n may be label) if CC == 0
			if(FLAGS==0)
				PC = opnd1;
			
			break;
		case JEQR:
			//JEQR r
			//jump to address contained in register r if CC == 0
			if(FLAGS==0)
				PC = regs[opnd1];
			
			break;
		case JNE:
			//JNE n
			//jump to location n (n may be label) if CC != 0
			if(FLAGS != 0)
				PC = opnd1;
				
			break;
		case JNER:
			//JNER r
			//jump to address contained in register r if CC != 0
			if(FLAGS !=0)
				PC = regs[opnd1];
			break;
		case JLT:
			//JLT n
			//jump to location n (n may be label) if CC == -1
			if(FLAGS == -1)
				PC = opnd1;
			
			break;
		case JLTR:
			//JLTR r
			//jump to address contained in register r if CC == -1
			if(FLAGS == -1)
				PC = regs[opnd1];
			
			break;
		case JGT:
			//JGT n
			//jump to location n (n may be label) if CC == 1
			if (FLAGS == 1)
				PC = opnd1;
				
			break;
		case JGTR:
			//JGTR r
			//jump to address contained in register r if CC == 1
			if (FLAGS == 1)
				PC = regs[opnd1];
			
			break;
		case JLE:
			//JLE n
			//jump to location n (n maybe label) if CC == -1 or CC == 0
			if(FLAGS == -1 || FLAGS == 0)
				PC = opnd1;
				
			break;
		case JLER:
			//JLER r
			//jump to address contained in register r if CC==-1 or CC==0
			if(FLAGS == -1 || FLAGS == 0)
				PC = regs[opnd1];
				
			break;
		case JGE:
			//JGE n
			//jump to location n (n maybe label) if CC == 1 or CC == 0
			if(FLAGS == 1 || FLAGS == 0)
				PC = opnd1;
				
			break;
		case JGER:
			//JGER r
			//jump to address contained in register r if CC ==1 or CC ==0
			if(FLAGS == 1 || FLAGS ==0)
				PC = regs[opnd1];
				
			break;
		case JR:
			//JR r
			//jump to address contained in register r
			PC = regs[opnd1];
			break;
		case BLKIOR:
			//BLKIOR r
			//r contains the address to the command
			//1) command 0-read,1-write,2-seek
			//2) memory address for the transfer (not used by seek)
			//3) disk block number for operation; 0 - 127 999 is special for current
			//4) status word - should be init to 0 by user; set to 1 if successful
			;
			cmd = regs[opnd1];
			memaddr = regs[opnd1]+1;
			disknum = regs[opnd1]+2;
			status = regs[opnd1]+3;
			
			//add to queue
			IOWaitQueue[0][0] = 0;//pid
			IOWaitQueue[0][1] = cmd;
			IOWaitQueue[0][2] = memaddr;
			IOWaitQueue[0][3] = disknum;
			IOWaitQueue[0][4] = status;
			
			break;	
		case BLKIO:
			//BLKIOR n
			//n label is the address to the command
			//1) command 0-read,1-write,2-seek
			//2) memory address for the transfer (not used by seek)
			//3) disk block number for operation; 0 - 127 999 is special for current
			//4) status word - should be init to 0 by user; set to 1 if successful
			;	
			cmd = opnd1;
			memaddr = opnd1+1;
			disknum = opnd1+2;
			status = opnd1+3;
		
			//add to queue
			IOWaitQueue[0][0] = 0;//pid
			IOWaitQueue[0][1] = cmd;
			IOWaitQueue[0][2] = memaddr;
			IOWaitQueue[0][3] = disknum;
			IOWaitQueue[0][4] = status;
			
			break;	
	    case DUMPR:
	    	//DUMPR
	    	//print registers as integers
	    	//print them 8 per line separated by single blanks
			printf("\nDUMPR from process:%d",0);
			for (i=0; i < NREGS; i++)
			{
				if ((i % 8) == 0)
				printf("\n");
				printf("%d ",regs[i]);
			}
			printf("\n");
			
			break;
		case DUMPI:
			//DUMPI m,n
			//print m words of memory as integers beginning at loc n; n may be label;
			//blank separted and 8 words per line;
			printf("\nDUMPI from process:%d beginning at location memory %d for %d words",0,opnd2,opnd1);
			for (i=0; i < opnd1; i++)
			{
				if ((i % 8) == 0)
				printf("\n");
				printf("%d ",realmem[opnd2+i]);
			}
			printf("\n");
			
			break;
	    case HALT:
	    	//printf("halting\n");
	    	run_vdisk_mgr = 0;
	    	pthread_join(diskmgr,NULL);
			exit(realmem[PC+1]);
		break;
	    default:
		printf("invalid opcode=%d PC=%d\n", realmem[PC],PC);
		run_vdisk_mgr = 0;
		pthread_join(diskmgr,NULL);
		exit(99);
	}
    }
}
