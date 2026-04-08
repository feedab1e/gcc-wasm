/* WebAssembly cpu description.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   Contributed by feedable.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#ifndef GCC_WASM_H
#define GCC_WASM_H

#ifndef USED_FOR_TARGET
#include "statistics.h"
#include "vec.h"
#endif

/* Debug */
#define PREFERRED_DEBUGGING_TYPE NO_DEBUG
/* End debug */

#define CC1PLUS_SPEC "-stdlib=libc++"
#define ASM_SPEC "-r --enable-annotations %{v:-v} %{Wa,*:%*}"
#define STANDARD_STARTFILE_PREFIX_1 "/lib/wasm32-wasi/"
#define STARTFILE_SPEC "%R" STANDARD_STARTFILE_PREFIX_1 "crt1%O%s"
#define LIB_SPEC "%{!shared:%{!p:%{!pg:-lc}}%{p:-lc_p}%{pg:-lc_p}}"
#define LINK_SPEC "%{!o:-o a.out}"

/* Run-time target specifications.  */
#define TARGET_CPU_CPP_BUILTINS()	 \
  do					  \
    {					  \
      builtin_define_std ("wasm32");	  \
      builtin_assert ("machine=wasm32");  \
      builtin_assert ("cpu=wasm32");	  \
      cpp_define (parse_in, "__wasm32__");\
      cpp_define (parse_in, "__wasi__");  \
    }					  \
  while (0)

/* End run-time target specifications.  */

/* Regs */
#define WASM_RETURN_REGNUM 0
#define STACK_POINTER_REGNUM 1
#define FRAME_POINTER_REGNUM 2
#define ARG_POINTER_REGNUM 3
#define STATIC_CHAIN_REGNUM 4
#define WASM_CONTROL_POINTER_REGNUM 5
#define WASM_BASE_POINTER_REGNUM 6

/* If this isn't defined, loads form $args will be performed with nested
   functions */
#define FRAME_POINTER_CFA_OFFSET(FNDECL) ((void)(FNDECL), 0)

/* ??? IRA is a bit silly with on targets that don't do register allocation,
   in that it still needs free hard regs to play with, give it some for now */
#define REGISTER_NAMES	\
  {			\
    "$return",		\
    "$stack",		\
    "$frame",		\
    "$args",		\
    "$chain",		\
    "$control",		\
    "$base",		\
    "general0",		\
    "general1",		\
    "general2",		\
    "general3",		\
  }

#define TARGET_NO_REGISTER_ALLOCATION true
#define FIRST_PSEUDO_REGISTER 11
#define FIXED_REGISTERS	    { 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 }
#define CALL_USED_REGISTERS { 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 }

enum reg_class	     {  NO_REGS,    ALL_REGS,	LIM_REG_CLASSES };
#define REG_CLASS_NAMES    { "NO_REGS",  "ALL_REGS" }
#define REG_CLASS_CONTENTS { { 0x0000 }, { 0xFFFF } }
#define N_REG_CLASSES (int) LIM_REG_CLASSES

#define GENERAL_REGS ALL_REGS
#define REGNO_REG_CLASS(R) ALL_REGS
#define BASE_REG_CLASS ALL_REGS
#define INDEX_REG_CLASS NO_REGS

#define REGNO_OK_FOR_BASE_P(X) true
#define REGNO_OK_FOR_INDEX_P(X) false

#define CLASS_MAX_NREGS(class, mode) 1

#define PROMOTE_MODE(MODE, UNSIGNEDP, TYPE) \
  if ((MODE) == QImode || (MODE) == HImode) \
      (MODE) = SImode; \
/* End regs */

/* Calling convention */
#define PARM_BOUNDARY 8
#define STACK_BOUNDARY 128
#define FUNCTION_BOUNDARY 16
#define BIGGEST_ALIGNMENT 128
#define STRICT_ALIGNMENT 0

/* Copied from elf.h and other places.  We'd otherwise use
   BIGGEST_ALIGNMENT and fail a number of testcases.  */
#define MAX_OFILE_ALIGNMENT (32768 * 8)

#define FRAME_GROWS_DOWNWARD 0
#define STACK_GROWS_DOWNWARD 1

#ifndef USED_FOR_TARGET
#define CUMULATIVE_ARGS wasm_cumulative_args
struct wasm_cumulative_args {
  bool incoming;
  vec<rtx, va_gc> *args = NULL;
  tree type = NULL;
};
#endif

#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, INDIRECT, N_NAMED_ARGS) \
  CUM = {false}
#define INIT_CUMULATIVE_INCOMING_ARGS(CUM, FNTYPE, LIBNAME) \
  CUM = {true}

#define FUNCTION_ARG_REGNO_P(r) 0
#define FUNCTION_VALUE_REGNO_P(r) 0

#define ELIMINABLE_REGS				\
  {						\
    {ARG_POINTER_REGNUM, FRAME_POINTER_REGNUM}	\
  }

#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET) (OFFSET) = 0

#define FIRST_PARM_OFFSET(FNDECL) 0
/* We only use the stack for varargs, let's abuse that to achieve the proper
   layout for the varargs buffer */
#define ACCUMULATE_OUTGOING_ARGS 1

#define TRAMPOLINE_SIZE 64
#define TRAMPOLINE_ALIGNMENT 32
/* End calling convention */

/* Costs */
#define NO_FUNCTION_CSE 1
#define SLOW_BYTE_ACCESS 0
#define BRANCH_COST(speed_p, predictable_p) 6
/* End costs */

/* Data layout */
#define STORE_FLAG_VALUE 1

#define BITS_BIG_ENDIAN 1
#define BYTES_BIG_ENDIAN 0
#define WORDS_BIG_ENDIAN 0
#define UNITS_PER_WORD 8
#define MIN_UNITS_PER_WORD 4

#define Pmode SImode
#define FUNCTION_MODE SImode
#define CASE_VECTOR_MODE SImode

#define POINTER_SIZE 32
#define INT_TYPE_SIZE 32
#define CHAR_TYPE_SIZE 8
#define SHORT_TYPE_SIZE 16
#define LONG_TYPE_SIZE 32
#define LONG_LONG_TYPE_SIZE 64
#define DEFAULT_SIGNED_CHAR 0
#define WCHAR_TYPE_SIZE 32
/* ??? Need to figure out why do we have to set this in order for TI to not
   appear */
#define MAX_FIXED_MODE_SIZE 64

#define SIZE_TYPE "long unsigned int"
#define WCHAR_TYPE "long int"
#define PTRDIFF_TYPE "long int"
#define INTPTR_TYPE "long int"
#define UINTPTR_TYPE "long unsigned int"

/* From bpf */
#define INT8_TYPE "signed char"
#define INT16_TYPE "short int"
#define INT32_TYPE "int"
#define INT64_TYPE "long long int"
#define UINT8_TYPE "unsigned char"
#define UINT16_TYPE "short unsigned int"
#define UINT32_TYPE "unsigned int"
#define UINT64_TYPE "long long unsigned int"

#define INT_LEAST8_TYPE INT8_TYPE
#define INT_LEAST16_TYPE INT16_TYPE
#define INT_LEAST32_TYPE INT32_TYPE
#define INT_LEAST64_TYPE INT64_TYPE
#define UINT_LEAST8_TYPE UINT8_TYPE
#define UINT_LEAST16_TYPE UINT16_TYPE
#define UINT_LEAST32_TYPE UINT32_TYPE
#define UINT_LEAST64_TYPE UINT64_TYPE

#define INT_FAST8_TYPE INT8_TYPE
#define INT_FAST16_TYPE INT16_TYPE
#define INT_FAST32_TYPE INT32_TYPE
#define INT_FAST64_TYPE INT64_TYPE
#define UINT_FAST8_TYPE UINT8_TYPE
#define UINT_FAST16_TYPE UINT16_TYPE
#define UINT_FAST32_TYPE UINT32_TYPE
#define UINT_FAST64_TYPE UINT64_TYPE

#define MOVE_MAX 8
#define MAX_REGS_PER_ADDRESS 1
#define LEGITIMATE_PIC_OPERAND_P(X) 1

#define WORD_REGISTER_OPERATIONS 1
#define LOAD_EXTEND_OP(M) SIGN_EXTEND
/* End data layout */

/* Asm */
#define ASM_COMMENT_START ";;"
#define ASM_APP_ON ""
#define ASM_APP_OFF ""

#define GLOBAL_ASM_OP ""

#ifndef USED_FOR_TARGET
void wasm_generate_internal_label (char *buf, const char *pfx, size_t no);
#define ASM_GENERATE_INTERNAL_LABEL(LABEL, PREFIX, NUM)\
  wasm_generate_internal_label (LABEL, PREFIX, NUM)
#define ASM_OUTPUT_LABEL(stream, name) \
  gcc_unreachable ()
#define ASM_OUTPUT_ALIGN(...)

void wasm_assemble_data_begin (FILE *stream, tree decl, const char *name,
			       HOST_WIDE_INT size, HOST_WIDE_INT align,
			       bool pub);
#define ASM_DECLARE_OBJECT_NAME(FILE, DECL, NAME) \
  wasm_assemble_data_begin (FILE, NAME, DECL, 0, 0, true);
void wasm_assemble_data_zeros (FILE *stream, tree decl, const char *name,
			       HOST_WIDE_INT size, HOST_WIDE_INT align,
			       bool pub);
#define ASM_OUTPUT_ALIGNED_DECL_COMMON(FILE, DECL, NAME, SIZE, ALIGN) \
  wasm_assemble_data_zeros (FILE, DECL, NAME, SIZE, ALIGN, true);
#define ASM_OUTPUT_ALIGNED_DECL_LOCAL(FILE, DECL, NAME, SIZE, ALIGN) \
  wasm_assemble_data_zeros (FILE, DECL, NAME, SIZE, ALIGN, 0);

void wasm_asm_start_function (FILE *stream, tree decl, const char *name);
#define ASM_OUTPUT_FUNCTION_LABEL(stream, name, decl) \
  wasm_asm_start_function(stream, decl, name)
void wasm_asm_end_function (FILE *stream, tree decl, const char *name);
#define ASM_DECLARE_FUNCTION_SIZE(stream, name, decl) \
  wasm_asm_end_function(stream, decl, name)

void wasm_handle_import (FILE *stream, const char *name, const_tree decl);
#define ASM_OUTPUT_EXTERNAL(stream, decl, name) \
  wasm_handle_import (stream, name, decl)

void wasm_assemble_skip (FILE *stream, unsigned HOST_WIDE_INT len);
#define ASM_OUTPUT_SKIP(...) wasm_assemble_skip (__VA_ARGS__)
void wasm_assemble_ascii (FILE *stream, const char *data, int len);
#define ASM_OUTPUT_ASCII(...) wasm_assemble_ascii (__VA_ARGS__)
#endif

#define HAS_INIT_SECTION
#define SUPPORTS_INIT_PRIORITY 1
#define SUPPORTS_WEAK 1
/* End asm */

#define FUNCTION_PROFILER(file, labelno) \
  sorry_at (input_location, \
	    "profiling is not yet implemented for this architecture")

#ifndef USED_FOR_TARGET
struct GTY(()) machine_function
{
  vec<rtx, va_gc> *func_args;  /* Arg list for the current function.  */
  machine_mode return_mode;
  bool stdarg_p;

  wasm_cumulative_args *call;
  bitmap regs_ever_live;
};
#endif

#endif /* GCC_WASM_H */
