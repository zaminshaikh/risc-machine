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

  // if we have a -p flag the index is 2
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


  if (strcmp(argv[1], "-p") == 0) {
    print_instruction_section(bof_header);
    print_data_section(bof_header);
    return 0;
  }
  
  set_registers(bof_header);

  while (PC <= bof_header.text_length)
  {
    instruction = memory.instrs[PC/4];
    if (trace)
    {
      print_registers(bof_header);
      printf("==> addr:   %d %s\n", PC, instruction_assembly_form(instruction));
    }
    execute_instruction(instruction);
    PC += BYTES_PER_WORD;
  }
    newline(stdout);


  return 0;
}

void load_data_section(BOFHeader bof_header, BOFFILE bof_file) {
  int i;
  for (i = 0; i < bof_header.data_length / BYTES_PER_WORD; i++) {
    memory.words[bof_header.data_start_address + i] = bof_read_word(bof_file);
  }
}
void load_instruction_section(BOFHeader bof_header, BOFFILE bof_file) {
  int i;
  for (i = 0; i < bof_header.text_length / BYTES_PER_WORD; i++) {
    memory.instrs[i] = instruction_read(bof_file);
  }

}

void print_instruction_section(BOFHeader bof_header) {
  int i;

  instruction_print_table_heading(stdout);
  
  for (i = 0; i < bof_header.text_length / BYTES_PER_WORD; i++) {
    printf("%4d %s", i*4, instruction_assembly_form(memory.instrs[i]));
    newline(stdout);
  }
}

void print_data_section(BOFHeader bof_header) 
{
    int i;

    if (bof_header.data_length == 0 || memory.words[bof_header.data_start_address] == 0)
    {
      printf("    %4d: 0	     ...\n", bof_header.data_start_address);
      return;
    }
    printf("    ");
    for (i = 0; i < bof_header.data_length / BYTES_PER_WORD; i++) {
        printf("%4d: %d	     ", bof_header.data_start_address + i*4, memory.words[bof_header.data_start_address + i]);
        if (i % 5 == 4){
          newline(stdout);
          printf("    ");
        }
    }
    printf("%4d: 0	     ...", bof_header.data_start_address + i*4);
    newline(stdout);
}

void set_registers(BOFHeader bof_header) {
  for (int i = 0; i < 32; i++) {
    GPR[i] = 0;
  }

  GPR[GP] = bof_header.data_start_address;
  GPR[FP] = GPR[SP] = bof_header.stack_bottom_addr;

  PC = bof_header.text_start_address;
  trace = 1;
}

void execute_instruction(bin_instr_t instruction) {
  instr_type type = instruction_type(instruction);
  switch (type) {
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
          newline(stdout);
}

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
    break;
  case MFHI_F:
    GPR[instruction.rd] = HI;
    break;
  case MFLO_F:
    GPR[instruction.rd] = LO;
    break;
  case AND_F:
    GPR[instruction.rd] = GPR[instruction.rs] && GPR[instruction.rt];
    break;
  case BOR_F:
    GPR[instruction.rd] = GPR[instruction.rs] || GPR[instruction.rt];
    break;
  case XOR_F:
    GPR[instruction.rd] = GPR[instruction.rs] ^ GPR[instruction.rt];
    break;
  case NOR_F:
    GPR[instruction.rd] = ~(GPR[instruction.rs] | GPR[instruction.rt]);
    break;
  case SLL_F:
    GPR[instruction.rd] = GPR[instruction.rt] << GPR[instruction.shift];
    break;
  case SRL_F:
    GPR[instruction.rd] = GPR[instruction.rt] >> GPR[instruction.shift];
    break;
  case JR_F:
    PC = GPR[instruction.rs];
    break;
  }
  
}

void execute_syscall_type_instr(syscall_instr_t instruction) {
  switch (instruction.code) {
    case exit_sc:
      exit(0);
    case print_str_sc:
      GPR[2] = printf("%s", (char *) &memory.words[GPR[4]]);
    case print_char_sc:
      GPR[2] = fputc(GPR[4], stdout);
    case read_char_sc:
      GPR[2] = getc(stdin);
    case start_tracing_sc:
      trace = 1;
      break;
    case stop_tracing_sc:
      trace = 0;
      break;
  }
}

void execute_immed_type_instr(immed_instr_t instruction) {
  switch (instruction.op) {
  case ADDI_O:
    GPR[instruction.rt] =
        GPR[instruction.rs] + machine_types_sgnExt(instruction.immed);
    break;
  case ANDI_O:
    GPR[instruction.rt] =
        GPR[instruction.rs] & machine_types_zeroExt(instruction.immed);
    break;
  case BORI_O:
    GPR[instruction.rt] =
        GPR[instruction.rs] | machine_types_zeroExt(instruction.immed);
    break;
  case XORI_O:
    GPR[instruction.rt] =
        GPR[instruction.rs] ^ machine_types_zeroExt(instruction.immed);
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
    if (GPR[instruction.rs] != 0) {
      PC = PC + machine_types_formOffset(instruction.immed);
    }
    break;
  case LBU_O:
    GPR[instruction.rt] = machine_types_zeroExt(
        memory.bytes[GPR[instruction.rs] +
                     machine_types_formOffset(instruction.immed)]);
    break;
  case LW_O:
    GPR[instruction.rt] =
        memory.words[GPR[instruction.rs] +
                     machine_types_formOffset(instruction.immed)];
    break;
  case SB_O:
    memory.bytes[GPR[instruction.rs] +
                 machine_types_formOffset(instruction.immed)] =
        GPR[instruction.rt];
    break;
  case SW_O:
    memory.words[GPR[instruction.rs] +
                 machine_types_formOffset(instruction.immed)] =
        GPR[instruction.rt];
    break;
  }
}

void execute_jump_type_instr(jump_instr_t instruction) {
  switch (instruction.op) {
  case 2:
    PC = GPR[instruction.addr];
  case 3:
    GPR[31] = PC;
    PC = machine_types_formAddress(PC, instruction.addr);
  }
  
}

// Checks whether the invariants have been broken
// if so, bail with an error
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

// Prints the registers, data, and stack
void print_registers(BOFHeader bof_header) 
{
    printf("      PC: %d\n", PC); // print PC


    for (int i = 0; i < 32; i++) 
    {
        printf("GPR[%-3s]: %-5d", regname_get(i), GPR[i]);
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
    int k = 1;
    int i; // index of addresses in a line
    printf("    "); // formatting
    // if no stack is used
    if (GPR[SP] == GPR[FP])
        printf("%d: 0	...", GPR[SP]);
    // print the stack
    for (i = GPR[SP]; i < GPR[FP]; i+=4)
    {
        printf("%d: %d	    ", i, memory.bytes[i]);
        // 5 per line
        if (k == 5)
        {
            printf("\n    ");
            k = 1;
        }
    }
    printf("%d: 0	...", GPR[SP]);

    newline(stdout);
}
#line 1 "asm_lexer.c"

#line 3 "asm_lexer.c"

#define  YY_INT_ALIGNED short int

/* A lexical scanner generated by flex */

#define FLEX_SCANNER
#define YY_FLEX_MAJOR_VERSION 2
#define YY_FLEX_MINOR_VERSION 6
#define YY_FLEX_SUBMINOR_VERSION 4
#if YY_FLEX_SUBMINOR_VERSION > 0
#define FLEX_BETA
#endif

#ifdef yyget_lval
#define yyget_lval_ALREADY_DEFINED
#else
#define yyget_lval yyget_lval
#endif

#ifdef yyset_lval
#define yyset_lval_ALREADY_DEFINED
#else
#define yyset_lval yyset_lval
#endif

/* First, we deal with  platform-specific or compiler-specific issues. */

/* begin standard C headers. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

/* end standard C headers. */

/* flex integer type definitions */

#ifndef FLEXINT_H
#define FLEXINT_H

/* C99 systems have <inttypes.h>. Non-C99 systems may or may not. */

#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

/* C99 says to define __STDC_LIMIT_MACROS before including stdint.h,
 * if you want the limit (max/min) macros for int types. 
 */
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <inttypes.h>
typedef int8_t flex_int8_t;
typedef uint8_t flex_uint8_t;
typedef int16_t flex_int16_t;
typedef uint16_t flex_uint16_t;
typedef int32_t flex_int32_t;
typedef uint32_t flex_uint32_t;
#else
typedef signed char flex_int8_t;
typedef short int flex_int16_t;
typedef int flex_int32_t;
typedef unsigned char flex_uint8_t; 
typedef unsigned short int flex_uint16_t;
typedef unsigned int flex_uint32_t;

/* Limits of integral types. */
#ifndef INT8_MIN
#define INT8_MIN               (-128)
#endif
#ifndef INT16_MIN
#define INT16_MIN              (-32767-1)
#endif
#ifndef INT32_MIN
#define INT32_MIN              (-2147483647-1)
#endif
#ifndef INT8_MAX
#define INT8_MAX               (127)
#endif
#ifndef INT16_MAX
#define INT16_MAX              (32767)
#endif
#ifndef INT32_MAX
#define INT32_MAX              (2147483647)
#endif
#ifndef UINT8_MAX
#define UINT8_MAX              (255U)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX             (65535U)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX             (4294967295U)
#endif

#ifndef SIZE_MAX
#define SIZE_MAX               (~(size_t)0)
#endif

#endif /* ! C99 */

#endif /* ! FLEXINT_H */

/* begin standard C++ headers. */

/* TODO: this is always defined, so inline it */
#define yyconst const

#if defined(__GNUC__) && __GNUC__ >= 3
#define yynoreturn __attribute__((__noreturn__))
#else
#define yynoreturn
#endif

/* Returned upon end-of-file. */
#define YY_NULL 0

/* Promotes a possibly negative, possibly signed char to an
 *   integer in range [0..255] for use as an array index.
 */
#define YY_SC_TO_UI(c) ((YY_CHAR) (c))

/* Enter a start condition.  This macro really ought to take a parameter,
 * but we do it the disgusting crufty way forced on us by the ()-less
 * definition of BEGIN.
 */
#define BEGIN (yy_start) = 1 + 2 *
/* Translate the current start state into a value that can be later handed
 * to BEGIN to return to the state.  The YYSTATE alias is for lex
 * compatibility.
 */
#define YY_START (((yy_start) - 1) / 2)
#define YYSTATE YY_START
/* Action number for EOF rule of a given start state. */
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)
/* Special action meaning "start processing a new file". */
#define YY_NEW_FILE yyrestart( yyin  )
#define YY_END_OF_BUFFER_CHAR 0

/* Size of default input buffer. */
#ifndef YY_BUF_SIZE
#ifdef __ia64__
/* On IA-64, the buffer size is 16k, not 8k.
 * Moreover, YY_BUF_SIZE is 2*YY_READ_BUF_SIZE in the general case.
 * Ditto for the __ia64__ case accordingly.
 */
#define YY_BUF_SIZE 32768
#else
#define YY_BUF_SIZE 16384
#endif /* __ia64__ */
#endif

/* The state buf must be large enough to hold one state per character in the main buffer.
 */
#define YY_STATE_BUF_SIZE   ((YY_BUF_SIZE + 2) * sizeof(yy_state_type))

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef size_t yy_size_t;
#endif

extern int yyleng;

extern FILE *yyin, *yyout;

#define EOB_ACT_CONTINUE_SCAN 0
#define EOB_ACT_END_OF_FILE 1
#define EOB_ACT_LAST_MATCH 2
    
    /* Note: We specifically omit the test for yy_rule_can_match_eol because it requires
     *       access to the local variable yy_act. Since yyless() is a macro, it would break
     *       existing scanners that call yyless() from OUTSIDE yylex.
     *       One obvious solution it to make yy_act a global. I tried that, and saw
     *       a 5% performance hit in a non-yylineno scanner, because yy_act is
     *       normally declared as a register variable-- so it is not worth it.
     */
    #define  YY_LESS_LINENO(n) \
            do { \
                int yyl;\
                for ( yyl = n; yyl < yyleng; ++yyl )\
                    if ( yytext[yyl] == '\n' )\
                        --yylineno;\
            }while(0)
    #define YY_LINENO_REWIND_TO(dst) \
            do {\
                const char *p;\
                for ( p = yy_cp-1; p >= (dst); --p)\
                    if ( *p == '\n' )\
                        --yylineno;\
            }while(0)
    
/* Return all but the first "n" matched characters back to the input stream. */
#define yyless(n) \
	do \
		{ \
		/* Undo effects of setting up yytext. */ \
        int yyless_macro_arg = (n); \
        YY_LESS_LINENO(yyless_macro_arg);\
		*yy_cp = (yy_hold_char); \
		YY_RESTORE_YY_MORE_OFFSET \
		(yy_c_buf_p) = yy_cp = yy_bp + yyless_macro_arg - YY_MORE_ADJ; \
		YY_DO_BEFORE_ACTION; /* set up yytext again */ \
		} \
	while ( 0 )
#define unput(c) yyunput( c, (yytext_ptr)  )

#ifndef YY_STRUCT_YY_BUFFER_STATE
#define YY_STRUCT_YY_BUFFER_STATE
struct yy_buffer_state
	{
	FILE *yy_input_file;

	char *yy_ch_buf;		/* input buffer */
	char *yy_buf_pos;		/* current position in input buffer */

	/* Size of input buffer in bytes, not including room for EOB
	 * characters.
	 */
	int yy_buf_size;

	/* Number of characters read into yy_ch_buf, not including EOB
	 * characters.
	 */
	int yy_n_chars;

	/* Whether we "own" the buffer - i.e., we know we created it,
	 * and can realloc() it to grow it, and should free() it to
	 * delete it.
	 */
	int yy_is_our_buffer;

	/* Whether this is an "interactive" input source; if so, and
	 * if we're using stdio for input, then we want to use getc()
	 * instead of fread(), to make sure we stop fetching input after
	 * each newline.
	 */
	int yy_is_interactive;

	/* Whether we're considered to be at the beginning of a line.
	 * If so, '^' rules will be active on the next match, otherwise
	 * not.
	 */
	int yy_at_bol;

    int yy_bs_lineno; /**< The line count. */
    int yy_bs_column; /**< The column count. */

	/* Whether to try to fill the input buffer when we reach the
	 * end of it.
	 */
	int yy_fill_buffer;

	int yy_buffer_status;

#define YY_BUFFER_NEW 0
#define YY_BUFFER_NORMAL 1
	/* When an EOF's been seen but there's still some text to process
	 * then we mark the buffer as YY_EOF_PENDING, to indicate that we
	 * shouldn't try reading from the input source any more.  We might
	 * still have a bunch of tokens to match, though, because of
	 * possible backing-up.
	 *
	 * When we actually see the EOF, we change the status to "new"
	 * (via yyrestart()), so that the user can continue scanning by
	 * just pointing yyin at a new input file.
	 */
#define YY_BUFFER_EOF_PENDING 2

	};
#endif /* !YY_STRUCT_YY_BUFFER_STATE */

/* Stack of input buffers. */
static size_t yy_buffer_stack_top = 0; /**< index of top of stack. */
static size_t yy_buffer_stack_max = 0; /**< capacity of stack. */
static YY_BUFFER_STATE * yy_buffer_stack = NULL; /**< Stack as an array. */

/* We provide macros for accessing buffer states in case in the
 * future we want to put the buffer states in a more general
 * "scanner state".
 *
 * Returns the top of the stack, or NULL.
 */
#define YY_CURRENT_BUFFER ( (yy_buffer_stack) \
                          ? (yy_buffer_stack)[(yy_buffer_stack_top)] \
                          : NULL)
/* Same as previous macro, but useful when we know that the buffer stack is not
 * NULL or when we need an lvalue. For internal use only.
 */
#define YY_CURRENT_BUFFER_LVALUE (yy_buffer_stack)[(yy_buffer_stack_top)]

/* yy_hold_char holds the character lost when yytext is formed. */
static char yy_hold_char;
static int yy_n_chars;		/* number of characters read into yy_ch_buf */
int yyleng;

/* Points to current character in buffer. */
static char *yy_c_buf_p = NULL;
static int yy_init = 0;		/* whether we need to initialize */
static int yy_start = 0;	/* start state number */

/* Flag which is used to allow yywrap()'s to do buffer switches
 * instead of setting up a fresh yyin.  A bit of a hack ...
 */
static int yy_did_buffer_switch_on_eof;

void yyrestart ( FILE *input_file  );
void yy_switch_to_buffer ( YY_BUFFER_STATE new_buffer  );
YY_BUFFER_STATE yy_create_buffer ( FILE *file, int size  );
void yy_delete_buffer ( YY_BUFFER_STATE b  );
void yy_flush_buffer ( YY_BUFFER_STATE b  );
void yypush_buffer_state ( YY_BUFFER_STATE new_buffer  );
void yypop_buffer_state ( void );

static void yyensure_buffer_stack ( void );
static void yy_load_buffer_state ( void );
static void yy_init_buffer ( YY_BUFFER_STATE b, FILE *file  );
#define YY_FLUSH_BUFFER yy_flush_buffer( YY_CURRENT_BUFFER )

YY_BUFFER_STATE yy_scan_buffer ( char *base, yy_size_t size  );
YY_BUFFER_STATE yy_scan_string ( const char *yy_str  );
YY_BUFFER_STATE yy_scan_bytes ( const char *bytes, int len  );

void *yyalloc ( yy_size_t  );
void *yyrealloc ( void *, yy_size_t  );
void yyfree ( void *  );

#define yy_new_buffer yy_create_buffer
#define yy_set_interactive(is_interactive) \
	{ \
	if ( ! YY_CURRENT_BUFFER ){ \
        yyensure_buffer_stack (); \
		YY_CURRENT_BUFFER_LVALUE =    \
            yy_create_buffer( yyin, YY_BUF_SIZE ); \
	} \
	YY_CURRENT_BUFFER_LVALUE->yy_is_interactive = is_interactive; \
	}
#define yy_set_bol(at_bol) \
	{ \
	if ( ! YY_CURRENT_BUFFER ){\
        yyensure_buffer_stack (); \
		YY_CURRENT_BUFFER_LVALUE =    \
            yy_create_buffer( yyin, YY_BUF_SIZE ); \
	} \
	YY_CURRENT_BUFFER_LVALUE->yy_at_bol = at_bol; \
	}
#define YY_AT_BOL() (YY_CURRENT_BUFFER_LVALUE->yy_at_bol)

/* Begin user sect3 */
typedef flex_uint8_t YY_CHAR;

FILE *yyin = NULL, *yyout = NULL;

typedef int yy_state_type;

extern int yylineno;
int yylineno = 1;

extern char *yytext;
#ifdef yytext_ptr
#undef yytext_ptr
#endif
#define yytext_ptr yytext

static yy_state_type yy_get_previous_state ( void );
static yy_state_type yy_try_NUL_trans ( yy_state_type current_state  );
static int yy_get_next_buffer ( void );
static void yynoreturn yy_fatal_error ( const char* msg  );

/* Done after the current pattern has been matched and before the
 * corresponding action - sets up yytext.
 */
#define YY_DO_BEFORE_ACTION \
	(yytext_ptr) = yy_bp; \
	yyleng = (int) (yy_cp - yy_bp); \
	(yy_hold_char) = *yy_cp; \
	*yy_cp = '\0'; \
	(yy_c_buf_p) = yy_cp;
#define YY_NUM_RULES 84
#define YY_END_OF_BUFFER 85
/* This struct is not used in this scanner,
   but its presence is necessary. */
struct yy_trans_info
	{
	flex_int32_t yy_verify;
	flex_int32_t yy_nxt;
	};
static const flex_int16_t yy_accept[173] =
    {   0,
        0,    0,    0,    0,    0,    0,   85,   83,    1,    5,
        1,    2,   83,   42,   44,   43,   83,   51,   51,   50,
       49,   82,   82,   82,   82,   82,   82,   82,   82,   82,
       82,   82,   82,   82,   82,    3,    1,    4,    1,    5,
        2,   52,    0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,   51,    0,   82,   82,   82,   82,   82,
       82,   82,   82,   82,   82,   82,   82,   18,   82,   30,
       82,   82,   82,   82,   82,   82,   31,   82,   82,   82,
       82,   32,   82,   82,    3,    4,   56,   57,   58,   59,
       53,   80,   78,   81,   68,   69,   70,   71,   72,   73,

       74,   75,   79,   60,   61,   62,   63,   64,   65,   66,
       67,   76,   77,   54,   55,    0,    0,    0,    0,   51,
        6,    8,   23,   82,   82,   82,   82,   28,    9,   13,
       82,   34,   33,   29,   82,   82,   12,   10,   82,   37,
       82,   38,   14,   15,   82,    7,   82,   11,    0,   48,
        0,    0,   19,   20,   24,   26,   25,   27,   21,   35,
       16,   17,   40,   36,   39,   41,   22,   46,    0,   45,
       47,    0
    } ;

static const YY_CHAR yy_ec[256] =
    {   0,
        1,    1,    1,    1,    1,    1,    1,    1,    2,    3,
        2,    2,    4,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    2,    1,    1,    5,    6,    1,    1,    1,    1,
        1,    1,    7,    8,    9,   10,    1,   11,   12,   13,
       14,   15,   16,   17,   18,   19,   20,   21,    1,    1,
       22,    1,    1,    1,   23,   24,   25,   26,   27,   28,
       29,   30,   31,   32,   33,   34,   35,   36,   37,   38,
       39,   40,   41,   42,   43,   44,   45,   46,   33,   47,
        1,    1,    1,    1,   33,    1,   48,   49,   50,   51,

       52,   53,   54,   33,   33,   33,   55,   33,   33,   56,
       33,   57,   33,   58,   59,   60,   33,   61,   33,   62,
       33,   33,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,

        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1
    } ;

static const YY_CHAR yy_meta[63] =
    {   0,
        1,    1,    2,    1,    1,    1,    1,    1,    1,    1,
        3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
        1,    1,    4,    4,    4,    4,    4,    4,    5,    5,
        5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
        5,    5,    5,    5,    5,    5,    5,    4,    4,    4,
        4,    4,    4,    5,    5,    5,    5,    5,    5,    5,
        5,    6
    } ;

static const flex_int16_t yy_base[178] =
    {   0,
        0,    0,   60,   62,   64,   66,  224,  225,  225,  225,
      220,    0,   60,  225,  225,  225,   30,  160,  159,  225,
      225,   57,   58,    0,  189,  173,   61,   62,   60,  181,
       74,  192,   82,  179,  178,  225,  211,  225,  210,  225,
        0,    0,   98,  155,  154,  162,  117,  164,   86,  161,
      152,  147,  154,  143,    0,    0,  178,  177,  163,   75,
       96,  174,  153,  148,  155,  139,  134,    0,  128,    0,
       70,  136,   97,  139,  126,  137,    0,  132,  131,  124,
      139,    0,  122,  121,  225,  225,  225,  225,  225,  225,
      225,  225,  225,  225,  225,  225,  225,  225,  225,  225,

      225,  225,  225,  225,  225,  225,  225,  225,  225,  225,
      225,  225,  225,  225,  225,  100,  108,  109,   94,    0,
      124,  123,    0,  106,  105,  104,  103,    0,  118,    0,
      106,    0,    0,    0,  116,  109,    0,    0,  105,    0,
      104,    0,    0,    0,  120,    0,  116,  110,   92,  225,
       86,   66,    0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,  225,   50,  225,
      225,  225,  184,  188,  192,   88,  196
    } ;

static const flex_int16_t yy_def[178] =
    {   0,
      172,    1,    1,    1,    1,    1,  172,  172,  172,  172,
      172,  173,  172,  172,  172,  172,  172,  174,  174,  172,
      172,  175,  175,  175,  175,  175,  175,  175,  175,  175,
      175,  175,  175,  175,  175,  172,  172,  172,  172,  172,
      173,  176,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  174,  177,  175,  175,  175,  175,  175,
      175,  175,  175,  175,  175,  175,  175,  175,  175,  175,
      175,  175,  175,  175,  175,  175,  175,  175,  175,  175,
      175,  175,  175,  175,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,

      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  177,
      175,  175,  175,  175,  175,  175,  175,  175,  175,  175,
      175,  175,  175,  175,  175,  175,  175,  175,  175,  175,
      175,  175,  175,  175,  175,  175,  175,  175,  172,  172,
      172,  172,  175,  175,  175,  175,  175,  175,  175,  175,
      175,  175,  175,  175,  175,  175,  175,  172,  172,  172,
      172,    0,  172,  172,  172,  172,  172
    } ;

static const flex_int16_t yy_nxt[288] =
    {   0,
        8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
       18,   19,   19,   19,   19,   19,   19,   19,   19,   19,
       20,   21,   22,   23,   24,   25,   26,   24,   24,   24,
       24,   27,   24,   28,   29,   30,   24,   31,   24,   32,
       33,   24,   24,   24,   34,   35,   24,   24,   24,   24,
       24,   24,   24,   24,   24,   24,   24,   24,   24,   24,
       24,   24,   36,   37,   36,   37,   38,   39,   38,   39,
       42,   42,   42,   42,   42,   42,   42,   42,   42,   42,
       50,   51,   57,   66,   59,   69,   60,   71,   52,   53,
       42,   61,   58,   62,   63,   67,  114,  115,   74,  135,

       68,  124,   72,  136,  171,   77,   70,   43,   87,   88,
       89,   90,   44,   45,   75,   78,  125,   46,   47,   48,
       49,   79,  126,   80,   81,  170,   82,   95,   96,   97,
       98,   99,  100,  101,  102,  169,  138,  127,  139,  168,
      167,  166,  165,  164,  163,  162,  161,  160,  159,  158,
      157,  156,  155,  154,  153,  152,  151,   91,  150,  149,
      148,  147,  146,  145,  144,  143,  142,  141,  140,  137,
      134,  133,  132,  103,  104,  105,  106,  107,  108,  109,
      110,  111,  112,  113,   41,  131,   41,   41,   41,   41,
       54,  130,  129,   54,   56,   56,   56,   56,  120,  120,

      128,  123,  122,  121,  172,  119,  118,  117,  116,   94,
       93,   92,   86,   85,   84,   83,   76,   73,   65,   64,
      172,   55,   40,  172,    7,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172
    } ;

static const flex_int16_t yy_chk[288] =
    {   0,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
        1,    1,    3,    3,    4,    4,    5,    5,    6,    6,
       13,   13,   13,   13,   13,   13,   13,   13,   13,   13,
       17,   17,   22,   27,   23,   28,   23,   29,   17,   17,
      176,   23,   22,   23,   23,   27,   49,   49,   31,   71,

       27,   60,   29,   71,  169,   33,   28,   13,   43,   43,
       43,   43,   13,   13,   31,   33,   60,   13,   13,   13,
       13,   33,   61,   33,   33,  152,   33,   47,   47,   47,
       47,   47,   47,   47,   47,  151,   73,   61,   73,  149,
      148,  147,  145,  141,  139,  136,  135,  131,  129,  127,
      126,  125,  124,  122,  121,  119,  118,   43,  117,  116,
       84,   83,   81,   80,   79,   78,   76,   75,   74,   72,
       69,   67,   66,   47,   48,   48,   48,   48,   48,   48,
       48,   48,   48,   48,  173,   65,  173,  173,  173,  173,
      174,   64,   63,  174,  175,  175,  175,  175,  177,  177,

       62,   59,   58,   57,   54,   53,   52,   51,   50,   46,
       45,   44,   39,   37,   35,   34,   32,   30,   26,   25,
       19,   18,   11,    7,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172,  172,  172,  172,
      172,  172,  172,  172,  172,  172,  172
    } ;

/* Table of booleans, true if rule could match eol. */
static const flex_int32_t yy_rule_can_match_eol[85] =
    {   0,
0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0,     };

static yy_state_type yy_last_accepting_state;
static char *yy_last_accepting_cpos;

extern int yy_flex_debug;
int yy_flex_debug = 0;

/* The intent behind this definition is that it'll catch
 * any uses of REJECT which flex missed.
 */
#define REJECT reject_used_but_not_detected
#define yymore() yymore_used_but_not_detected
#define YY_MORE_ADJ 0
#define YY_RESTORE_YY_MORE_OFFSET
char *yytext;
#line 1 "asm_lexer.l"
/* $Id: asm_lexer.l,v 1.26 2023/09/22 20:18:13 leavens Exp $ */
/* Scanner for the SRM Assembly Language */
#line 10 "asm_lexer.l"
#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "parser_types.h"
#include "utilities.h"
#include "lexer.h"

 /* Tokens generated by Bison */
#include "asm.tab.h"

 /* need declaration of fileno, part of the C standard library.
   (Putting an extern declaration here shuts off a gcc warning.) */
extern int fileno(FILE *stream);

/* The filename of the file being read */
char *filename;

/* The value of a token */
extern YYSTYPE yylval;

/* The FILE used by the generated lexer */
extern FILE *yyin;

#undef yywrap   /* sometimes a macro by default */

extern char *strdup(const char *s);

// set the lexer's value for a token in yylval as an AST
static void tok2ast(int code) {
    AST t;
    t.token.file_loc = file_location_make(filename, yylineno);
    t.token.type_tag = token_ast;
    t.token.code = code;
    t.token.text = strdup(yytext);
    yylval = t;
}

static void reg2ast(const char *txt) {
    AST t;
    t.reg.file_loc = file_location_make(filename, yylineno);
    t.reg.type_tag = reg_ast;
    t.reg.text = strdup(yytext);
    unsigned short n;
    sscanf(txt, "%hd", &n);
    t.reg.number = n;
    yylval = t;
}

static void namedreg2ast(unsigned short num, const char *txt) {
    AST t;
    t.reg.file_loc = file_location_make(filename, yylineno);
    t.reg.type_tag = reg_ast;
    t.reg.text = strdup(yytext);
    t.reg.number = num;
    yylval = t;
}


static void ident2ast(const char *name) {
    AST t;
    t.ident.file_loc = file_location_make(filename, yylineno);
    t.ident.type_tag = ident_ast;
    t.ident.name = strdup(name);
    yylval = t;
}

static void unsignednum2ast(unsigned int val)
{
    AST t;
    t.unsignednum.file_loc = file_location_make(filename, yylineno);
    t.unsignednum.type_tag = unsignednum_ast;
    t.unsignednum.text = strdup(yytext);
    t.unsignednum.value = val;
    yylval = t;
}

#line 680 "asm_lexer.c"
#line 101 "asm_lexer.l"
  /* states of the lexer */


#line 685 "asm_lexer.c"

#define INITIAL 0
#define INSTRUCTION 1
#define DATADECL 2

#ifndef YY_NO_UNISTD_H
/* Special case for "unistd.h", since it is non-ANSI. We include it way
 * down here because we want the user's section 1 to have been scanned first.
 * The user has a chance to override it with an option.
 */
#include <unistd.h>
#endif

#ifndef YY_EXTRA_TYPE
#define YY_EXTRA_TYPE void *
#endif

static int yy_init_globals ( void );

/* Accessor methods to globals.
   These are made visible to non-reentrant scanners for convenience. */

int yylex_destroy ( void );

int yyget_debug ( void );

void yyset_debug ( int debug_flag  );

YY_EXTRA_TYPE yyget_extra ( void );

void yyset_extra ( YY_EXTRA_TYPE user_defined  );

FILE *yyget_in ( void );

void yyset_in  ( FILE * _in_str  );

FILE *yyget_out ( void );

void yyset_out  ( FILE * _out_str  );

			int yyget_leng ( void );

char *yyget_text ( void );

int yyget_lineno ( void );

void yyset_lineno ( int _line_number  );

YYSTYPE * yyget_lval ( void );

void yyset_lval ( YYSTYPE * yylval_param  );

/* Macros after this point can all be overridden by user definitions in
 * section 1.
 */

#ifndef YY_SKIP_YYWRAP
#ifdef __cplusplus
extern "C" int yywrap ( void );
#else
extern int yywrap ( void );
#endif
#endif

#ifndef YY_NO_UNPUT
    
    static void yyunput ( int c, char *buf_ptr  );
    
#endif

#ifndef yytext_ptr
static void yy_flex_strncpy ( char *, const char *, int );
#endif

#ifdef YY_NEED_STRLEN
static int yy_flex_strlen ( const char * );
#endif

#ifndef YY_NO_INPUT
#ifdef __cplusplus
static int yyinput ( void );
#else
static int input ( void );
#endif

#endif

/* Amount of stuff to slurp up with each read. */
#ifndef YY_READ_BUF_SIZE
#ifdef __ia64__
/* On IA-64, the buffer size is 16k, not 8k */
#define YY_READ_BUF_SIZE 16384
#else
#define YY_READ_BUF_SIZE 8192
#endif /* __ia64__ */
#endif

/* Copy whatever the last rule matched to the standard output. */
#ifndef ECHO
/* This used to be an fputs(), but since the string might contain NUL's,
 * we now use fwrite().
 */
#define ECHO do { if (fwrite( yytext, (size_t) yyleng, 1, yyout )) {} } while (0)
#endif

/* Gets input and stuffs it into "buf".  number of characters read, or YY_NULL,
 * is returned in "result".
 */
#ifndef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ( YY_CURRENT_BUFFER_LVALUE->yy_is_interactive ) \
		{ \
		int c = '*'; \
		int n; \
		for ( n = 0; n < max_size && \
			     (c = getc( yyin )) != EOF && c != '\n'; ++n ) \
			buf[n] = (char) c; \
		if ( c == '\n' ) \
			buf[n++] = (char) c; \
		if ( c == EOF && ferror( yyin ) ) \
			YY_FATAL_ERROR( "input in flex scanner failed" ); \
		result = n; \
		} \
	else \
		{ \
		errno=0; \
		while ( (result = (int) fread(buf, 1, (yy_size_t) max_size, yyin)) == 0 && ferror(yyin)) \
			{ \
			if( errno != EINTR) \
				{ \
				YY_FATAL_ERROR( "input in flex scanner failed" ); \
				break; \
				} \
			errno=0; \
			clearerr(yyin); \
			} \
		}\
\

#endif

/* No semi-colon after return; correct usage is to write "yyterminate();" -
 * we don't want an extra ';' after the "return" because that will cause
 * some compilers to complain about unreachable statements.
 */
#ifndef yyterminate
#define yyterminate() return YY_NULL
#endif

/* Number of entries by which start-condition stack grows. */
#ifndef YY_START_STACK_INCR
#define YY_START_STACK_INCR 25
#endif

/* Report a fatal error. */
#ifndef YY_FATAL_ERROR
#define YY_FATAL_ERROR(msg) yy_fatal_error( msg )
#endif

/* end tables serialization structures and prototypes */

/* Default declaration of generated scanner - a define so the user can
 * easily add parameters.
 */
#ifndef YY_DECL
#define YY_DECL_IS_OURS 1

extern int yylex \
               (YYSTYPE * yylval_param );

#define YY_DECL int yylex \
               (YYSTYPE * yylval_param )
#endif /* !YY_DECL */

/* Code executed at the beginning of each rule, after yytext and yyleng
 * have been set up.
 */
#ifndef YY_USER_ACTION
#define YY_USER_ACTION
#endif

/* Code executed at the end of each rule. */
#ifndef YY_BREAK
#define YY_BREAK /*LINTED*/break;
#endif

#define YY_RULE_SETUP \
	YY_USER_ACTION

/** The main scanner function which does all the work.
 */
YY_DECL
{
	yy_state_type yy_current_state;
	char *yy_cp, *yy_bp;
	int yy_act;
    
        YYSTYPE * yylval;
    
    yylval = yylval_param;

	if ( !(yy_init) )
		{
		(yy_init) = 1;

#ifdef YY_USER_INIT
		YY_USER_INIT;
#endif

		if ( ! (yy_start) )
			(yy_start) = 1;	/* first start state */

		if ( ! yyin )
			yyin = stdin;

		if ( ! yyout )
			yyout = stdout;

		if ( ! YY_CURRENT_BUFFER ) {
			yyensure_buffer_stack ();
			YY_CURRENT_BUFFER_LVALUE =
				yy_create_buffer( yyin, YY_BUF_SIZE );
		}

		yy_load_buffer_state(  );
		}

	{
#line 106 "asm_lexer.l"


#line 917 "asm_lexer.c"

	while ( /*CONSTCOND*/1 )		/* loops until end-of-file is reached */
		{
		yy_cp = (yy_c_buf_p);

		/* Support of yytext. */
		*yy_cp = (yy_hold_char);

		/* yy_bp points to the position in yy_ch_buf of the start of
		 * the current run.
		 */
		yy_bp = yy_cp;

		yy_current_state = (yy_start);
yy_match:
		do
			{
			YY_CHAR yy_c = yy_ec[YY_SC_TO_UI(*yy_cp)] ;
			if ( yy_accept[yy_current_state] )
				{
				(yy_last_accepting_state) = yy_current_state;
				(yy_last_accepting_cpos) = yy_cp;
				}
			while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )
				{
				yy_current_state = (int) yy_def[yy_current_state];
				if ( yy_current_state >= 173 )
					yy_c = yy_meta[yy_c];
				}
			yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
			++yy_cp;
			}
		while ( yy_base[yy_current_state] != 225 );

yy_find_action:
		yy_act = yy_accept[yy_current_state];
		if ( yy_act == 0 )
			{ /* have to back up */
			yy_cp = (yy_last_accepting_cpos);
			yy_current_state = (yy_last_accepting_state);
			yy_act = yy_accept[yy_current_state];
			}

		YY_DO_BEFORE_ACTION;

		if ( yy_act != YY_END_OF_BUFFER && yy_rule_can_match_eol[yy_act] )
			{
			int yyl;
			for ( yyl = 0; yyl < yyleng; ++yyl )
				if ( yytext[yyl] == '\n' )
					
    yylineno++;
;
			}

do_action:	/* This label is used only to access EOF actions. */

		switch ( yy_act )
	{ /* beginning of action switch */
			case 0: /* must back up */
			/* undo the effects of YY_DO_BEFORE_ACTION */
			*yy_cp = (yy_hold_char);
			yy_cp = (yy_last_accepting_cpos);
			yy_current_state = (yy_last_accepting_state);
			goto yy_find_action;

case 1:
YY_RULE_SETUP
#line 108 "asm_lexer.l"
{ ; } /* do nothing */
	YY_BREAK
case 2:
YY_RULE_SETUP
#line 109 "asm_lexer.l"
{ ; } /* ignore comments */
	YY_BREAK
case 3:
/* rule 3 can match eol */
YY_RULE_SETUP
#line 110 "asm_lexer.l"
{ BEGIN INITIAL; return eolsym; }
	YY_BREAK
case 4:
/* rule 4 can match eol */
YY_RULE_SETUP
#line 111 "asm_lexer.l"
{ BEGIN INITIAL; return eolsym; }
	YY_BREAK
case 5:
/* rule 5 can match eol */
YY_RULE_SETUP
#line 112 "asm_lexer.l"
{ ; } /* ignore EOL outside of the above states */
	YY_BREAK
case 6:
YY_RULE_SETUP
#line 114 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(addopsym); return addopsym; }
	YY_BREAK
case 7:
YY_RULE_SETUP
#line 115 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(subopsym); return subopsym; }
	YY_BREAK
case 8:
YY_RULE_SETUP
#line 116 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(andopsym); return andopsym; }
	YY_BREAK
case 9:
YY_RULE_SETUP
#line 117 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(boropsym); return boropsym; }
	YY_BREAK
case 10:
YY_RULE_SETUP
#line 118 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(noropsym); return noropsym; }
	YY_BREAK
case 11:
YY_RULE_SETUP
#line 119 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(xoropsym); return xoropsym; }
	YY_BREAK
case 12:
YY_RULE_SETUP
#line 120 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(mulopsym); return mulopsym; }
	YY_BREAK
case 13:
YY_RULE_SETUP
#line 121 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(divopsym); return divopsym; }
	YY_BREAK
case 14:
YY_RULE_SETUP
#line 122 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(sllopsym); return sllopsym; }
	YY_BREAK
case 15:
YY_RULE_SETUP
#line 123 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(srlopsym); return srlopsym; }
	YY_BREAK
case 16:
YY_RULE_SETUP
#line 124 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(mfhiopsym); return mfhiopsym; }
	YY_BREAK
case 17:
YY_RULE_SETUP
#line 125 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(mfloopsym); return mfloopsym; }
	YY_BREAK
case 18:
YY_RULE_SETUP
#line 126 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(jropsym); return jropsym; }
	YY_BREAK
case 19:
YY_RULE_SETUP
#line 127 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(addiopsym); return addiopsym; }
	YY_BREAK
case 20:
YY_RULE_SETUP
#line 128 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(andiopsym); return andiopsym; }
	YY_BREAK
case 21:
YY_RULE_SETUP
#line 129 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(boriopsym); return boriopsym; }
	YY_BREAK
case 22:
YY_RULE_SETUP
#line 130 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(xoriopsym); return xoriopsym; }
	YY_BREAK
case 23:
YY_RULE_SETUP
#line 131 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(beqopsym); return beqopsym; }
	YY_BREAK
case 24:
YY_RULE_SETUP
#line 132 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(bgezopsym); return bgezopsym; }
	YY_BREAK
case 25:
YY_RULE_SETUP
#line 133 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(blezopsym); return blezopsym; }
	YY_BREAK
case 26:
YY_RULE_SETUP
#line 134 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(bgtzopsym); return bgtzopsym; }
	YY_BREAK
case 27:
YY_RULE_SETUP
#line 135 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(bltzopsym); return bltzopsym; }
	YY_BREAK
case 28:
YY_RULE_SETUP
#line 136 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(bneopsym); return bneopsym; }
	YY_BREAK
case 29:
YY_RULE_SETUP
#line 137 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(lbuopsym); return lbuopsym; }
	YY_BREAK
case 30:
YY_RULE_SETUP
#line 138 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(lwopsym); return lwopsym; }
	YY_BREAK
case 31:
YY_RULE_SETUP
#line 139 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(sbopsym); return sbopsym; }
	YY_BREAK
case 32:
YY_RULE_SETUP
#line 140 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(swopsym); return swopsym; }
	YY_BREAK
case 33:
YY_RULE_SETUP
#line 141 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(jmpopsym); return jmpopsym; }
	YY_BREAK
case 34:
YY_RULE_SETUP
#line 142 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(jalopsym); return jalopsym; }
	YY_BREAK
case 35:
YY_RULE_SETUP
#line 143 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(exitopsym); return exitopsym; }
	YY_BREAK
case 36:
YY_RULE_SETUP
#line 144 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(pstropsym); return pstropsym; }
	YY_BREAK
case 37:
YY_RULE_SETUP
#line 145 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(pchopsym); return pchopsym; }
	YY_BREAK
case 38:
YY_RULE_SETUP
#line 146 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(rchopsym); return rchopsym; }
	YY_BREAK
case 39:
YY_RULE_SETUP
#line 147 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(straopsym); return straopsym; }
	YY_BREAK
case 40:
YY_RULE_SETUP
#line 148 "asm_lexer.l"
{ BEGIN INSTRUCTION; tok2ast(notropsym); return notropsym; }
	YY_BREAK
case 41:
YY_RULE_SETUP
#line 150 "asm_lexer.l"
{ BEGIN DATADECL; tok2ast(wordsym); return wordsym; }
	YY_BREAK
case 42:
YY_RULE_SETUP
#line 152 "asm_lexer.l"
{ tok2ast(plussym); return plussym; }
	YY_BREAK
case 43:
YY_RULE_SETUP
#line 153 "asm_lexer.l"
{ tok2ast(minussym); return minussym; }
	YY_BREAK
case 44:
YY_RULE_SETUP
#line 154 "asm_lexer.l"
{ return commasym; }
	YY_BREAK
case 45:
YY_RULE_SETUP
#line 156 "asm_lexer.l"
{ tok2ast(dottextsym); return dottextsym; }
	YY_BREAK
case 46:
YY_RULE_SETUP
#line 157 "asm_lexer.l"
{ tok2ast(dotdatasym); return dotdatasym; }
	YY_BREAK
case 47:
YY_RULE_SETUP
#line 158 "asm_lexer.l"
{ tok2ast(dotstacksym); return dotstacksym; }
	YY_BREAK
case 48:
YY_RULE_SETUP
#line 159 "asm_lexer.l"
{ return dotendsym; }
	YY_BREAK
case 49:
YY_RULE_SETUP
#line 160 "asm_lexer.l"
{ tok2ast(equalsym); return equalsym; }
	YY_BREAK
case 50:
YY_RULE_SETUP
#line 161 "asm_lexer.l"
{ return colonsym; }
	YY_BREAK
case 51:
YY_RULE_SETUP
#line 163 "asm_lexer.l"
{ unsigned int val;
                  int ssf_ret;
                  if (yyleng >= 2 && (strncmp(yytext, "0x", 2) == 0)) {
                      // hex literal
                      ssf_ret = sscanf(yytext+2, "%xt", &val);
                      if (ssf_ret != 1) {
                         bail_with_error("Unsigned hex literal (%s) could not be read by lexer!",
                         yytext);
                      }
                  } else {
                      ssf_ret = sscanf(yytext, "%ut", &val);
                      if (ssf_ret != 1) {
                         bail_with_error("Unsigned decimal literal (%s) could not be read by lexer!",
                         yytext);
                      }
                  }
                  unsignednum2ast(val);
                  return unsignednumsym; 
                }
	YY_BREAK
case 52:
YY_RULE_SETUP
#line 183 "asm_lexer.l"
{ reg2ast(yytext+1); return regsym; }
	YY_BREAK
case 53:
YY_RULE_SETUP
#line 184 "asm_lexer.l"
{ namedreg2ast(1,yytext); return regsym; }
	YY_BREAK
case 54:
YY_RULE_SETUP
#line 185 "asm_lexer.l"
{ namedreg2ast(2,yytext); return regsym; }
	YY_BREAK
case 55:
YY_RULE_SETUP
#line 186 "asm_lexer.l"
{ namedreg2ast(3,yytext); return regsym; }
	YY_BREAK
case 56:
YY_RULE_SETUP
#line 187 "asm_lexer.l"
{ namedreg2ast(4,yytext); return regsym; }
	YY_BREAK
case 57:
YY_RULE_SETUP
#line 188 "asm_lexer.l"
{ namedreg2ast(5,yytext); return regsym; }
	YY_BREAK
case 58:
YY_RULE_SETUP
#line 189 "asm_lexer.l"
{ namedreg2ast(6,yytext); return regsym; }
	YY_BREAK
case 59:
YY_RULE_SETUP
#line 190 "asm_lexer.l"
{ namedreg2ast(7,yytext); return regsym; }
	YY_BREAK
case 60:
YY_RULE_SETUP
#line 191 "asm_lexer.l"
{ namedreg2ast(8,yytext); return regsym; }
	YY_BREAK
case 61:
YY_RULE_SETUP
#line 192 "asm_lexer.l"
{ namedreg2ast(9,yytext); return regsym; }
	YY_BREAK
case 62:
YY_RULE_SETUP
#line 193 "asm_lexer.l"
{ namedreg2ast(10,yytext); return regsym; }
	YY_BREAK
case 63:
YY_RULE_SETUP
#line 194 "asm_lexer.l"
{ namedreg2ast(11,yytext); return regsym; }
	YY_BREAK
case 64:
YY_RULE_SETUP
#line 195 "asm_lexer.l"
{ namedreg2ast(12,yytext); return regsym; }
	YY_BREAK
case 65:
YY_RULE_SETUP
#line 196 "asm_lexer.l"
{ namedreg2ast(13,yytext); return regsym; }
	YY_BREAK
case 66:
YY_RULE_SETUP
#line 197 "asm_lexer.l"
{ namedreg2ast(14,yytext); return regsym; }
	YY_BREAK
case 67:
YY_RULE_SETUP
#line 198 "asm_lexer.l"
{ namedreg2ast(15,yytext); return regsym; }
	YY_BREAK
case 68:
YY_RULE_SETUP
#line 199 "asm_lexer.l"
{ namedreg2ast(16,yytext); return regsym; }
	YY_BREAK
case 69:
YY_RULE_SETUP
#line 200 "asm_lexer.l"
{ namedreg2ast(17,yytext); return regsym; }
	YY_BREAK
case 70:
YY_RULE_SETUP
#line 201 "asm_lexer.l"
{ namedreg2ast(18,yytext); return regsym; }
	YY_BREAK
case 71:
YY_RULE_SETUP
#line 202 "asm_lexer.l"
{ namedreg2ast(19,yytext); return regsym; }
	YY_BREAK
case 72:
YY_RULE_SETUP
#line 203 "asm_lexer.l"
{ namedreg2ast(20,yytext); return regsym; }
	YY_BREAK
case 73:
YY_RULE_SETUP
#line 204 "asm_lexer.l"
{ namedreg2ast(21,yytext); return regsym; }
	YY_BREAK
case 74:
YY_RULE_SETUP
#line 205 "asm_lexer.l"
{ namedreg2ast(22,yytext); return regsym; }
	YY_BREAK
case 75:
YY_RULE_SETUP
#line 206 "asm_lexer.l"
{ namedreg2ast(23,yytext); return regsym; }
	YY_BREAK
case 76:
YY_RULE_SETUP
#line 207 "asm_lexer.l"
{ namedreg2ast(24,yytext); return regsym; }
	YY_BREAK
case 77:
YY_RULE_SETUP
#line 208 "asm_lexer.l"
{ namedreg2ast(25,yytext); return regsym; }
	YY_BREAK
case 78:
YY_RULE_SETUP
#line 209 "asm_lexer.l"
{ namedreg2ast(28,yytext); return regsym; }
	YY_BREAK
case 79:
YY_RULE_SETUP
#line 210 "asm_lexer.l"
{ namedreg2ast(29,yytext); return regsym; }
	YY_BREAK
case 80:
YY_RULE_SETUP
#line 211 "asm_lexer.l"
{ namedreg2ast(30,yytext); return regsym; }
	YY_BREAK
case 81:
YY_RULE_SETUP
#line 212 "asm_lexer.l"
{ namedreg2ast(31,yytext); return regsym; }
	YY_BREAK
case 82:
YY_RULE_SETUP
#line 214 "asm_lexer.l"
{ ident2ast(yytext); return identsym; }
	YY_BREAK
case 83:
YY_RULE_SETUP
#line 216 "asm_lexer.l"
{ char msgbuf[512];
      sprintf(msgbuf, "invalid character: '%c' ('\\0%o')", *yytext, *yytext);
      yyerror(lexer_filename(), msgbuf);
    }
	YY_BREAK
case 84:
YY_RULE_SETUP
#line 220 "asm_lexer.l"
ECHO;
	YY_BREAK
#line 1428 "asm_lexer.c"
case YY_STATE_EOF(INITIAL):
case YY_STATE_EOF(INSTRUCTION):
case YY_STATE_EOF(DATADECL):
	yyterminate();

	case YY_END_OF_BUFFER:
		{
		/* Amount of text matched not including the EOB char. */
		int yy_amount_of_matched_text = (int) (yy_cp - (yytext_ptr)) - 1;

		/* Undo the effects of YY_DO_BEFORE_ACTION. */
		*yy_cp = (yy_hold_char);
		YY_RESTORE_YY_MORE_OFFSET

		if ( YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_NEW )
			{
			/* We're scanning a new file or input source.  It's
			 * possible that this happened because the user
			 * just pointed yyin at a new source and called
			 * yylex().  If so, then we have to assure
			 * consistency between YY_CURRENT_BUFFER and our
			 * globals.  Here is the right place to do so, because
			 * this is the first action (other than possibly a
			 * back-up) that will match for the new input source.
			 */
			(yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
			YY_CURRENT_BUFFER_LVALUE->yy_input_file = yyin;
			YY_CURRENT_BUFFER_LVALUE->yy_buffer_status = YY_BUFFER_NORMAL;
			}

		/* Note that here we test for yy_c_buf_p "<=" to the position
		 * of the first EOB in the buffer, since yy_c_buf_p will
		 * already have been incremented past the NUL character
		 * (since all states make transitions on EOB to the
		 * end-of-buffer state).  Contrast this with the test
		 * in input().
		 */
		if ( (yy_c_buf_p) <= &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)] )
			{ /* This was really a NUL. */
			yy_state_type yy_next_state;

			(yy_c_buf_p) = (yytext_ptr) + yy_amount_of_matched_text;

			yy_current_state = yy_get_previous_state(  );

			/* Okay, we're now positioned to make the NUL
			 * transition.  We couldn't have
			 * yy_get_previous_state() go ahead and do it
			 * for us because it doesn't know how to deal
			 * with the possibility of jamming (and we don't
			 * want to build jamming into it because then it
			 * will run more slowly).
			 */

			yy_next_state = yy_try_NUL_trans( yy_current_state );

			yy_bp = (yytext_ptr) + YY_MORE_ADJ;

			if ( yy_next_state )
				{
				/* Consume the NUL. */
				yy_cp = ++(yy_c_buf_p);
				yy_current_state = yy_next_state;
				goto yy_match;
				}

			else
				{
				yy_cp = (yy_c_buf_p);
				goto yy_find_action;
				}
			}

		else switch ( yy_get_next_buffer(  ) )
			{
			case EOB_ACT_END_OF_FILE:
				{
				(yy_did_buffer_switch_on_eof) = 0;

				if ( yywrap(  ) )
					{
					/* Note: because we've taken care in
					 * yy_get_next_buffer() to have set up
					 * yytext, we can now set up
					 * yy_c_buf_p so that if some total
					 * hoser (like flex itself) wants to
					 * call the scanner after we return the
					 * YY_NULL, it'll still work - another
					 * YY_NULL will get returned.
					 */
					(yy_c_buf_p) = (yytext_ptr) + YY_MORE_ADJ;

					yy_act = YY_STATE_EOF(YY_START);
					goto do_action;
					}

				else
					{
					if ( ! (yy_did_buffer_switch_on_eof) )
						YY_NEW_FILE;
					}
				break;
				}

			case EOB_ACT_CONTINUE_SCAN:
				(yy_c_buf_p) =
					(yytext_ptr) + yy_amount_of_matched_text;

				yy_current_state = yy_get_previous_state(  );

				yy_cp = (yy_c_buf_p);
				yy_bp = (yytext_ptr) + YY_MORE_ADJ;
				goto yy_match;

			case EOB_ACT_LAST_MATCH:
				(yy_c_buf_p) =
				&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)];

				yy_current_state = yy_get_previous_state(  );

				yy_cp = (yy_c_buf_p);
				yy_bp = (yytext_ptr) + YY_MORE_ADJ;
				goto yy_find_action;
			}
		break;
		}

	default:
		YY_FATAL_ERROR(
			"fatal flex scanner internal error--no action found" );
	} /* end of action switch */
		} /* end of scanning one token */
	} /* end of user's declarations */
} /* end of yylex */

/* yy_get_next_buffer - try to read in a new buffer
 *
 * Returns a code representing an action:
 *	EOB_ACT_LAST_MATCH -
 *	EOB_ACT_CONTINUE_SCAN - continue scanning from current position
 *	EOB_ACT_END_OF_FILE - end of file
 */
static int yy_get_next_buffer (void)
{
    	char *dest = YY_CURRENT_BUFFER_LVALUE->yy_ch_buf;
	char *source = (yytext_ptr);
	int number_to_move, i;
	int ret_val;

	if ( (yy_c_buf_p) > &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars) + 1] )
		YY_FATAL_ERROR(
		"fatal flex scanner internal error--end of buffer missed" );

	if ( YY_CURRENT_BUFFER_LVALUE->yy_fill_buffer == 0 )
		{ /* Don't try to fill the buffer, so this is an EOF. */
		if ( (yy_c_buf_p) - (yytext_ptr) - YY_MORE_ADJ == 1 )
			{
			/* We matched a single character, the EOB, so
			 * treat this as a final EOF.
			 */
			return EOB_ACT_END_OF_FILE;
			}

		else
			{
			/* We matched some text prior to the EOB, first
			 * process it.
			 */
			return EOB_ACT_LAST_MATCH;
			}
		}

	/* Try to read more data. */

	/* First move last chars to start of buffer. */
	number_to_move = (int) ((yy_c_buf_p) - (yytext_ptr) - 1);

	for ( i = 0; i < number_to_move; ++i )
		*(dest++) = *(source++);

	if ( YY_CURRENT_BUFFER_LVALUE->yy_buffer_status == YY_BUFFER_EOF_PENDING )
		/* don't do the read, it's not guaranteed to return an EOF,
		 * just force an EOF
		 */
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars) = 0;

	else
		{
			int num_to_read =
			YY_CURRENT_BUFFER_LVALUE->yy_buf_size - number_to_move - 1;

		while ( num_to_read <= 0 )
			{ /* Not enough room in the buffer - grow it. */

			/* just a shorter name for the current buffer */
			YY_BUFFER_STATE b = YY_CURRENT_BUFFER_LVALUE;

			int yy_c_buf_p_offset =
				(int) ((yy_c_buf_p) - b->yy_ch_buf);

			if ( b->yy_is_our_buffer )
				{
				int new_size = b->yy_buf_size * 2;

				if ( new_size <= 0 )
					b->yy_buf_size += b->yy_buf_size / 8;
				else
					b->yy_buf_size *= 2;

				b->yy_ch_buf = (char *)
					/* Include room in for 2 EOB chars. */
					yyrealloc( (void *) b->yy_ch_buf,
							 (yy_size_t) (b->yy_buf_size + 2)  );
				}
			else
				/* Can't grow it, we don't own it. */
				b->yy_ch_buf = NULL;

			if ( ! b->yy_ch_buf )
				YY_FATAL_ERROR(
				"fatal error - scanner input buffer overflow" );

			(yy_c_buf_p) = &b->yy_ch_buf[yy_c_buf_p_offset];

			num_to_read = YY_CURRENT_BUFFER_LVALUE->yy_buf_size -
						number_to_move - 1;

			}

		if ( num_to_read > YY_READ_BUF_SIZE )
			num_to_read = YY_READ_BUF_SIZE;

		/* Read in more data. */
		YY_INPUT( (&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[number_to_move]),
			(yy_n_chars), num_to_read );

		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	if ( (yy_n_chars) == 0 )
		{
		if ( number_to_move == YY_MORE_ADJ )
			{
			ret_val = EOB_ACT_END_OF_FILE;
			yyrestart( yyin  );
			}

		else
			{
			ret_val = EOB_ACT_LAST_MATCH;
			YY_CURRENT_BUFFER_LVALUE->yy_buffer_status =
				YY_BUFFER_EOF_PENDING;
			}
		}

	else
		ret_val = EOB_ACT_CONTINUE_SCAN;

	if (((yy_n_chars) + number_to_move) > YY_CURRENT_BUFFER_LVALUE->yy_buf_size) {
		/* Extend the array by 50%, plus the number we really need. */
		int new_size = (yy_n_chars) + number_to_move + ((yy_n_chars) >> 1);
		YY_CURRENT_BUFFER_LVALUE->yy_ch_buf = (char *) yyrealloc(
			(void *) YY_CURRENT_BUFFER_LVALUE->yy_ch_buf, (yy_size_t) new_size  );
		if ( ! YY_CURRENT_BUFFER_LVALUE->yy_ch_buf )
			YY_FATAL_ERROR( "out of dynamic memory in yy_get_next_buffer()" );
		/* "- 2" to take care of EOB's */
		YY_CURRENT_BUFFER_LVALUE->yy_buf_size = (int) (new_size - 2);
	}

	(yy_n_chars) += number_to_move;
	YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)] = YY_END_OF_BUFFER_CHAR;
	YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars) + 1] = YY_END_OF_BUFFER_CHAR;

	(yytext_ptr) = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[0];

	return ret_val;
}

/* yy_get_previous_state - get the state just before the EOB char was reached */

    static yy_state_type yy_get_previous_state (void)
{
	yy_state_type yy_current_state;
	char *yy_cp;
    
	yy_current_state = (yy_start);

	for ( yy_cp = (yytext_ptr) + YY_MORE_ADJ; yy_cp < (yy_c_buf_p); ++yy_cp )
		{
		YY_CHAR yy_c = (*yy_cp ? yy_ec[YY_SC_TO_UI(*yy_cp)] : 1);
		if ( yy_accept[yy_current_state] )
			{
			(yy_last_accepting_state) = yy_current_state;
			(yy_last_accepting_cpos) = yy_cp;
			}
		while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )
			{
			yy_current_state = (int) yy_def[yy_current_state];
			if ( yy_current_state >= 173 )
				yy_c = yy_meta[yy_c];
			}
		yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
		}

	return yy_current_state;
}

/* yy_try_NUL_trans - try to make a transition on the NUL character
 *
 * synopsis
 *	next_state = yy_try_NUL_trans( current_state );
 */
    static yy_state_type yy_try_NUL_trans  (yy_state_type yy_current_state )
{
	int yy_is_jam;
    	char *yy_cp = (yy_c_buf_p);

	YY_CHAR yy_c = 1;
	if ( yy_accept[yy_current_state] )
		{
		(yy_last_accepting_state) = yy_current_state;
		(yy_last_accepting_cpos) = yy_cp;
		}
	while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )
		{
		yy_current_state = (int) yy_def[yy_current_state];
		if ( yy_current_state >= 173 )
			yy_c = yy_meta[yy_c];
		}
	yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];
	yy_is_jam = (yy_current_state == 172);

		return yy_is_jam ? 0 : yy_current_state;
}

#ifndef YY_NO_UNPUT

    static void yyunput (int c, char * yy_bp )
{
	char *yy_cp;
    
    yy_cp = (yy_c_buf_p);

	/* undo effects of setting up yytext */
	*yy_cp = (yy_hold_char);

	if ( yy_cp < YY_CURRENT_BUFFER_LVALUE->yy_ch_buf + 2 )
		{ /* need to shift things up to make room */
		/* +2 for EOB chars. */
		int number_to_move = (yy_n_chars) + 2;
		char *dest = &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[
					YY_CURRENT_BUFFER_LVALUE->yy_buf_size + 2];
		char *source =
				&YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[number_to_move];

		while ( source > YY_CURRENT_BUFFER_LVALUE->yy_ch_buf )
			*--dest = *--source;

		yy_cp += (int) (dest - source);
		yy_bp += (int) (dest - source);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars =
			(yy_n_chars) = (int) YY_CURRENT_BUFFER_LVALUE->yy_buf_size;

		if ( yy_cp < YY_CURRENT_BUFFER_LVALUE->yy_ch_buf + 2 )
			YY_FATAL_ERROR( "flex scanner push-back overflow" );
		}

	*--yy_cp = (char) c;

    if ( c == '\n' ){
        --yylineno;
    }

	(yytext_ptr) = yy_bp;
	(yy_hold_char) = *yy_cp;
	(yy_c_buf_p) = yy_cp;
}

#endif

#ifndef YY_NO_INPUT
#ifdef __cplusplus
    static int yyinput (void)
#else
    static int input  (void)
#endif

{
	int c;
    
	*(yy_c_buf_p) = (yy_hold_char);

	if ( *(yy_c_buf_p) == YY_END_OF_BUFFER_CHAR )
		{
		/* yy_c_buf_p now points to the character we want to return.
		 * If this occurs *before* the EOB characters, then it's a
		 * valid NUL; if not, then we've hit the end of the buffer.
		 */
		if ( (yy_c_buf_p) < &YY_CURRENT_BUFFER_LVALUE->yy_ch_buf[(yy_n_chars)] )
			/* This was really a NUL. */
			*(yy_c_buf_p) = '\0';

		else
			{ /* need more input */
			int offset = (int) ((yy_c_buf_p) - (yytext_ptr));
			++(yy_c_buf_p);

			switch ( yy_get_next_buffer(  ) )
				{
				case EOB_ACT_LAST_MATCH:
					/* This happens because yy_g_n_b()
					 * sees that we've accumulated a
					 * token and flags that we need to
					 * try matching the token before
					 * proceeding.  But for input(),
					 * there's no matching to consider.
					 * So convert the EOB_ACT_LAST_MATCH
					 * to EOB_ACT_END_OF_FILE.
					 */

					/* Reset buffer status. */
					yyrestart( yyin );

					/*FALLTHROUGH*/

				case EOB_ACT_END_OF_FILE:
					{
					if ( yywrap(  ) )
						return 0;

					if ( ! (yy_did_buffer_switch_on_eof) )
						YY_NEW_FILE;
#ifdef __cplusplus
					return yyinput();
#else
					return input();
#endif
					}

				case EOB_ACT_CONTINUE_SCAN:
					(yy_c_buf_p) = (yytext_ptr) + offset;
					break;
				}
			}
		}

	c = *(unsigned char *) (yy_c_buf_p);	/* cast for 8-bit char's */
	*(yy_c_buf_p) = '\0';	/* preserve yytext */
	(yy_hold_char) = *++(yy_c_buf_p);

	if ( c == '\n' )
		
    yylineno++;
;

	return c;
}
#endif	/* ifndef YY_NO_INPUT */

/** Immediately switch to a different input stream.
 * @param input_file A readable stream.
 * 
 * @note This function does not reset the start condition to @c INITIAL .
 */
    void yyrestart  (FILE * input_file )
{
    
	if ( ! YY_CURRENT_BUFFER ){
        yyensure_buffer_stack ();
		YY_CURRENT_BUFFER_LVALUE =
            yy_create_buffer( yyin, YY_BUF_SIZE );
	}

	yy_init_buffer( YY_CURRENT_BUFFER, input_file );
	yy_load_buffer_state(  );
}

/** Switch to a different input buffer.
 * @param new_buffer The new input buffer.
 * 
 */
    void yy_switch_to_buffer  (YY_BUFFER_STATE  new_buffer )
{
    
	/* TODO. We should be able to replace this entire function body
	 * with
	 *		yypop_buffer_state();
	 *		yypush_buffer_state(new_buffer);
     */
	yyensure_buffer_stack ();
	if ( YY_CURRENT_BUFFER == new_buffer )
		return;

	if ( YY_CURRENT_BUFFER )
		{
		/* Flush out information for old buffer. */
		*(yy_c_buf_p) = (yy_hold_char);
		YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	YY_CURRENT_BUFFER_LVALUE = new_buffer;
	yy_load_buffer_state(  );

	/* We don't actually know whether we did this switch during
	 * EOF (yywrap()) processing, but the only time this flag
	 * is looked at is after yywrap() is called, so it's safe
	 * to go ahead and always set it.
	 */
	(yy_did_buffer_switch_on_eof) = 1;
}

static void yy_load_buffer_state  (void)
{
    	(yy_n_chars) = YY_CURRENT_BUFFER_LVALUE->yy_n_chars;
	(yytext_ptr) = (yy_c_buf_p) = YY_CURRENT_BUFFER_LVALUE->yy_buf_pos;
	yyin = YY_CURRENT_BUFFER_LVALUE->yy_input_file;
	(yy_hold_char) = *(yy_c_buf_p);
}

/** Allocate and initialize an input buffer state.
 * @param file A readable stream.
 * @param size The character buffer size in bytes. When in doubt, use @c YY_BUF_SIZE.
 * 
 * @return the allocated buffer state.
 */
    YY_BUFFER_STATE yy_create_buffer  (FILE * file, int  size )
{
	YY_BUFFER_STATE b;
    
	b = (YY_BUFFER_STATE) yyalloc( sizeof( struct yy_buffer_state )  );
	if ( ! b )
		YY_FATAL_ERROR( "out of dynamic memory in yy_create_buffer()" );

	b->yy_buf_size = size;

	/* yy_ch_buf has to be 2 characters longer than the size given because
	 * we need to put in 2 end-of-buffer characters.
	 */
	b->yy_ch_buf = (char *) yyalloc( (yy_size_t) (b->yy_buf_size + 2)  );
	if ( ! b->yy_ch_buf )
		YY_FATAL_ERROR( "out of dynamic memory in yy_create_buffer()" );

	b->yy_is_our_buffer = 1;

	yy_init_buffer( b, file );

	return b;
}

/** Destroy the buffer.
 * @param b a buffer created with yy_create_buffer()
 * 
 */
    void yy_delete_buffer (YY_BUFFER_STATE  b )
{
    
	if ( ! b )
		return;

	if ( b == YY_CURRENT_BUFFER ) /* Not sure if we should pop here. */
		YY_CURRENT_BUFFER_LVALUE = (YY_BUFFER_STATE) 0;

	if ( b->yy_is_our_buffer )
		yyfree( (void *) b->yy_ch_buf  );

	yyfree( (void *) b  );
}

/* Initializes or reinitializes a buffer.
 * This function is sometimes called more than once on the same buffer,
 * such as during a yyrestart() or at EOF.
 */
    static void yy_init_buffer  (YY_BUFFER_STATE  b, FILE * file )

{
	int oerrno = errno;
    
	yy_flush_buffer( b );

	b->yy_input_file = file;
	b->yy_fill_buffer = 1;

    /* If b is the current buffer, then yy_init_buffer was _probably_
     * called from yyrestart() or through yy_get_next_buffer.
     * In that case, we don't want to reset the lineno or column.
     */
    if (b != YY_CURRENT_BUFFER){
        b->yy_bs_lineno = 1;
        b->yy_bs_column = 0;
    }

        b->yy_is_interactive = file ? (isatty( fileno(file) ) > 0) : 0;
    
	errno = oerrno;
}

/** Discard all buffered characters. On the next scan, YY_INPUT will be called.
 * @param b the buffer state to be flushed, usually @c YY_CURRENT_BUFFER.
 * 
 */
    void yy_flush_buffer (YY_BUFFER_STATE  b )
{
    	if ( ! b )
		return;

	b->yy_n_chars = 0;

	/* We always need two end-of-buffer characters.  The first causes
	 * a transition to the end-of-buffer state.  The second causes
	 * a jam in that state.
	 */
	b->yy_ch_buf[0] = YY_END_OF_BUFFER_CHAR;
	b->yy_ch_buf[1] = YY_END_OF_BUFFER_CHAR;

	b->yy_buf_pos = &b->yy_ch_buf[0];

	b->yy_at_bol = 1;
	b->yy_buffer_status = YY_BUFFER_NEW;

	if ( b == YY_CURRENT_BUFFER )
		yy_load_buffer_state(  );
}

/** Pushes the new state onto the stack. The new state becomes
 *  the current state. This function will allocate the stack
 *  if necessary.
 *  @param new_buffer The new state.
 *  
 */
void yypush_buffer_state (YY_BUFFER_STATE new_buffer )
{
    	if (new_buffer == NULL)
		return;

	yyensure_buffer_stack();

	/* This block is copied from yy_switch_to_buffer. */
	if ( YY_CURRENT_BUFFER )
		{
		/* Flush out information for old buffer. */
		*(yy_c_buf_p) = (yy_hold_char);
		YY_CURRENT_BUFFER_LVALUE->yy_buf_pos = (yy_c_buf_p);
		YY_CURRENT_BUFFER_LVALUE->yy_n_chars = (yy_n_chars);
		}

	/* Only push if top exists. Otherwise, replace top. */
	if (YY_CURRENT_BUFFER)
		(yy_buffer_stack_top)++;
	YY_CURRENT_BUFFER_LVALUE = new_buffer;

	/* copied from yy_switch_to_buffer. */
	yy_load_buffer_state(  );
	(yy_did_buffer_switch_on_eof) = 1;
}

/** Removes and deletes the top of the stack, if present.
 *  The next element becomes the new top.
 *  
 */
void yypop_buffer_state (void)
{
    	if (!YY_CURRENT_BUFFER)
		return;

	yy_delete_buffer(YY_CURRENT_BUFFER );
	YY_CURRENT_BUFFER_LVALUE = NULL;
	if ((yy_buffer_stack_top) > 0)
		--(yy_buffer_stack_top);

	if (YY_CURRENT_BUFFER) {
		yy_load_buffer_state(  );
		(yy_did_buffer_switch_on_eof) = 1;
	}
}

/* Allocates the stack if it does not exist.
 *  Guarantees space for at least one push.
 */
static void yyensure_buffer_stack (void)
{
	yy_size_t num_to_alloc;
    
	if (!(yy_buffer_stack)) {

		/* First allocation is just for 2 elements, since we don't know if this
		 * scanner will even need a stack. We use 2 instead of 1 to avoid an
		 * immediate realloc on the next call.
         */
      num_to_alloc = 1; /* After all that talk, this was set to 1 anyways... */
		(yy_buffer_stack) = (struct yy_buffer_state**)yyalloc
								(num_to_alloc * sizeof(struct yy_buffer_state*)
								);
		if ( ! (yy_buffer_stack) )
			YY_FATAL_ERROR( "out of dynamic memory in yyensure_buffer_stack()" );

		memset((yy_buffer_stack), 0, num_to_alloc * sizeof(struct yy_buffer_state*));

		(yy_buffer_stack_max) = num_to_alloc;
		(yy_buffer_stack_top) = 0;
		return;
	}

	if ((yy_buffer_stack_top) >= ((yy_buffer_stack_max)) - 1){

		/* Increase the buffer to prepare for a possible push. */
		yy_size_t grow_size = 8 /* arbitrary grow size */;

		num_to_alloc = (yy_buffer_stack_max) + grow_size;
		(yy_buffer_stack) = (struct yy_buffer_state**)yyrealloc
								((yy_buffer_stack),
								num_to_alloc * sizeof(struct yy_buffer_state*)
								);
		if ( ! (yy_buffer_stack) )
			YY_FATAL_ERROR( "out of dynamic memory in yyensure_buffer_stack()" );

		/* zero only the new slots.*/
		memset((yy_buffer_stack) + (yy_buffer_stack_max), 0, grow_size * sizeof(struct yy_buffer_state*));
		(yy_buffer_stack_max) = num_to_alloc;
	}
}

/** Setup the input buffer state to scan directly from a user-specified character buffer.
 * @param base the character buffer
 * @param size the size in bytes of the character buffer
 * 
 * @return the newly allocated buffer state object.
 */
YY_BUFFER_STATE yy_scan_buffer  (char * base, yy_size_t  size )
{
	YY_BUFFER_STATE b;
    
	if ( size < 2 ||
	     base[size-2] != YY_END_OF_BUFFER_CHAR ||
	     base[size-1] != YY_END_OF_BUFFER_CHAR )
		/* They forgot to leave room for the EOB's. */
		return NULL;

	b = (YY_BUFFER_STATE) yyalloc( sizeof( struct yy_buffer_state )  );
	if ( ! b )
		YY_FATAL_ERROR( "out of dynamic memory in yy_scan_buffer()" );

	b->yy_buf_size = (int) (size - 2);	/* "- 2" to take care of EOB's */
	b->yy_buf_pos = b->yy_ch_buf = base;
	b->yy_is_our_buffer = 0;
	b->yy_input_file = NULL;
	b->yy_n_chars = b->yy_buf_size;
	b->yy_is_interactive = 0;
	b->yy_at_bol = 1;
	b->yy_fill_buffer = 0;
	b->yy_buffer_status = YY_BUFFER_NEW;

	yy_switch_to_buffer( b  );

	return b;
}

/** Setup the input buffer state to scan a string. The next call to yylex() will
 * scan from a @e copy of @a str.
 * @param yystr a NUL-terminated string to scan
 * 
 * @return the newly allocated buffer state object.
 * @note If you want to scan bytes that may contain NUL values, then use
 *       yy_scan_bytes() instead.
 */
YY_BUFFER_STATE yy_scan_string (const char * yystr )
{
    
	return yy_scan_bytes( yystr, (int) strlen(yystr) );
}

/** Setup the input buffer state to scan the given bytes. The next call to yylex() will
 * scan from a @e copy of @a bytes.
 * @param yybytes the byte buffer to scan
 * @param _yybytes_len the number of bytes in the buffer pointed to by @a bytes.
 * 
 * @return the newly allocated buffer state object.
 */
YY_BUFFER_STATE yy_scan_bytes  (const char * yybytes, int  _yybytes_len )
{
	YY_BUFFER_STATE b;
	char *buf;
	yy_size_t n;
	int i;
    
	/* Get memory for full buffer, including space for trailing EOB's. */
	n = (yy_size_t) (_yybytes_len + 2);
	buf = (char *) yyalloc( n  );
	if ( ! buf )
		YY_FATAL_ERROR( "out of dynamic memory in yy_scan_bytes()" );

	for ( i = 0; i < _yybytes_len; ++i )
		buf[i] = yybytes[i];

	buf[_yybytes_len] = buf[_yybytes_len+1] = YY_END_OF_BUFFER_CHAR;

	b = yy_scan_buffer( buf, n );
	if ( ! b )
		YY_FATAL_ERROR( "bad buffer in yy_scan_bytes()" );

	/* It's okay to grow etc. this buffer, and we should throw it
	 * away when we're done.
	 */
	b->yy_is_our_buffer = 1;

	return b;
}

#ifndef YY_EXIT_FAILURE
#define YY_EXIT_FAILURE 2
#endif

static void yynoreturn yy_fatal_error (const char* msg )
{
			fprintf( stderr, "%s\n", msg );
	exit( YY_EXIT_FAILURE );
}

/* Redefine yyless() so it works in section 3 code. */

#undef yyless
#define yyless(n) \
	do \
		{ \
		/* Undo effects of setting up yytext. */ \
        int yyless_macro_arg = (n); \
        YY_LESS_LINENO(yyless_macro_arg);\
		yytext[yyleng] = (yy_hold_char); \
		(yy_c_buf_p) = yytext + yyless_macro_arg; \
		(yy_hold_char) = *(yy_c_buf_p); \
		*(yy_c_buf_p) = '\0'; \
		yyleng = yyless_macro_arg; \
		} \
	while ( 0 )

/* Accessor  methods (get/set functions) to struct members. */

/** Get the current line number.
 * 
 */
int yyget_lineno  (void)
{
    
    return yylineno;
}

/** Get the input stream.
 * 
 */
FILE *yyget_in  (void)
{
        return yyin;
}

/** Get the output stream.
 * 
 */
FILE *yyget_out  (void)
{
        return yyout;
}

/** Get the length of the current token.
 * 
 */
int yyget_leng  (void)
{
        return yyleng;
}

/** Get the current token.
 * 
 */

char *yyget_text  (void)
{
        return yytext;
}

/** Set the current line number.
 * @param _line_number line number
 * 
 */
void yyset_lineno (int  _line_number )
{
    
    yylineno = _line_number;
}

/** Set the input stream. This does not discard the current
 * input buffer.
 * @param _in_str A readable stream.
 * 
 * @see yy_switch_to_buffer
 */
void yyset_in (FILE *  _in_str )
{
        yyin = _in_str ;
}

void yyset_out (FILE *  _out_str )
{
        yyout = _out_str ;
}

int yyget_debug  (void)
{
        return yy_flex_debug;
}

void yyset_debug (int  _bdebug )
{
        yy_flex_debug = _bdebug ;
}

static int yy_init_globals (void)
{
        /* Initialization is the same as for the non-reentrant scanner.
     * This function is called from yylex_destroy(), so don't allocate here.
     */

    /* We do not touch yylineno unless the option is enabled. */
    yylineno =  1;
    
    (yy_buffer_stack) = NULL;
    (yy_buffer_stack_top) = 0;
    (yy_buffer_stack_max) = 0;
    (yy_c_buf_p) = NULL;
    (yy_init) = 0;
    (yy_start) = 0;

/* Defined in main.c */
#ifdef YY_STDINIT
    yyin = stdin;
    yyout = stdout;
#else
    yyin = NULL;
    yyout = NULL;
#endif

    /* For future reference: Set errno on error, since we are called by
     * yylex_init()
     */
    return 0;
}

/* yylex_destroy is for both reentrant and non-reentrant scanners. */
int yylex_destroy  (void)
{
    
    /* Pop the buffer stack, destroying each element. */
	while(YY_CURRENT_BUFFER){
		yy_delete_buffer( YY_CURRENT_BUFFER  );
		YY_CURRENT_BUFFER_LVALUE = NULL;
		yypop_buffer_state();
	}

	/* Destroy the stack itself. */
	yyfree((yy_buffer_stack) );
	(yy_buffer_stack) = NULL;

    /* Reset the globals. This is important in a non-reentrant scanner so the next time
     * yylex() is called, initialization will occur. */
    yy_init_globals( );

    return 0;
}

/*
 * Internal utility routines.
 */

#ifndef yytext_ptr
static void yy_flex_strncpy (char* s1, const char * s2, int n )
{
		
	int i;
	for ( i = 0; i < n; ++i )
		s1[i] = s2[i];
}
#endif

#ifdef YY_NEED_STRLEN
static int yy_flex_strlen (const char * s )
{
	int n;
	for ( n = 0; s[n]; ++n )
		;

	return n;
}
#endif

void *yyalloc (yy_size_t  size )
{
			return malloc(size);
}

void *yyrealloc  (void * ptr, yy_size_t  size )
{
		
	/* The cast to (char *) in the following accommodates both
	 * implementations that use char* generic pointers, and those
	 * that use void* generic pointers.  It works with the latter
	 * because both ANSI C and C++ allow castless assignment from
	 * any pointer type to void*, and deal with argument conversions
	 * as though doing an assignment.
	 */
	return realloc(ptr, size);
}

void yyfree (void * ptr )
{
			free( (char *) ptr );	/* see yyrealloc() for (char *) cast */
}

#define YYTABLES_NAME "yytables"

#line 220 "asm_lexer.l"


/* Requires: fname != NULL
 * Requires: fname is the name of a readable file
 * Initialize the lexer and start it reading from the given file. */
void asm_lexer_init(char *fname) {
   filename = fname;    
   yyin = fopen(fname, "r");
   if (yyin == NULL) {
       bail_with_error("Lexer cannot open %s", fname);
   }
}

// Close the file yyin
// and return 0 to indicate that there are no more files
int yywrap() {
    if (yyin != NULL) {
	int rc = fclose(yyin);
	if (rc == EOF) {
	    bail_with_error("Cannot close %s!", filename);
	}
    }
    filename = NULL;
    return 1;  /* no more input */
}

/* Report an error to the user on stderr */
void yyerror(const char *filename, const char *msg)
{
    fflush(stdout);
    fprintf(stderr, "%s:%d: %s\n", filename, lexer_line(), msg);
}

/* On standard output:
 * Print a message about the file name of the lexer's input
 * and then print a heading for the lexer's output. */
extern void lexer_print_output_header();

/* Print information about the token t to stdout
 * followed by a newline */
extern void lexer_print_token(yytoken_kind_t t, unsigned int tline,
			      const char *txt);

/* Read all the tokens from the input file
 * and print each token on standard output
 * using the format in lexer_print_token */
void lexer_output()
{
    lexer_print_output_header();
    AST dummy;
    yytoken_kind_t t;
    do {
	t = yylex(&dummy);
	if (t == YYEOF) {
	    break;
	}
        if (t != eolsym) {
	    lexer_print_token(t, yylineno, yytext);
        } else {
	    lexer_print_token(t, yylineno, "\\n");
	}
    } while (t != YYEOF);
}

