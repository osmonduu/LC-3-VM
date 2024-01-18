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
    FL_POS = 1 << 0,    // P = 1
    FL_ZRO = 1 << 1,    // Z = 2
    FL_NEG = 1 << 2     // N = 4
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
    // check if the sign bit is 1 (negative number)
    if ((x >> (bit_count - 1)) & 1) 
    {
        // sign extend with 1 to 16 bits
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

                break;
            case OP_LDR:

                break;
            case OP_LEA:

                break;
            case OP_ST:

                break;
            case OP_STI:

                break;
            case OP_STR:

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

/* AND logic */
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

/* NOT logic */
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

/* BR (branch) logic */
void BR(uint16_t instr) 
{
    uint16_t pc_offset =  sign_extend(instr & 0x1FF, 9);
    uint16_t cond_flag = (instr >> 9) & 0x7;
    if (cond_flag & reg[R_COND])
    {
        reg[R_PC] += pc_offset;
    }
}

/* JMP (jump) logic */
void JMP(uint16_t instr)
{
    // also handles RET
    uint16_t r1 = (instr >> 6) & 0x7;
    reg[R_PC] = reg[r1];
}

/* JSR (jump register) logic */
void JSR(uint16_t instr) 
{
    uint16_t long_flag = (instr >> 11) & 1;
    reg[R_R7] = reg[R_PC];
    if (long_flag)
    {
        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
        reg[R_PC] += long_pc_offset;        // JSR
    }
    else
    {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] += reg[r1];       // JSRR
    }
}

/* LD (load) logic */
void LD(uint16_t instr)
{
    // destination register (DR)
    uint16_t r0 = (instr >> 9) & 0x7;
    // mask 9 bits and sign extend to find the pc offset
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read(reg[R_PC] + pc_offset);
    update_flags(r0);
}

/* LDI (load indirect) */
void LDI(uint16_t instr)
{

}