#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bof.h"
#include "instruction.h"
#include "machine_types.h"
#include "regname.h"
#include "utilities.h"
#include "machine.h"

// Define an array for 32 general-purpose registers and variables for PC, HI, LO, and trace.
word_type GPR[32];
int PC, HI, LO;
int trace;

// Define the main function to execute the virtual machine.
int main(int argc, char **argv) {
    int index;
    bin_instr_t instruction;
    BOFFILE bof_file;
    BOFHeader bof_header;

    // Check for the correct number of command-line arguments.
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Missing arguments\n");
        exit(0);
    }

    // If the -p flag is provided, set the index accordingly.
    index = argc == 3 ? 2 : 1;

    // Open the BOF file and read its header.
    bof_file = bof_read_open(argv[index]);
    bof_header = bof_read_header(bof_file);

    // Load the instruction and data sections from the BOF file.
    load_instruction_section(bof_header, bof_file);
    load_data_section(bof_header, bof_file);

    // Set initial register values.
    set_registers(bof_header);

    // If the program is run with -p flag, print the assembly instructions and data sections.
    if (strcmp(argv[1], "-p") == 0) {
        print_instruction_section(bof_header);
        print_data_section(bof_header);
        return 0;
    }

    // Main execution loop for processing instructions.
    while (PC <= bof_header.text_length) {
        instruction = memory.instrs[PC / 4];

        // If the trace flag is set, print the current instruction and register values.
        if (trace) {
            print_registers(bof_header);
            printf("==> addr: %d %s\n", PC, instruction_assembly_form(instruction));
        }

        PC += BYTES_PER_WORD;
        execute_instruction(instruction);
        error_check();
    }

    return 0;
}


// Function to load data from BOF file
void load_data_section(BOFHeader bof_header, BOFFILE bof_file) {
    int i;

    // Read each word one by one into the memory union in the data section of memory.
    for (i = 0; i < bof_header.data_length / BYTES_PER_WORD; i++) {
        memory.words[(bof_header.data_start_address + i*4) / BYTES_PER_WORD] = bof_read_word(bof_file);
    }

}

// Function to read instructions from BOF file
void load_instruction_section(BOFHeader bof_header, BOFFILE bof_file) {
    int i;
    // Read each instruction one by one into the memory union in the instruction section
    // (which starts at index 0 of the array)
    for (i = 0; i < bof_header.text_length / BYTES_PER_WORD; i++) {
        memory.instrs[i] = instruction_read(bof_file);
    }

}

// Prints the instructions in MIPS architecture to stdout
void print_instruction_section(BOFHeader bof_header) {

    int i;

    // Print a table heading for the instruction output.
    instruction_print_table_heading(stdout);

    // Iterate through the instructions in the text section of memory.
    for (i = 0; i < bof_header.text_length / BYTES_PER_WORD; i++) {
        // Print the instruction's address (byte offset) and its assembly representation.
        printf("%4d %s", i * 4, instruction_assembly_form(memory.instrs[i]));
        newline(stdout);  // Print a newline to separate instructions.
    }
}

// Prints the data section, formatted, to stdout
void print_data_section(BOFHeader bof_header) 
{
    int i;

    // Check if there is no data or the first byte of data is zero.
    if (bof_header.data_length == 0 || memory.bytes[bof_header.data_start_address] == 0) {
        // If no data exists, print a message and return.
        printf("    %4d: 0    ...\n", bof_header.data_start_address);
        return;
    }

    printf("    ");  // Formatting: Start with an indentation.

    for (i = 0; i < bof_header.data_length / BYTES_PER_WORD; i++) {
        // Print the byte offset, data value, and format it accordingly.
        printf("%4d: %d    ", bof_header.data_start_address + i * 4, memory.words[(bof_header.data_start_address + i * 4) / BYTES_PER_WORD]);

        // Check for an ellipsis (...) condition.
        if (memory.words[(bof_header.data_start_address + i * 4) / BYTES_PER_WORD] == 0 &&
            memory.words[(bof_header.data_start_address + (i - 1) * 4)] == 0) {
            printf("...\n");
            return;
        }

        if (i % 5 == 4) {
            newline(stdout);  // Insert a newline after every 5 data entries.
            printf("    ");    // Indentation for the next line.
        }
    }

    printf("%4d: 0    ...", bof_header.data_start_address + i * 4);  // Print ellipsis for the last data entry.
    newline(stdout);
}

// Initialize registers, program counter, and trace flag using BOFHeader data.
void set_registers(BOFHeader bof_header) {

    for (int i = 0; i < 32; i++) 
        GPR[i] = 0;  // Initialize all general-purpose registers to zero.
    

    GPR[GP] = bof_header.data_start_address; // Set the global pointer (GP).
    GPR[FP] = GPR[SP] = bof_header.stack_bottom_addr;  // Set frame pointer (FP) and stack pointer (SP).

    PC = bof_header.text_start_address;  // Set the program counter (PC).
    trace = 1;  // Enable instruction tracing.

}

// Execute an instruction based on its type, handling various instruction categories.
void execute_instruction(bin_instr_t instruction) {
    // Determine the type of the instruction.
    instr_type type = instruction_type(instruction);
    
    switch (type) 
    {
        case reg_instr_type:
            execute_reg_type_instr(instruction.reg); // Execute a register-type instruction.
            break;
        case syscall_instr_type:
            execute_syscall_type_instr(instruction.syscall); // Execute a syscall-type instruction.
            break;
        case immed_instr_type:
            execute_immed_type_instr(instruction.immed); // Execute an immediate-type instruction.
            break;
        case jump_instr_type:
            execute_jump_type_instr(instruction.jump); // Execute a jump-type instruction.
            break;
        default:
            bail_with_error("Error reading instruction type"); // Handle an unknown instruction type.
            break;
    }
}

// Execute a register-type instruction, performing arithmetic and logical operations.
void execute_reg_type_instr(reg_instr_t instruction) {
    switch (instruction.func) {
        case ADD_F:
            // Add values of source registers and store the result in the destination register.
            GPR[instruction.rd] = GPR[instruction.rs] + GPR[instruction.rt];
            break;
        case SUB_F:
            // Subtract values of source registers and store the result in the destination register.
            GPR[instruction.rd] = GPR[instruction.rs] - GPR[instruction.rt];
            break;
        case MUL_F:
            // Multiply source registers using long long to handle overflow.
            long long int result = (long long)GPR[instruction.rs] * GPR[instruction.rt]; 
            // Extract and store the most significant and least significant bits.
            HI = (int)(result >> 32); // Most significant bits
            LO = (int)result;         // Least significant bits
            break;
        case DIV_F:
            // Divide source registers and store the quotient in LO and the remainder in HI.
            HI = GPR[instruction.rs] % GPR[instruction.rt];
            LO = GPR[instruction.rs] / GPR[instruction.rt];
            break;
        case MFHI_F:
            // Move the value of HI to the destination register.
            GPR[instruction.rd] = HI;
            break;
        case MFLO_F:
            // Move the value of LO to the destination register.
            GPR[instruction.rd] = LO;
            break;
        case AND_F:
            // Perform a bitwise AND operation on source registers and store the result.
            GPR[instruction.rd] = GPR[instruction.rs] & GPR[instruction.rt];
            break;
        case BOR_F:
            // Perform a bitwise OR operation on source registers and store the result.
            GPR[instruction.rd] = GPR[instruction.rs] | GPR[instruction.rt];
            break;
        case XOR_F:
            // Perform a bitwise XOR operation on source registers and store the result.
            GPR[instruction.rd] = GPR[instruction.rs] ^ GPR[instruction.rt];
            break;
        case NOR_F:
            // Perform a bitwise NOR operation on source registers and store the result.
            GPR[instruction.rd] = ~(GPR[instruction.rs] | GPR[instruction.rt]);
            break;
        case SLL_F:
            // Shift the value in the source register left by a specified number of bits.
            GPR[instruction.rd] = GPR[instruction.rt] << instruction.shift;
            break;
        case SRL_F:
            // Shift the value in the source register right by a specified number of bits.
            GPR[instruction.rd] = GPR[instruction.rt] >> instruction.shift;
            break;
        case JR_F:
            // Jump to the address in the source register.
            PC = GPR[instruction.rs];
            break;
    }
}

// Execute a syscall-type instruction, handling various system call operations.
void execute_syscall_type_instr(syscall_instr_t instruction) {
    switch (instruction.code) {
        case exit_sc:
            // Exit the program.
            exit(0);
            break;
        case print_str_sc:
            // Print a string from memory and store the result in GPR[2].
            GPR[2] = printf("%s", (char *) &memory.words[GPR[4]]);
            break;
        case print_char_sc:
            // Print a character and store the result in GPR[2].
            GPR[2] = fputc(GPR[4], stdout);
            break;
        case read_char_sc:
            // Read a character from stdin and store the result in GPR[2].
            GPR[2] = getc(stdin);
            break;
        case start_tracing_sc:
            // Enable instruction tracing.
            trace = 1;
            break;
        case stop_tracing_sc:
            // Disable instruction tracing.
            trace = 0;
            break;
    }
}

// Execute an immediate-type instruction, performing operations based on the instruction type.
void execute_immed_type_instr(immed_instr_t instruction) 
{
    switch (instruction.op) {
        case ADDI_O:
            // Add a sign-extended immediate value to the source register and store the result.
            GPR[instruction.rt] = GPR[instruction.rs] + machine_types_sgnExt(instruction.immed);
            break;
        case ANDI_O:
            // Perform a bitwise AND operation with a zero-extended immediate value and store the result.
            GPR[instruction.rt] = GPR[instruction.rs] & machine_types_zeroExt(instruction.immed);
            break;
        case BORI_O:
            // Perform a bitwise OR operation with a zero-extended immediate value and store the result.
            GPR[instruction.rt] = GPR[instruction.rs] | machine_types_zeroExt(instruction.immed);
            break;
        case XORI_O:
            // Perform a bitwise XOR operation with a zero-extended immediate value and store the result.
            GPR[instruction.rt] = GPR[instruction.rs] ^ machine_types_zeroExt(instruction.immed);
            break;
        case BEQ_O:
            // Branch if the values in two source registers are equal.
            if (GPR[instruction.rs] == GPR[instruction.rt]) {
                PC = PC + machine_types_formOffset(instruction.immed);
            }
            break;
        case BGEZ_O:
            // Branch if the value in a source register is greater than or equal to zero.
            if (GPR[instruction.rs] >= 0) {
                PC = PC + machine_types_formOffset(instruction.immed);
            }
            break;
        case BGTZ_O:
            // Branch if the value in a source register is greater than zero.
            if (GPR[instruction.rs] > 0) {
                PC = PC + machine_types_formOffset(instruction.immed);
            }
            break;
        case BLEZ_O:
            // Branch if the value in a source register is less than or equal to zero.
            if (GPR[instruction.rs] <= 0) {
                PC = PC + machine_types_formOffset(instruction.immed);
            }
            break;
        case BLTZ_O:
            // Branch if the value in a source register is less than zero.
            if (GPR[instruction.rs] < 0) {
                PC = PC + machine_types_formOffset(instruction.immed);
            }
            break;
        case BNE_O:
            // Branch if the values in two source registers are not equal.
            if (GPR[instruction.rs] != GPR[instruction.rt]) {
                PC = PC + machine_types_formOffset(instruction.immed);
            }
            break;
        case LBU_O:
            // Load a byte from memory, zero-extend it, and store it in the destination register.
            GPR[instruction.rt] = machine_types_zeroExt(memory.bytes[GPR[instruction.rs] + machine_types_formOffset(instruction.immed)]);
            break;
        case LW_O:
            // Load a word from memory and store it in the destination register.
            GPR[instruction.rt] = memory.words[(GPR[instruction.rs] + machine_types_formOffset(instruction.immed)) / BYTES_PER_WORD];
            break;
        case SB_O:
            // Store a byte from the source register into memory.
            memory.bytes[GPR[instruction.rs] + machine_types_formOffset(instruction.immed)] = GPR[instruction.rt];
            break;
        case SW_O:
            // Store a word from the source register into memory.
            memory.words[(GPR[instruction.rs] + machine_types_formOffset(instruction.immed)) / BYTES_PER_WORD] = GPR[instruction.rt];
            break;
    }
}

// Execute a jump-type instruction, updating the program counter (PC) accordingly.
void execute_jump_type_instr(jump_instr_t instruction) 
{
    switch (instruction.op) {
        case 2:
            // Execute an unconditional jump by updating the PC based on the address in the instruction.
            PC = machine_types_formAddress(PC, instruction.addr);
            break;
        case 3:
            // Execute a jump and link (jal) operation, saving the return address in GPR[RA].
            GPR[RA] = PC;
            PC = machine_types_formAddress(PC, instruction.addr);
            break;
    }
}

// Function to check for errors based on invariants
void error_check() 
{
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


// Print the contents of program registers, including PC, HI, LO, and GPR.
void print_registers(BOFHeader bof_header) 
{
    // Check if the HI and LO registers are non-zero, and print them along with PC.
    if (HI != 0 || LO != 0)
        printf("      PC: %d       HI: %d       LO: %d\n", PC, HI, LO);
    else
        printf("      PC: %d\n", PC); // Print PC

    // Print the General Purpose Registers (GPR) with their values.
    for (int i = 0; i < 32; i++) 
    {
        printf("GPR[%-3s]: %-4d    ", regname_get(i), GPR[i]);
        
        // Print a newline after every 6 registers for better formatting.
        if (i % 6 == 5)
            newline(stdout);
    }

    newline(stdout);

    // Print the data section.
    print_data_section(bof_header);

    // Print the stack section.
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
