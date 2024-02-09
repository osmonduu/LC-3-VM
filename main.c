#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <Windows.h>
#include <conio.h>  // _kbhit

#define MEMORY_MAX (1 << 16)        // store 2^16 into MEMORY_MAX global variable by right shifting binary 1 16 times 
uint16_t memory[MEMORY_MAX];        // memory will be represented in an array of size 65536

/* Registers */
enum 
{
    //R0-R7 are general purpose registers
    R_R0 = 0,
    R_R1, 
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,       // program counter: unsigned int which indicates the next instruction in memory to execute
    R_COND,     // condition flag 
    R_COUNT   
};
uint16_t reg[R_COUNT];      // store registers in an array of unsigned 16 bit ints

/* Instruction set - Opcodes */ 
enum
{
    OP_BR = 0,  // branch
    OP_ADD,     // add
    OP_LD,      // load
    OP_ST,      // store
    OP_JSR,     // jump register
    OP_AND,     // bitwise and
    OP_LDR,     // load register
    OP_STR,     // store register
    OP_RTI,     // unused
    OP_NOT,     // bitwise not
    OP_LDI,     // load indirect
    OP_STI,     // store indirect
    OP_JMP,     // jump
    OP_RES,     // reserved (unused)
    OP_LEA,     // load effective address
    OP_TRAP     // execute trap
};

/* Condition flags */ 
enum 
{
    FL_POS = 1 << 0,    // P = 001
    FL_ZRO = 1 << 1,    // Z = 010
    FL_NEG = 1 << 2     // N = 100
};

/*------------------------------------------------------------------------------------------------------------*/

/* Input Buffering - used to access the keyboard in windows */ 
HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); /* save old mode */
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  /* no input echo */
            ^ ENABLE_LINE_INPUT; /* return when one or
                                    more characters are available */
    SetConsoleMode(hStdin, fdwMode); /* set new mode */
    FlushConsoleInputBuffer(hStdin); /* clear buffer */
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}

/* Handle Interrupt */ 
void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

/* Sign Extend */
uint16_t sign_extend(uint16_t x, int bit_count)
{
    // check if the sign bit is 1 -> (negative number): this checks the sign bit to the left of the number given
    if ((x >> (bit_count - 1)) & 1) 
    {
        // sign extend 1 for a total of 16 bits: left shift so that all the bits to the left of the input, x, gets OR'd and become 1
        x |= (0xFFFF << bit_count);
    }
    return x;
}

/* Update Flags */
void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15)      // a 1 in the left-most bit indicates negative value
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

/*------------------------------------------------------------------------------------------------------------*/

/* Main Loop */
int main(int argc, const char* argv[])
{
    /* Load Arguments */ 
    if (argc < 2) 
    {
        // show usage string
        printf("lc3 [image-file1] ... \n");
        exit(2);
    }

    for (int j  = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    /* Setup */
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    // since exactly one condition flag should be set at any given time, set the z flag
    reg[R_COND] = FL_ZRO;

    // set the PC to starting position
    // 0x3000 is the default 
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running) 
    {
        /* FETCH */
        // increment pc to arrive at address to a location in memory which contains an address of the value to load
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op)
        {
            case OP_ADD:
                ADD(instr);
                break;
            case OP_AND:
                AND(instr);
                break;
            case OP_NOT:
                NOT(instr);
                break;
            case OP_BR:
                BR(instr);
                break;
            case OP_JMP:
                JMP(instr);
                break;
            case OP_JSR:
                JSR(instr);
                break;
            case OP_LD:
                LD(instr);
                break;
            case OP_LDI:
                LDI(instr);
                break;
            case OP_LDR:
                LDR(instr);
                break;
            case OP_LEA:
                LEA(instr);
                break;
            case OP_ST:
                ST(instr);
                break;
            case OP_STI:
                STI(instr);
                break;
            case OP_STR:
                STR(instr);
                break;
            case OP_TRAP:
                
                break;
            case OP_RES:
            case OP_RTI:
            default:

                break;
            
        }
    }
    /* Shutdown */
    restore_input_buffering(); 
}


/* ---------------------------------------------------------LOGIC FOR OPCODES--------------------------------------------------------- */
/* see https://www.jmeiners.com/lc3-vm/supplies/lc3-isa.pdf for further explanation of the opcode logic*/

/* ADD logic */
void ADD(uint16_t instr) 
{
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0x7;
    // first operand (SR1)
    uint16_t r1 = (instr >> 6) & 0x7;
    // whether we are in immediate mode - immediate mode indicates that the second operand is a value instead of a register
    uint16_t imm_flag = (instr >> 5) & 0x1;

    // in immediate mode, the second value is embedded in the right0most 5 bits of the instruction
    if (imm_flag)
    {   
        // immediate data is 5 bits in length (2^5=32)
        // values which are shorter than 16 bits need to be sign extended
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
    }
    // in register mode, the second value to add is found in a register
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }

    // any time an instruction modifies a reigster, the condition flags need to be updated
    update_flags(r0);
}

/* AND logic - take the first operand and second operand, perform bitwise logical AND, then store into destination register*/
void AND(uint16_t instr)
{
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0x7;
    // first operand (SR1)
    uint16_t r1 = (instr >> 6) & 0x7;
    // whether we are in imediate mode - left shift instr by 5 and mask LSB to get immediate flag
    uint16_t imm_flag = (instr >> 5) & 0x1;

    // in immediate mode, the second value is embedded in the right0most 5 bits of the instruction
    if (imm_flag)
    {
        // values which are shorter than 16 bits need to be sign extended
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] & imm5;
    }
    // in register mode, the second value is found in a register
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] & reg[r2];
    }

    // any time an instruction modifies a reigster, the condition flags need to be updated
    update_flags(r0);
}

/* NOT logic - take first operand, perform bitwise logical NOT, then store into destination register */
void NOT(uint16_t instr)
{
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0x7;
    // first operand (SR1)
    uint16_t r1 = (instr >> 6) & 0x7;

    reg[r0] = ~reg[r1];
    // any time an instruction modifies a reigster, the condition flags need to be updated
    update_flags(r0);
}

/* BR (Branch) logic - take offset and increment the program counter if any of the condition codes are set*/
void BR(uint16_t instr) 
{
    // find the offset by masking the 9 LSB and sign extend
    uint16_t pc_offset =  sign_extend(instr & 0x1FF, 9);
    // the condition code is found taking bits [11:9] and checking if they match with any of the condition flags
    uint16_t cond_flag = (instr >> 9) & 0x7;
    // if any of the condition codes are set (anything other than 000), branch to the offset
    if (cond_flag & reg[R_COND])
    {
        // add the offset to the program counter to branch to the next instruction
        reg[R_PC] += pc_offset;
    }
}

/* JMP (Jump) logic - set the program counter to value stored in register */
void JMP(uint16_t instr)
{
    // also handles RET functions
    // get bits [8:6]
    uint16_t r1 = (instr >> 6) & 0x7;
    // jump to the specified location given by bits [8:6]
    reg[R_PC] = reg[r1];
}

/* JSR/JSRR (Jump Register) logic - SAVES the program counter into a register (R7) and:
                                        either jumps to specified address given in register (JSR) 
                                        or 
                                        adds offset onto program counter and then jump (JSRR) */
void JSR(uint16_t instr) 
{
    // find the long flag from bit 11
    uint16_t long_flag = (instr >> 11) & 1;
    // the pc is saved and stored in register R7
    reg[R_R7] = reg[R_PC];
    // if the long flag is 1
    if (long_flag)
    {
        // find the offset from bits [10:0] and sign extend
        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
        // add the offset to the pc
        reg[R_PC] += long_pc_offset;        // JSR
    }
    // if the long flag is 0
    else
    {
        // find bits [8:6]
        uint16_t r1 = (instr >> 6) & 0x7;
        // jump to the address specified by r1 using the pc
        reg[R_PC] = reg[r1];       // JSRR
    }
}

/* LD (Load) logic - given an offset, add to program counter and store contents of address into destination register */
void LD(uint16_t instr)
{
    // destination register (DR) - get bits [11:9]
    uint16_t r0 = (instr >> 9) & 0x7;
    // mask 9 bits and sign extend to find the pc offset
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    // add the offset to the pc and load(read) the contents of the address to register R0
    reg[r0] = mem_read(reg[R_PC] + pc_offset);
    // udpate the flags based on register R0 because R0 changed value
    update_flags(r0);
}

/* 
    LD is limited to address offset of 9 bits which get added to the current PC. It is useful in loading values from neighboring addresses 
    LDI is useful to loading values in locations far away but the address to that value has to be stored in an address nearby to the current PC.
*/

/* LDI (Load Indirect) logic - add offset to program counter to find address which holds another address that holds the value needed and store value into destination register */
void LDI(uint16_t instr)
{
    // destination register (DR) - get bits [11:9]
    uint16_t r0 = (instr >> 9) & 0x7;
    // PCOffset - get bits [8:0] and sign extend to 16 bits
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    // add pc_offset to the current PC, look at that memory location to get the final address
    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
    // update flags after changing register value
    update_flags(r0);
}

/* LDR (Load Register) logic - add an offset to a given base register and store contents of incremeneted address into destination register */
void LDR(uint16_t instr) 
{
    // destination register (DR) - get bits [11:9]
    uint16_t r0 = (instr >> 9) & 0x7;
    // base register (baseR) - get bits [8:6]
    uint16_t r1 = (instr >> 6) & 0x7;
    // get 6 bit offset (bits [5:0]) and sign extend to 16 bits
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    // add the offset to the base register and read the contents of this address into r0
    reg[r0] = mem_read(reg[r1] + offset);
    // update flags after changing register value
    update_flags(r0);
}

/* LEA (Load Effective Address) logic - add an offset to program counter and store the address value into destination register */
void LEA(uint16_t instr)
{
    // destination register (DR) - get bits [11:9]
    uint16_t r0 = (instr >> 9) & 0x7;
    // PCOffset - get bits [8:0] and sign extend to 16 bits
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    // add offset to PC and store address into r0
    reg[r0] = reg[R_PC] + pc_offset;
    // update flags after changing register value
    update_flags(r0);
}

/* ST (Store) logic - add offset to program counter and store contents of given register into the address */
void ST(uint16_t instr)
{
    // source register (SR) - get bits [11:9]
    uint16_t r0 = (instr >> 9) & 0x7;
    // PCOffset - get bits [8:0] and sign extend to 16 bits
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    // add offset to PC and write the contents of r0 into the address with the offset
    mem_write(reg[R_PC] + pc_offset, reg[r0]);
}

/* STI (Store Indirect) logic - add offset to program counter to get an address whhich stores the destination address in which the contents of r0 will be written into */
void STI(uint16_t instr)
{
    // source register (SR) - get bits [11:9]
    uint16_t r0 = (instr >> 9) & 0x7;
    // PCOffset - get bits [8:0] and sign extend to 16 bits
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    // add offset to PC and read address with the offset to get destination address where the contents of r0 will be written into
    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
}   

/* STR (Store Register) logic -  store the contents of the first operand into the sum of the offset and the address stored in the second operand  */
void STR(uint16_t instr)
{
    // source register (SR) - get bits [11:9]
    uint16_t r0 = (instr >> 9) & 0x7;
    // base register (BaseR) - get bits [8:6]
    uint16_t r1 = (instr >> 6) & 0x7;
    // offset6 - get bits [5:0] and sign extend
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    // add immediate value offset to address stored in r1 and write the contents of r0 into that new address
    mem_write(reg[r1] + offset, reg[r0]);
}