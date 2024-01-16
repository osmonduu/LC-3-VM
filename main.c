#include <stdio.h>
#include <stdint.h>

#define MEMORY_MAX (1 << 16) //store 2^16 into MEMORY_MAX global variable by right shifting binary 1 16 times 
uint16_t memory[MEMORY_MAX]; //memory will be represented in an array of size 65536
