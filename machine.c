#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bof.h"
#include "instruction.h"
#include "machine_types.h"
#include "regname.h"
#include "utilities.h"

// gcc -o vm machine.c bof.c instruction.c machine_types.c regname.c utilities.c
// ./vm -p vm_test0.bof
// ./vm vm_test0.bof


#define MEMORY_SIZE_IN_BYTES (65536 - BYTES_PER_WORD)
#define MEMORY_SIZE_IN_WORDS (MEMORY_SIZE_IN_BYTES / BYTES_PER_WORD)

// Create memory data structure (From example in Manual)
static union mem_u {
    byte_type bytes[MEMORY_SIZE_IN_BYTES];
    word_type words[MEMORY_SIZE_IN_WORDS];
    bin_instr_t instrs[MEMORY_SIZE_IN_WORDS];
} memory;

word_type GPR[32];
int PC, HI, LO;
int trace;

void execute_jump_type_instr(jump_instr_t instruction);
void execute_immed_type_instr(immed_instr_t instruction);
void execute_syscall_type_instr(syscall_instr_t instruction);
void execute_reg_type_instr(reg_instr_t instruction);
void execute_instruction(bin_instr_t instruction);
void error_check();
void set_registers(BOFHeader bof_header);
void load_data_section(BOFHeader bof_header, BOFFILE bof_file);
void load_instruction_section(BOFHeader bof_header, BOFFILE bof_file);
void print_instruction_section(BOFHeader bof_header);
void print_data_section(BOFHeader bof_header);
void print_registers(BOFHeader bof_header);
void print_stack(BOFHeader bof_header);


int main(int argc, char **argv) {
    int index;
    bin_instr_t instruction;
    BOFFILE bof_file;
    BOFHeader bof_header;

    // Check for correct number of arguments
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Missing arguments\n");
        exit(0);
    }

    // If we have a -p flag the index is 2
    index = argc == 3 ? 2 : 1;

    // Open the BOF file
    bof_file = bof_read_open(argv[index]);
    // Read the BOF header
    bof_header = bof_read_header(bof_file);
    // Load the instruction section
    load_instruction_section(bof_header, bof_file);
    // printf("%d\n", bof_header.text_length);
    // Load the data section
    load_data_section(bof_header, bof_file);
    
    set_registers(bof_header);

    // Print assembly language form of the program
    if (strcmp(argv[1], "-p") == 0) {
        print_instruction_section(bof_header);
        print_data_section(bof_header);
        return 0;
    }
    

    // printf("%d\n", memory.words[513]);

    // Main execution loop
    while (PC <= bof_header.text_length)
    {
        // Load the instruction
        instruction = memory.instrs[PC/4];
        // If trace flag is set print the instruction
        if (trace )
        {
        print_registers(bof_header);
        printf("==> addr:    %d %s\n", PC, instruction_assembly_form(instruction));
        }
        PC += BYTES_PER_WORD; // Increment the program counter
        execute_instruction(instruction); // Execute instruction
        error_check(); // Check for errors
    }


    return 0;
}


// Function to load data from BOF file
void load_data_section(BOFHeader bof_header, BOFFILE bof_file) {
int i;
for (i = 0; i < bof_header.data_length / BYTES_PER_WORD; i++) {
    memory.words[(bof_header.data_start_address + i*4) / BYTES_PER_WORD] = bof_read_word(bof_file);
}

}

// Function to read instructions from BOF file
void load_instruction_section(BOFHeader bof_header, BOFFILE bof_file) {
int i;
for (i = 0; i < bof_header.text_length / BYTES_PER_WORD; i++) {
    memory.instrs[i] = instruction_read(bof_file);
}

}

// Funciton to print instruction in stdout
void print_instruction_section(BOFHeader bof_header) {
int i;

instruction_print_table_heading(stdout);

for (i = 0; i < bof_header.text_length / BYTES_PER_WORD; i++) {
    printf("%4d %s", i*4, instruction_assembly_form(memory.instrs[i]));
    newline(stdout);
}
}

// Function to print data in stdout
void print_data_section(BOFHeader bof_header) 
{
    int i;

    if (bof_header.data_length == 0 || memory.bytes[bof_header.data_start_address] == 0)
    //if (bof_header.data_length == 0 || memory.words[bof_header.data_start_address] == 0)
    {
      printf("    %4d: 0	...\n", bof_header.data_start_address);
      return;
    }
    printf("    ");

    for (i = 0; i < bof_header.data_length / BYTES_PER_WORD; i++) {
        printf("%4d: %d	    ", bof_header.data_start_address + i*4, memory.words[(bof_header.data_start_address + i*4)/ BYTES_PER_WORD]);
        //printf("%4d: %d	    ", bof_header.data_start_address + i*4, memory.words[bof_header.data_start_address + i]);
        if (memory.words[(bof_header.data_start_address + i*4)/ BYTES_PER_WORD] == 0 && memory.words[(bof_header.data_start_address + (i-1) * 4)] == 0)
        {
            printf("...\n");
            return;
        } 
        
        if (i % 5 == 4){
          newline(stdout);
          printf("    ");
        }
    }

    printf("%4d: 0	     ...", bof_header.data_start_address + i*4);
    newline(stdout);
}

// Function to set the initial value of registers
void set_registers(BOFHeader bof_header) {
    for (int i = 0; i < 32; i++) {
        GPR[i] = 0;
    }

    GPR[GP] = bof_header.data_start_address;
    GPR[FP] = GPR[SP] = bof_header.stack_bottom_addr;

    PC = bof_header.text_start_address;
    trace = 1;
}

// Function to execute an instruction based on its type
void execute_instruction(bin_instr_t instruction) {
    instr_type type = instruction_type(instruction);
    switch (type) 
    {
        case reg_instr_type:
        execute_reg_type_instr(instruction.reg);
        break;
        case syscall_instr_type:
        execute_syscall_type_instr(instruction.syscall);
        break;
        case immed_instr_type:
        execute_immed_type_instr(instruction.immed);
        break;
        case jump_instr_type:
        execute_jump_type_instr(instruction.jump);
        break;
        default:
        bail_with_error("Error reading instruction type");
        break;
    }
}

// Function to execute register-type instruction
void execute_reg_type_instr(reg_instr_t instruction) {
    switch (instruction.func) {
    case ADD_F:
        GPR[instruction.rd] = GPR[instruction.rs] + GPR[instruction.rt];
        break;
    case SUB_F:
        GPR[instruction.rd] = GPR[instruction.rs] - GPR[instruction.rt];
        break;
    case MUL_F:
        long long int result = (long long)GPR[instruction.rs] *
                            GPR[instruction.rt]; 
        // Perform multiplication using
        // long long to handle overflow
        // Extract and store the most significant and least significant bits
        HI = (int)(result >> 32); // Most significant bits
        LO = (int)result;         // Least significant bits
        break;
    case DIV_F:
        HI = GPR[instruction.rs] % GPR[instruction.rt];
        LO = GPR[instruction.rs] / GPR[instruction.rt];
        break;
    case MFHI_F:
        GPR[instruction.rd] = HI;
        break;
    case MFLO_F:
        GPR[instruction.rd] = LO;
        break;
    case AND_F:
        GPR[instruction.rd] = GPR[instruction.rs] & GPR[instruction.rt];
        break;
    case BOR_F:
        GPR[instruction.rd] = GPR[instruction.rs] | GPR[instruction.rt];
        break;
    case XOR_F:
        GPR[instruction.rd] = GPR[instruction.rs] ^ GPR[instruction.rt];
        break;
    case NOR_F:
        GPR[instruction.rd] = ~(GPR[instruction.rs] | GPR[instruction.rt]);
        break;
    case SLL_F:
        GPR[instruction.rd] = GPR[instruction.rt] << instruction.shift;
        break;
    case SRL_F:
        GPR[instruction.rd] = GPR[instruction.rt] >> instruction.shift;
        break;
    case JR_F:
        PC = GPR[instruction.rs];
        break;
    }
    
}

// Function to execute syscall-type instruction
void execute_syscall_type_instr(syscall_instr_t instruction) {
    switch (instruction.code) {
        case exit_sc:
            exit(0);
            break;
        case print_str_sc:
            GPR[2] = printf("%s", (char *) &memory.words[GPR[4]]);
            break;
        case print_char_sc:
            GPR[2] = fputc(GPR[4], stdout);
            break;
        case read_char_sc:
            GPR[2] = getc(stdin);
            break;
        case start_tracing_sc:
            trace = 1;
            break;
        case stop_tracing_sc:
            trace = 0;
            break;
    }
}

// Function to execute an immediate-type instruction
void execute_immed_type_instr(immed_instr_t instruction) {
  switch (instruction.op) {
    case ADDI_O:
        GPR[instruction.rt] = GPR[instruction.rs] + machine_types_sgnExt(instruction.immed);
        break;
    case ANDI_O:
        GPR[instruction.rt] = GPR[instruction.rs] & machine_types_zeroExt(instruction.immed);
        break;
    case BORI_O:
        GPR[instruction.rt] = GPR[instruction.rs] | machine_types_zeroExt(instruction.immed);
        break;
    case XORI_O:
        GPR[instruction.rt] = GPR[instruction.rs] ^ machine_types_zeroExt(instruction.immed);
        break;
    case BEQ_O:
        if (GPR[instruction.rs] == GPR[instruction.rt]) {
            PC = PC + machine_types_formOffset(instruction.immed);
        }
        break;
    case BGEZ_O:
        if (GPR[instruction.rs] >= 0) {
            PC = PC + machine_types_formOffset(instruction.immed);
        }
        break;
    case BGTZ_O:
        if (GPR[instruction.rs] > 0) {
        PC = PC + machine_types_formOffset(instruction.immed);
        }
        break;
    case BLEZ_O:
        if (GPR[instruction.rs] <= 0) {
        PC = PC + machine_types_formOffset(instruction.immed);
        }
        break;
    case BLTZ_O:
        if (GPR[instruction.rs] < 0) {
        PC = PC + machine_types_formOffset(instruction.immed);
        }
        break;
    case BNE_O:
        if (GPR[instruction.rs] != GPR[instruction.rt]) {
            PC = PC + machine_types_formOffset(instruction.immed);
        }
        break;
    case LBU_O:
        GPR[instruction.rt] = machine_types_zeroExt(memory.bytes[GPR[instruction.rs] + machine_types_formOffset(instruction.immed)]);
        break;
    case LW_O:
        GPR[instruction.rt] = memory.words[(GPR[instruction.rs] + machine_types_formOffset(instruction.immed)) / BYTES_PER_WORD];
        break;
    case SB_O:
        memory.bytes[GPR[instruction.rs] + machine_types_formOffset(instruction.immed)] = GPR[instruction.rt];
        break;
    case SW_O:
        memory.words[(GPR[instruction.rs] + machine_types_formOffset(instruction.immed)) / BYTES_PER_WORD] =   GPR[instruction.rt];
        break;
  }
}

// Function to execute a jump-type instruction
void execute_jump_type_instr(jump_instr_t instruction) {
    switch (instruction.op) {
        case 2:
            PC = machine_types_formAddress(PC, instruction.addr);;
            break;
        case 3:
            GPR[RA] = PC;
            PC = machine_types_formAddress(PC, instruction.addr);
            break;
    }
  
}

// Function to check for errors based on invariants
void error_check() {
    if (PC % BYTES_PER_WORD != 0) 
        bail_with_error("Invariant broken: PC % BYTES_PER_WORD = 0");

    else if (GPR[GP] % BYTES_PER_WORD != 0) 
        bail_with_error("Invariant broken: GPR[GP] % BYTES_PER_WORD = 0");

    else if (GPR[SP] % BYTES_PER_WORD != 0) 
        bail_with_error("Invariant broken: GPR[SP] % BYTES_PER_WORD = 0");

    else if (GPR[FP] % BYTES_PER_WORD != 0) 
        bail_with_error("Invariant broken: GPR[FP] % BYTES_PER_WORD = 0");

    else if (0 > GPR[GP]) 
        bail_with_error("Invariant broken: 0 < GPR[GP]");

    else if (GPR[GP] >= GPR[SP]) 
        bail_with_error("Invariant broken: GPR[GP] < GPR[SP]");

    else if (GPR[SP] > GPR[FP]) 
        bail_with_error("Invariant broken: GPR[SP] <= GPR[FP]");

    else if (GPR[FP] >= MEMORY_SIZE_IN_BYTES) 
        bail_with_error("Invariant broken: GPR[FP] < MEMORY_SIZE_IN_BYTES");

    else if (0 > PC) 
        bail_with_error("Invariant broken: 0 <= PC");

    else if (PC >= MEMORY_SIZE_IN_BYTES) 
        bail_with_error("Invariant broken: PC < MEMORY_SIZE_IN_BYTES");

    else if (GPR[0] != 0) 
        bail_with_error("Invariant broken: GPR[0] = 0");

}

// Function to print the registers, data, and stack
void print_registers(BOFHeader bof_header) 
{
    if (HI != 0 || LO!= 0)
        printf("      PC: %d	      HI: %d	      LO: %d\n", PC, HI, LO); // print PC
    else
        printf("      PC: %d\n", PC); // print PC


    for (int i = 0; i < 32; i++) 
    {
        printf("GPR[%-3s]: %-4d   	", regname_get(i), GPR[i]);
        if (i % 6 == 5)
            newline(stdout);
    }
    newline(stdout);
    print_data_section(bof_header);
    print_stack(bof_header);
}

// Helper function for print_registers
void print_stack(BOFHeader bof_header) 
{
    int i, k = 1; // index of addresses in a line
    printf("    "); // formatting
    // If no stack is used
    if (GPR[SP] == GPR[FP])
    {
        printf("%d: 0	...", GPR[SP]);
        newline(stdout);
        return;
    }
    // print the stack
    for (i = GPR[SP]; i < GPR[FP]; i += BYTES_PER_WORD)
    {
        // If there are back to back 0s
        if (memory.bytes[i-BYTES_PER_WORD] == 0 && memory.bytes[i] == 0 && i != GPR[SP])
            continue;
        // If only the current value is 0
        else if (memory.bytes[i] == 0)
        {
            printf("%d: 0	...    ", i);

            // Format 5 per line
            if (k == 5)
            {
                printf("\n    ");
                k = 1;
            } else k++;
            continue;
        }

        printf("%d: %d	    ", i, memory.bytes[i]);

        // Format 5 per line
        if (k == 5)
        {
            printf("\n    ");
            k = 1;
        } else k++;
    }

    if (memory.bytes[i-BYTES_PER_WORD] != 0)
        printf("%d: 0	...", i);

    newline(stdout);
}
