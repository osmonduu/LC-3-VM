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

                break;
            case OP_AND:

                break;
            case OP_NOT:

                break;
            case OP_BR:

                break;
            case OP_JMP:

                break;
            case OP_JSR:

                break;
            case OP_LD:

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