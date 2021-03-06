#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#undef mips			/* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo (int changedReg, int changedMem);
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;
//R instruction funct codes
int addu = 0x21;
int and = 0x24;
int jr = 0x08;
int or = 0x25;
int slt = 0x2a;
int sll = 0x00;
int srl = 0x02;
int subu = 0x23;

//I intruction op code
int addiu = 0x9;
int andi = 0xc;
int beq = 0x4;
int bne = 0x5;
int lui = 0xf;
int lw = 0x23;
int ori = 0xd;
int sw =0x2b;

//J instruction op code
int j = 0x2;
int jal = 0x3;


/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
  int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }
    
    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein)) {
	/*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;
    
    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);

        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

     /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */
        Decode (instr, &d, &rVals);
        
        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);

	UpdatePC(&d,val);

        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);

        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
        changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    return mips.memory[(addr-0x00400000)/4];
}

/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {
    if(instr == 0)                              //invalid instruction
        exit(0);
    unsigned int temp;
    temp = instr;
    temp = temp>>26;
    d->op = temp;                               //op
    temp = instr<<6;

    if(d->op == 0){
        d->type = R;                            //R instruction

        temp = temp>>27;
        d->regs.r.rs = temp;
        rVals->R_rs = mips.registers[temp];     //rs

        temp = instr<<11;
        temp =temp>>27;
        d->regs.r.rt = temp;
        rVals->R_rt = mips.registers[temp];     //rt

        temp = instr<<16;
        temp = temp>>27;
        d->regs.r.rd = temp;
        rVals->R_rd = mips.registers[temp];     //rd

        temp = instr<<21;
        temp = temp>>27;
        d->regs.r.shamt = temp;                 //shamt

        temp = instr<<26;
        temp = temp>>26;
        d->regs.r.funct = temp;                 //funct

    }

    else if(d->op == j || d->op == jal){
        d->type = J;                            //J instruction
        temp = temp>>4;
        d->regs.j.target = temp;
    }

    else if(d->op == addiu || d->op == andi || d->op == beq || d->op == bne || d->op == lui || d->op == lw || d->op == ori || d->op == sw){
        d->type = I;                            //I instruction
        temp = temp>>27;
        d->regs.i.rs = temp;
        rVals->R_rs = mips.registers[temp];     //rs

        temp = instr<<11;
        temp = temp>>27;
        d->regs.i.rt = temp;
        rVals->R_rt = mips.registers[temp];     //rt

        temp = instr<<16;
        temp = temp>>16;
        if((temp & 0x00008000) > 0)
		temp =  temp | 0xffff0000;
        d->regs.i.addr_or_immed = temp;         //imm
    }
    else
        exit(0);                                //if none of these exit   
}

/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {

    //R INSTRUCTION
    if(d->type == R){
        if(d->regs.r.funct == addu){
            printf("addu\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        else if(d->regs.r.funct == and){
            printf("and\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        else if(d->regs.r.funct == jr){
            printf("jr\t$%d\n", d->regs.r.rs);
        }
        else if(d->regs.r.funct == or){
            printf("or\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        else if(d->regs.r.funct == slt){
            printf("slt\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        else if(d->regs.r.funct == sll){
            printf("sll\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.shamt);
        }
        else if(d->regs.r.funct == srl){
            printf("srl\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.shamt);
        }
        else if(d->regs.r.funct == subu){
            printf("subu\t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
    }

    //I INSTRUCTION
    else if(d->type == I){
        if(d->op == addiu){
            printf("addiu\t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
        else if(d->op == andi){
            printf("andi\t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
        else if(d->op == beq){
            printf("beq\t$%d, $%d, 0x%8.8x\n", d->regs.i.rs, d->regs.i.rt,((mips.pc + 12) + d->regs.i.addr_or_immed) +1);
        }
        else if(d->op == bne){
            printf("bne\t$%d, $%d, 0x%8.8x\n", d->regs.i.rs, d->regs.i.rt,((mips.pc + 12) + d->regs.i.addr_or_immed) +1);
        }
        else if(d->op == lui){
            printf("lui\t$%d, %d\n", d->regs.i.rt, d->regs.i.addr_or_immed);
        }
        else if(d->op == lw){
            printf("lw\t$%d, %d(%d)\n", d->regs.i.rt, d->regs.i.addr_or_immed, d->regs.i.rs);
        }
        else if(d->op == ori){
            printf("ori\t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
        else if(d->op == sw){
            printf("sw\t$%d, %d(%d)\n", d->regs.i.rt, d->regs.i.addr_or_immed, d->regs.i.rs);
        }

    }
    else if (d->type == J){
        if(d->op == j){
            printf("j\t0x%8.8x\n", d->regs.j.target);
        }
        else if(d->op == jal){
            printf("jal\t0x%8.8x\n", d->regs.j.target);
        }
    }
   else
        exit(0);

}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */
    if(d->type == R){
        if(d->regs.r.funct == addu){
            int sum = rVals->R_rs + rVals->R_rt;
            return sum;
        }
        else if(d->regs.r.funct == and){
            int result = rVals->R_rs & rVals->R_rt;
            return result;
        }
        else if(d->regs.r.funct == jr){
            int result = rVals->R_rs;
            return result;
        }
        else if(d->regs.r.funct == or){ 
             int result = rVals->R_rs | rVals->R_rt;
            return result;
        }
        else if(d->regs.r.funct == slt){
            if(rVals->R_rs < rVals->R_rt)
                return 1;
            else
                return 0;
        }
        else if(d->regs.r.funct == sll){
            int result = rVals->R_rt<<d->regs.r.shamt;
            return result;

        }
        else if(d->regs.r.funct == srl){
            int result = rVals->R_rt>>d->regs.r.shamt;
            return result;
        }
        else if(d->regs.r.funct == subu){
            int diff = rVals->R_rs - rVals->R_rt;
            return diff;
        }

    }
    else if(d->type == I){
        if(d->op == addiu){
            int sum = rVals->R_rs + d->regs.i.addr_or_immed;
            return sum;
        }
        else if(d->op == andi){
            int result = rVals->R_rs & d->regs.i.addr_or_immed;
            return result;
        }
        else if(d->op == beq){
            if((rVals->R_rs - rVals->R_rt) == 0) //they are equal
                return(d->regs.i.addr_or_immed<<2);
            else
                return 0;
        }
        else if(d->op == bne){
            if((rVals->R_rs - rVals->R_rt) != 0) //they are not equal
                return(d->regs.i.addr_or_immed<<2);
            else
                return 0;
        }
        else if(d->op == lui){
            int result = d->regs.i.addr_or_immed<<16;  //extract upper 16 bits
            return result;
        }
        else if(d->op == lw){
            int result = rVals->R_rs + d->regs.i.addr_or_immed;
            return result;
        }
        else if(d->op == ori){
            int result = rVals->R_rs | d->regs.i.addr_or_immed;
            return result;
        }
        else if(d->op == sw){
            int result = rVals->R_rs + d->regs.i.addr_or_immed;
            return result;
        }

    }
    else if(d->type == J){
        if(d->op == jal)
            return(mips.pc+4);

    }
  return 0;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    mips.pc+=4;
    /* Your code goes here */
    if(d->type == R){
        if(d->regs.r.funct == jr){
            mips.pc = val;
        }
    }
    else if(d->type == I){
        if(d->op == beq || d->op == bne){
            mips.pc = mips.pc + val;
        }
    }
    else{
        if(d->op == j || d->op == jal){
            mips.pc = d->regs.j.target;
        }
    }
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    /* Your code goes here */
  return val;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    /* Your code goes here */

    if(d->type == R){
        if(d->op == jr){
            *changedReg = -1;
        }
        else{
            mips.registers[d->regs.r.rd] = val;
            *changedReg = d->regs.r.rd;
        }
    }
    else if(d->type == I){
        if(d->op == beq || d->op == bne || d->op == sw){
            *changedReg = -1;
        }
        else{
            mips.registers[d->regs.i.rt] = val;
            *changedReg = d->regs.i.rt;
        }
    }
    else if(d->type == J){
        if(d->op == jal){
            mips.registers[31] = val;       //$ra
            *changedReg = 31;
        }
        else{
            *changedReg = -1;
        }
    }
    else
        *changedReg = -1;
}
