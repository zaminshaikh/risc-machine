#ifndef _MACHINE_H
#define _MACHINE_H

#include "bof.h"
#include "instruction.h"
#include "machine_types.h"
#include "regname.h"
#include "utilities.h"

// Size of memory 
#define MEMORY_SIZE_IN_BYTES (65536 - BYTES_PER_WORD)
#define MEMORY_SIZE_IN_WORDS (MEMORY_SIZE_IN_BYTES / BYTES_PER_WORD)

// Create memory union so that we can access the memory by bytes or by words and store
// instructions and data in memory
static union mem_u {
    byte_type bytes[MEMORY_SIZE_IN_BYTES];
    word_type words[MEMORY_SIZE_IN_WORDS];
    bin_instr_t instrs[MEMORY_SIZE_IN_WORDS];
} memory;

// Function to load data from BOF file into memory
void load_data_section(BOFHeader bof_header, BOFFILE bof_file);

// Function to load instructions from BOF file into memory
void load_instruction_section(BOFHeader bof_header, BOFFILE bof_file);

// Prints the instructions in MIPS architecture to stdout
void print_instruction_section(BOFHeader bof_header);

// Prints the data section, formatted, to stdout
void print_data_section(BOFHeader bof_header);

// Initialize registers, program counter, and trace flag using BOFHeader data.
void set_registers(BOFHeader bof_header);

// Execute an instruction based on its type, handling various instruction categories.
void execute_instruction(bin_instr_t instruction);

// Execute a register-type instruction, performing arithmetic and logical operations.
void execute_reg_type_instr(reg_instr_t instruction);

// Execute a syscall-type instruction, handling various system call operations.
void execute_syscall_type_instr(syscall_instr_t instruction);

// Execute an immediate-type instruction, performing operations based on the instruction type.
void execute_immed_type_instr(immed_instr_t instruction);

// Execute a jump-type instruction, updating the program counter (PC) accordingly.
void execute_jump_type_instr(jump_instr_t instruction);

// Function to check for errors based on invariants
void error_check();

// Print the contents of program registers, including PC, HI, LO, and GPR.
void print_registers(BOFHeader bof_header);

// Helper function for print_registers
void print_stack(BOFHeader bof_header);

#endif
