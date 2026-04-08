/* WebAssembly assembly output utilities.
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

#define IN_TARGET_CODE 1

#include <limits>
#include <utility>

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "regs.h"
#include "stringpool.h"
#include "attribs.h"
#include "gimple.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "output.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "calls.h"
#include "explow.h"
#include "expr.h"
#include "langhooks.h"
#include "cfgrtl.h"
#include "gimplify.h"
#include "reload.h"
#include "builtins.h"
#include "tree-pass.h"

void wasm_print_operand (FILE *stream, rtx value, int mode);

struct cfl_hash: int_hash<int, -1, -2> {};
extern hash_map<const_rtx, tree> external_libcalls;
extern hash_map<rtx_insn *, int> labels_to_cfno;
extern hash_map<cfl_hash, rtx_insn *> labelno_to_labels;

namespace
{

int indent;
void output_indent(FILE *stream)
{
  fprintf (stream, "%*s", indent * 2, "");
}

char *fake_out_file_data;
size_t fake_out_file_length;
FILE *saved_asm_out_file;

hash_map<tree, std::pair<tree, const_tree>> import_map;
hash_set<const_rtx> external_libcall_set;

bool
global_p (rtx reg)
{
  if (GET_CODE (reg) == SUBREG)
    reg = SUBREG_REG (reg);
  gcc_assert (GET_CODE (reg) == REG);
  return REGNO (reg) == STACK_POINTER_REGNUM;
}

bool
is_escape (char x)
{
  const char *escapes = "\"\'\\\t\n\r";
  while (*escapes and x != *escapes)
    ++escapes;
  return *escapes;
}

void
assemble_string (FILE *stream, const char *string, int size, bool hex_only)
{
  fprintf (stream, " \"");
  for (int i = 0; i != size; ++i)
    {
      char c[] = {'\\', string[i], '\0'};
      if (c[1] < 32 || c[1] > 127 || hex_only)
	fprintf (stream, "\\%0.2hhx", c[1]);
      else
	fprintf (stream, "%s", c + !is_escape(c[1]));
    }
  fprintf (stream, "\"");
}

void
assemble_zeroes (FILE *stream, unsigned HOST_WIDE_INT size)
{
  fputs (" \"", stream);
  for (unsigned HOST_WIDE_INT i = 0; i != size; ++i)
    fputs ("\\00", stream);
  fputs ("\"", stream);
}

void
assemble_const_int (FILE *stream, HOST_WIDE_INT value, unsigned size)
{
  char val[sizeof (HOST_WIDE_INT)];
  for (int i = 0; i != sizeof (HOST_WIDE_INT); ++i)
    val[i] = value >> (i * 8);
  assemble_string (stream, val, size, true);
}

/* Can handle following shapes:
   (const_int x)
   (symbol_ref x)
   (const (plus (symbol_ref x) (const_int y))
   (const (minus (symbol_ref x) (const_int y))
   (const (plus (const_int x) (const_int y))
   (const (minus (const_int x) (const_int y)) */
bool
assemble_integer (FILE *stream, rtx x, unsigned size, int)
{
  HOST_WIDE_INT addend = 0;

  if (GET_CODE (x) == CONST)
    {
      x = XEXP (x, 0);
      switch (GET_CODE (x))
	{
	default:
	  return false;
	case PLUS:
	  if (GET_CODE (XEXP (x, 1)) != CONST_INT)
	    return false;
	  addend = INTVAL (XEXP (x, 1));
	  x = XEXP (x, 0);
	  break;
	case MINUS:
	  if (GET_CODE (XEXP (x, 1)) != CONST_INT)
	    return false;
	  addend = -INTVAL (XEXP (x, 1));
	  x = XEXP (x, 0);
	  break;
	}
    }
  switch (GET_CODE (x))
    {
    default:
      return false;
    case CONST_INT:
      assemble_const_int (stream, INTVAL (x) + addend, size);
      return true;
    case SYMBOL_REF:
      tree decl = SYMBOL_REF_DECL (x);
      bool func_p = TREE_CODE (decl) == FUNCTION_DECL;
      const char *name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));

      assemble_zeroes (stream, size);
      fprintf (stream, " (@reloc i%d %s ", size * 8,
	       func_p ? "functable" : "data");
      wasm_print_operand (stream, x, 'l');
      if (addend)
	fprintf(stream, " %+ld", addend);
      fprintf (stream, ")");
      return true;
    }
}


void
print_type (FILE *stream, const_tree type, bool first = false)
{
  const char *delim = first ? "" : " ";
  switch (TREE_CODE (type))
    {
    default:
      gcc_unreachable ();
    case VOID_TYPE:
      break;
    case NULLPTR_TYPE:
    case VECTOR_TYPE:
    case COMPLEX_TYPE:
      print_type (stream, ptr_type_node, first);
      break;
    case UNION_TYPE:
    case RECORD_TYPE:
      if (TYPE_EMPTY_P (type))
	break;
      if (TYPE_TRANSPARENT_AGGR (type))
	print_type (stream, TREE_TYPE (first_field (type)), first);
      else
	print_type (stream, ptr_type_node, first);
      break;
    case POINTER_TYPE:
    case REFERENCE_TYPE:
    case OFFSET_TYPE:
    case INTEGER_TYPE:
    case BOOLEAN_TYPE:
    case ENUMERAL_TYPE:
      fprintf (stream, "%s%s", delim,
	       TYPE_PRECISION (type) > 32 ? "i64" : "i32");
      break;
    case REAL_TYPE:
      fprintf (stream, "%s%s", delim,
	       TYPE_PRECISION (type) > 32 ? "f64" : "f32");
      break;
    case FUNCTION_TYPE:
    case METHOD_TYPE:
      function_args_iterator it;
      tree *arg;
      fprintf (stream, "%s(param", delim);
      CUMULATIVE_ARGS args;
      INIT_CUMULATIVE_ARGS(args, type, nullptr, false, 0);
      FOREACH_FUNCTION_ARGS_PTR (type, arg, it)
      {
	function_arg_info info (*arg, stdarg_p (type));
	if (pass_by_reference(&args, info))
	  print_type (stream, intSI_type_node);
	else
	  print_type (stream, *arg);
      }
      fprintf (stream, ")");
      tree return_type = TREE_TYPE (type);
      fprintf (stream, " (result");
      if (return_type != void_type_node)
	{
	  print_type (stream, return_type);
	}
      fprintf (stream, ")");
      break;
    }
}
void
print_local_decl (FILE *stream, rtx reg, bool param = false)
{
  output_indent (stream);
  fprintf (stream, "(%s ", param ? "param" : "local");
  wasm_print_operand (stream, reg, 0);
  tree param_type = lang_hooks.types.type_for_mode (GET_MODE (reg), 0);
  print_type (stream, param_type);
  fprintf (stream, ")\n");
}

void assemble_visibility (FILE *stream, const_tree decl)
{
  if (decl && DECL_VISIBILITY (decl) == VISIBILITY_HIDDEN)
    fprintf (stream, " hidden");
}

void assemble_binding (FILE *stream, const_tree decl)
{
  if (!decl)
    return;
  if (DECL_WEAK (decl))
    fprintf (stream, " weak");
  if (!TREE_PUBLIC (decl))
    fprintf (stream, " local");
}

void assemble_init_prio (FILE *stream, const_tree decl)
{
  if (DECL_STATIC_CONSTRUCTOR (decl))
    {
      int prio = decl_init_priority_lookup (const_cast<tree> (decl));
      fprintf (stream, " (init_prio %d)", prio);
    }
}

void assemble_sym_name (FILE *stream, const char *name)
{
  fprintf (stream, " (name \"");
  assemble_name (stream, name);
  fprintf (stream, "\")");
}

void
assemble_data_import (FILE *stream, const_tree decl, const char *name)
{
  output_indent (stream);
  fprintf (stream, "(@sym.import.data $");
  assemble_name (stream, name);
  assemble_binding (stream, decl);
  assemble_visibility (stream, decl);
  assemble_sym_name (stream, name);
  fprintf (stream, ")\n");
}

/* Given the type, guess where assign_params must've put it */
void print_func_frame_related (FILE *stream, tree type)
{
  if (VOID_TYPE_P (type))
    return;
  if (TREE_CODE (type) == COMPLEX_TYPE
	    && targetm.calls.split_complex_arg (type))
    {
      print_func_frame_related (stream, TREE_TYPE (type));
      print_func_frame_related (stream, TREE_TYPE (type));
      return;
    }
  if (RECORD_OR_UNION_TYPE_P (type)
      && TYPE_TRANSPARENT_AGGR (type))
    type = TREE_TYPE (first_field (type));

  function_arg_info arg (type, true);
  if (pass_by_reference (NULL, arg))
    print_type (stream, build_pointer_type (type));
  else
    print_type (stream, type);
}

void
print_func_type (FILE *stream, const_tree type, bool for_block = false)
{
  tree result = TREE_TYPE (type);

  /* Taken from assign_parms_augmented_arg_list, please keep in sync */
  bool return_by_ref = aggregate_value_p (result, type);
  if (!for_block)
    {
      fprintf (stream, " (param");
      if (return_by_ref)
	print_func_frame_related (stream, build_pointer_type (result));
      tree arg;
      function_args_iterator it;
      FOREACH_FUNCTION_ARGS (type, arg, it)
      {
	print_func_frame_related (stream, arg);
      }
      if (stdarg_p (type) || !TYPE_ARG_TYPES (type))
	print_func_frame_related (stream, ptr_type_node);
      fprintf (stream, ")");
    }

  fprintf (stream, "%s(result", for_block ? "": " ");
  if (!return_by_ref)
    print_func_frame_related (stream, result);
  fprintf (stream, ")");
}

void print_global_type (FILE *stream, const_tree type)
{
  if (!TYPE_READONLY (type))
    fprintf (stream, " (mut");
  print_type (stream, type);
  if (!TYPE_READONLY (type))
    fprintf (stream, ")");
}

void
assemble_entity_import (FILE *stream, const char *name, const char *abi_name,
			const_tree decl)
{
  const_tree type = DECL_P (decl) ? TREE_TYPE (decl) : decl;

  output_indent (stream);
  fprintf (stream, "(import \"env\" \"");
  assemble_name (stream, abi_name);
  fprintf (stream, "\" " );
  switch (TREE_CODE (type))
    {
    case INTEGER_TYPE:
    case BOOLEAN_TYPE:
    case ENUMERAL_TYPE:
    case POINTER_TYPE:
    case OFFSET_TYPE:
    case RECORD_TYPE:
    case UNION_TYPE: {
      fprintf (stream, "(global $");
      assemble_name (stream, name);
      print_global_type (stream, type);
      break;
    }
    case FUNCTION_TYPE:
    case METHOD_TYPE: {
      fprintf (stream, "(func $");
      assemble_name (stream, name);
      fprintf (stream, " (@sym");
      if (type != decl) {
	assemble_init_prio (stream, decl);
	assemble_binding (stream, decl);
	assemble_visibility (stream, decl);
      }
      fprintf (stream, ")");

      print_func_type (stream, type);
      break;
    }
    default:
      gcc_unreachable ();
    }
  fprintf (stream, "))\n");
}

void
decl_end (FILE *stream)
{
  fprintf (stream, ")\n");
}

void
assemble_import (FILE *stream, const char *name, const_tree decl)
{
  if (!FUNC_OR_METHOD_TYPE_P (TREE_TYPE (decl)))
    return assemble_data_import (stream, decl, name);
  assemble_entity_import (stream, name, name, decl);
}

template<class T>
void
print_nan_payload (FILE *stream, const REAL_VALUE_TYPE *n)
{
  constexpr int ndata = sizeof (T) / 4;
  long data[ndata];
  real_to_target (data, n, float_mode_for_size (sizeof (T) * 8).require ());
  int mantissa = std::numeric_limits<T>::digits - 1;
  int mantissa_words = ((mantissa - 1) / 32) + 1;
  for (int i = mantissa_words; i != ndata; ++i)
    data[i] = 0;
  unsigned mask = (1 << (mantissa % 32)) - 1;
  data[mantissa / 32] &= mask;

  bool start = true;
  for (int i = ndata; i--;)
    if (start && !data[i])
      ;
    else if (start)
      fprintf (stream, "%lx", data[i]), start = false;
    else
      fprintf (stream, "%08lx", data[i]);
}

}

/* TARGET_ASM_INIT_SECTIONS */
void
wasm_asm_init_sections ()
{
  text_section = get_unnamed_section (SECTION_CODE, [](const char *){}, "T");
  data_section = get_unnamed_section (SECTION_WRITE, [](const char *){}, "D");
}

/* TARGET_ASM_FILE_START */
void
wasm_asm_file_start ()
{
  output_indent(asm_out_file);
  ++indent;
  fprintf (asm_out_file, "(module\n");
  saved_asm_out_file = asm_out_file;
  asm_out_file = open_memstream (&fake_out_file_data, &fake_out_file_length);
}

/* TARGET_ASM_FILE_END */
void
wasm_asm_file_end ()
{
  /* ??? All wasm globals should become builtin decls */
  output_indent (saved_asm_out_file);
  fprintf (saved_asm_out_file, "(import \"env\" \"__stack_pointer\""
	   " (global $stack (mut i32)))\n");

  output_indent (saved_asm_out_file);
  fprintf (saved_asm_out_file, "(import \"env\" \"memory\" (memory 0))\n");
  output_indent (saved_asm_out_file);
  fprintf (saved_asm_out_file, "(import \"env\" \"__indirect_function_table\" (table 0 funcref))\n");

  for (const auto &map_entry: import_map)
    {
      tree name = map_entry.second.first;
      const_tree decl = map_entry.second.second;
      assemble_import (saved_asm_out_file, IDENTIFIER_POINTER (name), decl);
    }
  for (auto sym: external_libcall_set)
    assemble_entity_import (saved_asm_out_file, XSTR (sym, 0), XSTR (sym, 0),
			    *external_libcalls.get (sym));

  fflush (asm_out_file);
  fwrite (fake_out_file_data, 1, fake_out_file_length, saved_asm_out_file);

  fprintf (saved_asm_out_file, ")\n");
}

/* ASM_OUTPUT_FUNCTION_LABEL */
void
wasm_asm_start_function (FILE *stream, tree decl, const char *name)
{
  output_indent(stream);
  ++indent;
  fprintf (stream, "(func $");
  assemble_name (stream, name);

  bool override_args = false;
  if (MAIN_NAME_P (DECL_NAME (decl)))
    {
      if (!cfun->machine->func_args)
	name = "__main_void";
      else
	name = "__main_argc_argv", override_args = true;
    }

  fprintf (stream, " (@sym");
  if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      assemble_init_prio (stream, decl);
      assemble_binding (stream, decl);
      assemble_visibility (stream, decl);
      assemble_sym_name (stream, name);
    }
  fprintf (stream, ") ");

  tree type = TREE_TYPE (decl);
  fprintf (stream, "\n");
  int i;
  rtx reg;

  if (override_args)
    {
      FOR_EACH_VEC_SAFE_ELT (cfun->machine->func_args, i, reg)
	  print_local_decl (stream, reg, i < 2);
      if (i < 1)
	{
	  output_indent (stream);
	  fprintf (stream, "(param i32)\n");
	}
      if (i < 2)
	{
	  output_indent (stream);
	  fprintf (stream, "(param i32)\n");
	}
    }
  else
    {
      FOR_EACH_VEC_SAFE_ELT (cfun->machine->func_args, i, reg)
	print_local_decl (stream, reg, true);
      if (stdarg_p (type))
	print_local_decl (stream, regno_reg_rtx[ARG_POINTER_REGNUM], true);
      if (DECL_STATIC_CHAIN (decl))
	print_local_decl (stream, regno_reg_rtx[STATIC_CHAIN_REGNUM], true);
    }
  output_indent (stream);
  print_func_type (stream, type, true);
  fprintf (stream, "\n");

  output_indent (stream);
  fprintf (stream, ";; hard\n");
  if (cfun->machine->return_mode != VOIDmode)
    print_local_decl (stream, gen_rtx_REG (cfun->machine->return_mode,
					   WASM_RETURN_REGNUM));

  df_set_regs_ever_live(WASM_CONTROL_POINTER_REGNUM, true);
  for (int r = 0; r < FIRST_PSEUDO_REGISTER; r++)
    {
      if (r == WASM_RETURN_REGNUM)
	continue;
      if (r == ARG_POINTER_REGNUM)
	continue;
      if (r == STATIC_CHAIN_REGNUM && DECL_STATIC_CHAIN (decl))
	continue;
      reg = regno_reg_rtx[r];
      if (df_regs_ever_live_p (r))
	{
	  if (!global_p (reg))
	    print_local_decl (stream, reg);
	}
    }

  output_indent (stream);
  fprintf (stream, ";; locals\n");
  int max = max_reg_num();
  for (int r = FIRST_PSEUDO_REGISTER; r < max; r++)
    {
      reg = regno_reg_rtx[r];
      if (!reg)
	continue;
      if (reg == const0_rtx)
	continue;
      if (vec_safe_contains (cfun->machine->func_args, reg))
	continue;
      if (!bitmap_bit_p (cfun->machine->regs_ever_live, r))
	continue;
      print_local_decl (stream, reg);
    }
  output_indent (stream);
  fprintf (stream, "(local.set $control (i32.const 0))\n");
  output_indent (stream);
  fprintf (stream, "(loop $control ");
  print_func_type (stream, type, true);
  fprintf (stream, "\n");
  ++indent;
  int len = labelno_to_labels.elements ();
  for (i = len - 1; i >= 0; --i)
    {
      output_indent (stream);
      ++indent;
      fprintf (stream, "(block $%d\n", i);
    }

  output_indent (stream);

  fprintf (stream, "(br_table");
  for (i = 0; i < len; ++i)
    fprintf (stream, " $%d", i);
  fprintf (stream, " (local.get $control))\n");
}

/* ASM_DECLARE_FUNCTION_SIZE */
void
wasm_asm_end_function (FILE *stream, tree, const char *name)
{
  --indent;
  output_indent (stream);
  fprintf (stream, ") ;; loop $control\n");
  --indent;
  output_indent (stream);
  fprintf (stream, ") ;;%s\n",
	   IDENTIFIER_POINTER (targetm.asm_out.mangle_assembler_name (name)));
}

/* TARGET_ASM_ASSEMBLE_UNDEFINED_DECL */
void
wasm_handle_import (FILE *, const char *name, const_tree decl)
{
  auto has_proto = [] (const_tree ty)
    {
      if (TREE_CODE (ty) != FUNCTION_DECL)
	return true;
      ty = TREE_TYPE (ty);
      return TYPE_ARG_TYPES (ty) || TYPE_NO_NAMED_ARGS_STDARG_P (ty);
    };
  bool existed = false;
  tree key = targetm.asm_out.mangle_assembler_name (name);
  auto &slot = import_map.get_or_insert (key, &existed);
  if (!existed || (!has_proto (slot.second) && has_proto (decl)))
    slot = {get_identifier (name), decl};
}

/* TARGET_ASM_ASSEMBLE_EXTERNAL_LIBCALL */
void
wasm_handle_libcall (rtx symbol)
{
  external_libcall_set.add (symbol);
}

/* TARGET_ASM_INTEGER */
bool
wasm_assemble_integer (rtx x, unsigned size, int align)
{
  return assemble_integer (asm_out_file, x, size, align);
}

/* TARGET_ASM_END_DECL */
void
wasm_assemble_decl_end ()
{
  return decl_end(asm_out_file);
}

/* ASM_OUTPUT_ASCII */
void
wasm_assemble_ascii (FILE *stream, const char *data, int len)
{
  return assemble_string (stream, data, len, false);
}

/* ASM_OUTPUT_SKIP */
void
wasm_assemble_skip (FILE *stream, unsigned HOST_WIDE_INT len)
{
  return assemble_zeroes (stream, len);
}

/* ASM_DECLARE_OBJECT_NAME */
/* ASM_DECLARE_CONSTANT_NAME */
void
wasm_assemble_data_begin (FILE *stream, tree decl, const char *name,
			  HOST_WIDE_INT size, HOST_WIDE_INT align, bool pub)
{
  if (!name)
    name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  if (!size)
    size = tree_to_uhwi (DECL_SIZE_UNIT (decl));
  output_indent(stream);
  /* ??? This is a crutch. We should know the alignment of the constant we're
     declaring, but ASM_DECLARE_CONSTANT_NAME doesn't supply it. Assume it's
     huge and move on for now. */
  align = align ? align : 1024;
  fprintf (stream, "(data (@sym (align %ld)) (i32.const 0) (@sym $", align);
  assemble_name (stream, name);
  if (decl)
    {
      assemble_binding (stream, decl);
    }
  else
    {
      if (!pub)
	fprintf (stream, " local");
    }
  assemble_visibility (stream, decl);
  assemble_sym_name (stream, name);
  fprintf (stream, " (size %ld)", size);
  fprintf (stream, ")");
}

/* ASM_OUTPUT_ALIGNED_DECL_COMMON */
/* ASM_OUTPUT_ALIGNED_DECL_LOCAL */
void
wasm_assemble_data_zeros (FILE *stream, tree decl, const char *name,
			  HOST_WIDE_INT size, HOST_WIDE_INT align, bool pub)
{
  wasm_assemble_data_begin (stream, decl, name, size, align, pub);
  wasm_assemble_skip (stream, size);
  decl_end(stream);
}

/* TARGET_PRINT_OPERAND_PUNCT_VALID_P */
bool
wasm_valid_punct_p (unsigned char code)
{
  return code == '#';
}

/* TARGET_PRINT_OPERAND */
void
wasm_print_operand (FILE *stream, rtx value, int mode)
{
  if (value && GET_CODE (value) == UNSPEC_VOLATILE)
    return wasm_print_operand (stream, XVECEXP (value, 0, 0), mode);

  if (mode == '#') /* print return */
    {
      if (TREE_TYPE (TREE_TYPE (cfun->decl)) != void_type_node)
	fprintf (stream, " (local.get $return)");
    }
  else if (mode == 'M') /* print local/global */
    if (global_p(value))
      fprintf (stream, "global");
    else
      fprintf (stream, "local");
  else if (mode == 'A') /* print an arglist */
    {
      gcc_assert (GET_CODE (value) == PARALLEL);
      int len = XVECLEN (value, 0);
      for (int i = 1; i < len; ++i)
	{
	  rtx arg = XVECEXP (value, 0, i);
	  gcc_assert (GET_CODE (arg) == USE);
	  rtx reg = XEXP (arg, 0);
	  gcc_assert (GET_CODE (reg) == REG);
	  fprintf (stream, " ");
	  wasm_print_operand (stream, reg, 'i');
	}
    }
  else if (mode == 'T') /* print a type */
    {
      if (GET_CODE (value) == PARALLEL)
	{
	  int len = XVECLEN (value, 0);
	  for (int i = 1; i < len; ++i)
	    {
	      rtx arg = XVECEXP (value, 0, i);
	      gcc_assert (GET_CODE (arg) == USE);
	      rtx reg = XEXP (arg, 0);
	      gcc_assert (GET_CODE (reg) == REG);
	      wasm_print_operand (stream, reg, mode);
	    }
	}
      else if (REG_P (value))
	print_type (stream, lang_hooks.types.type_for_mode (GET_MODE (value), 0));
      else
	gcc_unreachable ();
    }
  else switch (GET_CODE (value))
    {
    case MEM:
      {
	rtx addr = XEXP (value, 0);
	if (GET_CODE (addr) == PLUS)
	  {
	    fprintf (stream, "offset=%lu ", UINTVAL (XEXP (addr, 1)));
	    addr = XEXP (addr, 0);
	  }

	wasm_print_operand (stream, addr, 'i');
	break;
      }
    case CONST:
    case SYMBOL_REF:
      {
	rtx op = GET_CODE (value) == CONST ? XEXP (value, 0) : value;
	rtx offset = NULL_RTX;
	if (GET_CODE (op) == PLUS)
	  offset = XEXP (op, 1), op = XEXP (op, 0);
	tree decl = SYMBOL_REF_P (op) ? SYMBOL_REF_DECL (op) : NULL_TREE;
	bool fndecl_p = decl && TREE_CODE (decl) == FUNCTION_DECL;
	if (mode == 'i')
	  fprintf (stream, "(i32.const 0 (@reloc %s ",
		   fndecl_p ? "functable" : "data");

	fprintf (stream, "$");
	output_addr_const (stream, op);
	if (offset)
	  {
	    fprintf (stream, " %+" HOST_WIDE_INT_PRINT "d",
		     INTVAL (offset));
	  }
	if (mode == 'i')
	  fprintf (stream, "))");
	break;
      }
    case SUBREG:
      gcc_assert (SUBREG_BYTE (value) == 0);
      wasm_print_operand (stream, SUBREG_REG (value), mode);
      break;
    case REG:
      {
	int reg = REGNO (value);
	if (mode == 'i' || mode == 'o')
	  fprintf (stream, "%s%s.%s ", mode == 'o' ? "" : "(",
		   global_p (value) ? "global" : "local",
		   mode == 'i' ? "get" : "set");
	if (reg < FIRST_PSEUDO_REGISTER)
	  fprintf (stream, "%s", reg_names[reg]);
	else
	  {
	    reg -= FIRST_PSEUDO_REGISTER;
	    fprintf (stream, "$local_%d", reg);
	  }
	if (mode == 'i')
	  fprintf (stream, ")");
	break;
      }
    case CONST_INT:
      if (mode == 'i')
	fprintf (stream, "(i32.const ");
      fprintf (stream, "%ld", INTVAL (value));
      if (mode == 'i')
	fprintf (stream, ")");
      break;
    case CONST_DOUBLE:
      {
	if (mode == 'i')
	  fprintf (stream, "(f%d.const ", GET_MODE_PRECISION (GET_MODE (value)));
	const REAL_VALUE_TYPE *n = CONST_DOUBLE_REAL_VALUE (value);
	if (REAL_VALUE_ISINF (*n))
	  fprintf (stream, "%cinf", REAL_VALUE_NEGATIVE (*n) ? '-' : '+');
	else if (REAL_VALUE_ISNAN (*n))
	  {
	    fprintf (stream, "%cnan", REAL_VALUE_NEGATIVE (*n) ? '-' : '+');
	    if (!n->canonical)
	      {
		fprintf (stream, ":0x");
		switch (GET_MODE (value))
		  {
		  case DFmode:
		    print_nan_payload<double> (stream, n);
		    break;
		  case SFmode:
		    print_nan_payload<float> (stream, n);
		    break;
		  default:
		    gcc_unreachable ();
		  }
	      }
	  }
	else
	  {
	    /* There are always 32 bits in each long, no matter the size of
	       the hosts long.  */
	    long tmp[2];
	    REAL_VALUE_TO_TARGET_DOUBLE (*n, tmp);
	    int32_t tmp2[2] = {(int32_t)tmp[0], (int32_t)tmp[1]};
	    double r;
	    std::memcpy (&r, tmp2, sizeof r);
	    fprintf (stream, "%a", r);
	  }
	if (mode == 'i')
	  fprintf (stream, ")");
	break;
      }
    default:
      gcc_unreachable ();
    }
}

static int get_cf_label (size_t no)
{
  rtx_insn **l = labelno_to_labels.get (no);
  if (!l)
    return -1;
  int cf = *labels_to_cfno.get (*l);
  gcc_assert (cf >= 0);
  return cf;
}

void
wasm_generate_internal_label (char *buf, const char *pfx, size_t no)
{
  if (!strcmp (pfx, "L"))
    sprint_ul (buf, get_cf_label (no));
  else
    sprintf (buf, "%s" HOST_WIDE_INT_PRINT_DEC, pfx, no);
}

void
wasm_output_internal_label (FILE *stream, const char *pfx, size_t no)
{
  if (!strcmp (pfx, "L"))
    {
      int l = get_cf_label (no);
      if (l >= 0)
	{
	  --indent;
	  output_indent (stream);
	  fprintf (stream, ") ;; $%d\n", l);
	}
    }
  else if (!strcmp (pfx, "LFB"))
    /* Function begin */;
  else if (!strcmp (pfx, "LFE"))
    /* Function end */;
  else
    /* unknown */;
}
