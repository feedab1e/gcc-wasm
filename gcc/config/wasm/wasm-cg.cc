/* WebAssembly code generation utilities.
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
#include "cfghooks.h"

hash_map<const_rtx, tree> external_libcalls;

namespace
{

rtx
unify_mem (rtx mem)
{
  HOST_WIDE_INT offset = 0;
  machine_mode mode = GET_MODE (mem);
  if (SUBREG_P (mem) && MEM_P(SUBREG_REG (mem)))
    {
      offset += SUBREG_BYTE (mem);
      mem = SUBREG_REG (mem);
    }
  if (!MEM_P (mem))
    return mem;
  rtx addr = XEXP (mem, 0);
  if (GET_CODE (addr) == PLUS && CONST_INT_P (XEXP (addr, 1)))
    {
      rtx offset_rtx = XEXP (addr, 1);
      addr = XEXP (addr, 0);
      offset += INTVAL (offset_rtx);
    }
  addr = force_reg (Pmode, addr);
  addr = gen_rtx_PLUS (Pmode, addr, gen_rtx_CONST_INT (Pmode, offset));
  if (offset < 0)
    addr = gen_rtx_PLUS (Pmode, force_reg (Pmode, addr), const0_rtx);
  mem = copy_rtx (mem);
  XEXP (mem, 0) = addr;
  return mem;
}

bool symref_p (rtx expr)
{
  if (GET_CODE(expr) == CONST)
    {
      expr = XEXP (expr, 0);
      if (GET_CODE (expr) == PLUS)
	expr = XEXP (expr, 0);
    }
  return GET_CODE (expr) == SYMBOL_REF;
}

void
record_libcall (const_rtx sym, tree ret)
{
  auto_vec<tree> args;
  vec<rtx, va_gc> *args_rtx = cfun->machine->call->args;
  int cnt = vec_safe_length (args_rtx);
  args.safe_grow (cnt);
  for (int i = 0; i != cnt; ++i)
    args[i] = lang_hooks.types.type_for_mode (GET_MODE ((*args_rtx)[i]), false);
  tree ty = build_function_type_array (ret, cnt, args.address ());
  bool existed;
  tree &slot = external_libcalls.get_or_insert (sym, &existed);
  if (!existed)
    slot = ty;
  gcc_assert (slot == ty);
}

}

machine_mode
wasm_real_register_mode (rtx reg)
{
  if (SUBREG_P (reg))
    reg = SUBREG_REG (reg);
  if (!REG_P (reg))
    return VOIDmode;
  machine_mode m = GET_MODE (reg);
  PROMOTE_MODE (m, 0, NULL_TREE);
  return m;
}

static bool
expand_const (rtx dest, rtx src, machine_mode mode)
{
  if (!const_int_operand (src, mode) && !const_double_operand (src, mode)
      && GET_CODE (src) != LABEL_REF && !symref_p (src))
    return false;
  if (symref_p (src))

    if (GET_CODE (src) == CONST)
      if (GET_CODE (XEXP (src, 0)) == PLUS)
	if (GET_CODE (XEXP (XEXP (src, 0), 1)) == CONST_INT)
	{
	  tree symref_dcl = SYMBOL_REF_DECL (XEXP (XEXP (src, 0), 0));
	  if (INTVAL (XEXP (XEXP (src, 0), 1)) <= 0)
	    src = force_reg (Pmode, XEXP (src, 0));
	  else if (symref_dcl && TREE_CODE (symref_dcl) == FUNCTION_DECL)
	    src = force_reg (Pmode, XEXP (src, 0));
	}

  emit_insn (gen_rtx_SET (dest, src));
  return true;
}

bool
wasm_expand_mov (rtx dest, rtx src, machine_mode mode)
{
  temporary_volatile_ok g {1};
  dest = unify_mem (dest);
  src = unify_mem (src);
  if (register_operand (dest, mode) && immediate_operand (src, mode))
    return expand_const (dest, src, mode);
  gcc_assert (REG_P (dest) || MEM_P (dest) || SUBREG_P (dest));
  if (register_operand (dest, mode) && memory_operand (src, mode))
    {
      emit_insn (gen_rtx_SET (dest, src));
      return true;
    }
  if (!SUBREG_P (src))
    src = force_reg (mode, src);
  emit_insn (gen_rtx_SET (dest, src));
  return true;
}

void
wasm_expand_conv (rtx dest, rtx src, rtx_code code, bool strict)
{
  if (code == TRUNCATE)
    code = LOAD_EXTEND_OP (VOIDmode);

  auto real_reg = [](rtx reg)
    {
      if (!SUBREG_P (reg))
	return reg;
      gcc_assert (SUBREG_BYTE (reg) == 0);
      return SUBREG_REG (reg);
    };
  rtx r_src = real_reg (src), r_dest = real_reg (dest);

  /* This is an intra-reg conversion */
  bool src_di_p = (GET_MODE (r_src) == DImode);
  bool dest_di_p = (GET_MODE (r_dest) == DImode);
  bool do_conv = src_di_p != dest_di_p;
  bool do_intra = (GET_MODE (src) != SImode && GET_MODE (src) != DImode)
                   || (GET_MODE (src) != DImode && (dest_di_p && src_di_p));
  machine_mode mid_mode = src_di_p ? DImode : SImode;

  rtx mid = do_conv
	    ? do_intra
	      ? gen_reg_rtx (mid_mode)
	      : src
	    : simplify_gen_subreg (mid_mode, dest, GET_MODE (r_dest), 0);
  if (do_intra)
    {
      if (GET_MODE_PRECISION (GET_MODE (src))
	  >= GET_MODE_PRECISION (GET_MODE (mid)))
	src = simplify_gen_subreg (mid_mode, src, GET_MODE (r_src), 0);
      rtx op = gen_rtx_fmt_e_stat (code, mid_mode, src);
      rtx_insn *i = emit_insn (gen_rtx_SET (mid, op));
      extract_insn (i);
    }

  if (do_conv)
    {
      if (mid_mode == DImode)
	code = TRUNCATE;
      rtx op = gen_rtx_fmt_e_stat (code, GET_MODE (r_dest), mid);
      rtx_insn *i = emit_insn (gen_rtx_SET (r_dest, op));
      extract_insn (i);
    }
}

void
wasm_expand_call (rtx retval, rtx fn, rtx aux)
{
  bool vararg_p = false;
  bool has_proto = true;
  bool is_fn = true;
  bool need_chain = false;

  if (tree type = cfun->machine->call->type)
    {
      vararg_p = stdarg_p (type);
      has_proto = TYPE_ARG_TYPES (type) || vararg_p;
    }

  if (GET_CODE (XEXP (fn, 0)) == SYMBOL_REF)
    if (tree decl = SYMBOL_REF_DECL (XEXP (fn, 0)))
      {
	if (TREE_CODE (decl) != FUNCTION_DECL)
	is_fn = false;
	else if (DECL_STATIC_CHAIN (decl))
	need_chain = true;
      }

  /* Pretend unprototyped fn calls dynamic. That way we can assert the function
     type ourselves, and if they don't agree with whatever is actually supplied
     by the linker, ANSI C 3.3.2.2 says it's undefied anyway */
  if (!has_proto || !is_fn)
    {
      rtvec ops = gen_rtvec (1, force_reg (Pmode, XEXP (fn, 0)));
      XEXP (fn, 0) = gen_rtx_UNSPEC_VOLATILE (SImode, ops,
				      UNSPECV_CALL_ADDRESS_BARRIER);
    }

  rtx call = gen_rtx_CALL(VOIDmode, fn, aux);
  if (retval)
    call = gen_rtx_SET (retval, call);

  auto *args_rtx = cfun->machine->call->args;

  /* ??? This should be handled by the call machinery, not me! */
  if (vararg_p)
    {
      rtx stdarg_reg = gen_reg_rtx (SImode);
      /* Rely on args being pushed in reverse order, and on only varargs being
	 pushed, so that the beginning of the vararg buffer is stack_pointer_rtx.
	 This is a hack. */
      emit_move_insn (stdarg_reg, stack_pointer_rtx);
      vec_safe_insert (args_rtx, 0, stdarg_reg);
    }
  if (need_chain)
    {
      rtx chain_reg = gen_reg_rtx (SImode);
      emit_move_insn (chain_reg, regno_reg_rtx[STATIC_CHAIN_REGNUM]);
      vec_safe_insert (args_rtx, 0, chain_reg);
    }

  int argc = vec_safe_length (args_rtx);
  rtx res = gen_rtx_PARALLEL (VOIDmode, rtvec_alloc (argc + 1));
  XVECEXP (res, 0, 0) = call;
  for (int i = 0; i < argc; ++i)
    XVECEXP (res, 0, argc - i) = gen_rtx_USE (VOIDmode, (*args_rtx)[i]);
  emit_call_insn (res);
}

void
wasm_expand_prologue ()
{
  emit_move_insn (regno_reg_rtx[WASM_BASE_POINTER_REGNUM],
		stack_pointer_rtx);
  if (crtl->stack_realign_needed)
    {
      int align = ~(crtl->max_used_stack_slot_alignment / BITS_PER_UNIT - 1);
      rtx align_mask = gen_rtx_CONST_INT (SImode, align);
      rtx reg = gen_reg_rtx (SImode);
      emit_move_insn (reg, align_mask);
      emit_insn (gen_andsi3 (stack_pointer_rtx, stack_pointer_rtx, reg));
    }

  if (flag_stack_usage_info)
    current_function_static_stack_size = 0;
  if (poly_int64 stack_space = get_frame_size ())
    {
      HOST_WIDE_INT unit_boundary = STACK_BOUNDARY / BITS_PER_UNIT;
      poly_int64 aligned = aligned_upper_bound (stack_space, unit_boundary);
      rtx aligned_space_rtx = gen_int_mode (aligned, Pmode);
      emit_insn (gen_sub2_insn (stack_pointer_rtx, aligned_space_rtx));
      emit_move_insn (frame_pointer_rtx, stack_pointer_rtx);
      if (flag_stack_usage_info)
	current_function_static_stack_size = aligned;
    }
  if (HOST_WIDE_INT stack_space = crtl->outgoing_args_size)
    {
      emit_insn (gen_sub2_insn (stack_pointer_rtx,
			  gen_int_mode (stack_space, Pmode)));
    }
}

void
wasm_expand_epilogue ()
{
  emit_move_insn (stack_pointer_rtx, regno_reg_rtx[WASM_BASE_POINTER_REGNUM]);
}

void
wasm_expand_compare (machine_mode m, rtx res, rtx op)
{
  rtx *left_p = &XEXP (op, 0), *right_p = &XEXP (op, 1);

  *left_p = force_reg (m, *left_p);
  *right_p = force_reg (m, *right_p);

  rtx left = *left_p, right = *right_p;

  switch (GET_CODE (op))
    {
    case ORDERED:
      {
	rtx eq = gen_reg_rtx (SImode), ne = gen_reg_rtx (SImode);
	wasm_expand_compare (m, eq, gen_rtx_LE (SImode, left, right));
	wasm_expand_compare (m, ne, gen_rtx_GE (SImode, left, right));
	emit_insn (gen_iorsi3 (res, eq, ne));
	break;
      }
    case LTGT:
      {
	rtx eq = gen_reg_rtx (SImode), ne = gen_reg_rtx (SImode);
	wasm_expand_compare (m, eq, gen_rtx_LT (SImode, left, right));
	wasm_expand_compare (m, ne, gen_rtx_GT (SImode, left, right));
	emit_insn (gen_iorsi3 (res, eq, ne));
	break;
      }
    case LTU:
    case GTU:
    case LEU:
    case GEU:
    case LT:
    case GT:
    case LE:
    case GE:
    case EQ:
    case NE:
      op = gen_rtx_fmt_ee (GET_CODE (op), SImode, left, right);
      emit_insn (gen_rtx_SET (res, op));
      break;
    case UNLT:
    case UNGT:
    case UNLE:
    case UNGE:
    case UNEQ:
    case UNORDERED:
      {
	rtx_code cond_code = reverse_condition_maybe_unordered (GET_CODE (op));
	rtx cond = gen_rtx_fmt_ee (cond_code, SImode, left, right);
	rtx mid = gen_reg_rtx (SImode);
	wasm_expand_compare (m, mid, cond);
	emit_insn (gen_xorsi3 (res, mid,
			       gen_rtx_CONST_INT (SImode, STORE_FLAG_VALUE)));
	break;
      }
    default:
      gcc_unreachable ();
    }
}

bool
wasm_regno_mode_ok (unsigned regno, machine_mode mode)
{
  if (COMPLEX_MODE_P (mode))
    return false;
  if (regno == WASM_CONTROL_POINTER_REGNUM)
    return mode == SImode;
  if (regno == ARG_POINTER_REGNUM
      || regno == STACK_POINTER_REGNUM
      || regno == FRAME_POINTER_REGNUM
      || regno == STATIC_CHAIN_REGNUM
      || regno == WASM_BASE_POINTER_REGNUM)
    return mode == Pmode;
  if (GET_MODE_CLASS (mode) == MODE_INT)
    return mode < TImode;
  if (GET_MODE_CLASS (mode) == MODE_FLOAT)
    return mode == DFmode || mode == SFmode;
  return false;
}

bool
wasm_can_change_mode_class (machine_mode from, machine_mode to, reg_class_t)
{
  return QImode <= from && from <= SImode && QImode <= to && to <= SImode;
}

/* TARGET_LEGITIMATE_ADDRESS_P */
bool
wasm_legitimate_address_p (machine_mode, rtx x, bool, code_helper)
{
  if (GET_CODE (x) == CONST)
    x = XEXP (x, 0);
  if (GET_CODE (x) == PLUS && CONST_INT_P (XEXP (x, 1)) &&
      INTVAL (XEXP (x, 1)) >= 0)
    x = XEXP (x, 0);
  switch (GET_CODE (x))
    {
    case SYMBOL_REF:
    case LABEL_REF:
    case CONST_INT:
    case REG:
      return true;
    default:
      return false;
    }
}

/* TARGET_MARK_ARG_REGNOS */
void
wasm_mark_arg_regnos (bitmap regnos)
{
  for (rtx arg: cfun->machine->func_args)
    bitmap_set_bit (regnos, REGNO(arg));
}

/* TARGET_START_CALL_ARGS */
void
wasm_start_call_args (cumulative_args_t args)
{
  cfun->machine->call = get_cumulative_args (args);
}

/* TARGET_CALL_ARGS */
void
wasm_call_args (cumulative_args_t args, rtx arg, tree type)
{
  get_cumulative_args (args)->type = type;
  if (arg == pc_rtx)
    return;

if (GET_CODE (arg) == PARALLEL)
  {
    rtvec elts = XVEC (arg, 0);
    for (int i = 0; i != GET_NUM_ELEM (elts); ++i)
      wasm_call_args (args, XEXP (RTVEC_ELT (elts, i), 0), type);
    return;
  }
  gcc_assert (REG_P (arg));
  vec_safe_push (get_cumulative_args (args)->args, arg);
}

/* TARGET_END_CALL_ARGS */
void
wasm_end_call_args (cumulative_args_t)
{
  cfun->machine->call = NULL;
}

/* TARGET_FUNCTION_ARG_ADVANCE */
void
wasm_function_arg_advance (cumulative_args_t, const function_arg_info &)
{
}

/* TARGET_FUNCTION_ARG_BOUNDARY */
unsigned int wasm_vararg_align (machine_mode mode, const_tree type)
{
  unsigned align = 0;

  if (type)
    align = TYPE_ALIGN (type);
  if (align)
    return MIN (MAX (align, BITS_PER_UNIT), BIGGEST_ALIGNMENT);
  return get_mode_alignment (mode);
}

/* TARGET_FUNCTION_VALUE */
rtx
wasm_function_value (const_tree type, const_tree ARG_UNUSED (func),
		     bool outgoing)
{
  machine_mode mode = TYPE_MODE (type);
  if (!cfun)
    /* fake return regnum since none is actually available */
    return gen_rtx_REG (mode, WASM_RETURN_REGNUM);
  if (outgoing)
    {
      /* Where we actually put the actual return val */
      cfun->machine->return_mode = mode;
      return gen_rtx_REG (mode, WASM_RETURN_REGNUM);
    }
  if (!cfun->machine->call)
    /* fake return regnum again, not in a call */
    return gen_rtx_REG (mode, WASM_RETURN_REGNUM);

  /* Put the retval in a fresh reg, its mode may be different from actual
     retval, and we can't have that */
  return gen_reg_rtx (mode);
}

/* TARGET_FUNCTION_ARG */
rtx
wasm_function_arg (cumulative_args_t cum_v, const function_arg_info &arg)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);

  if (arg.end_marker_p ())
    return NULL_RTX;

  if (!arg.named)
    return NULL_RTX;

  if (arg.mode == TImode)
    {
      rtx const8_rtx = gen_rtx_CONST_INT (VOIDmode, 8);
      rtvec para = gen_rtvec(2,
	gen_rtx_EXPR_LIST (DImode, gen_reg_rtx (DImode), const0_rtx),
	gen_rtx_EXPR_LIST (DImode, gen_reg_rtx (DImode), const8_rtx));
      return gen_rtx_PARALLEL (TImode, para);
    }
  return gen_reg_rtx (arg.mode);
}

/* TARGET_FUNCTION_INCOMING_ARG */
rtx
wasm_function_incoming_arg (cumulative_args_t cum_v,
			    const function_arg_info &arg)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);

  auto maybe_push_arg = [cum](rtx arg)
    {
      if (cum->incoming)
	vec_safe_push (cum->args, arg);
    };

  if (arg.end_marker_p ())
    {
      gcc_assert (!cfun->machine->func_args);
      if (cum->incoming)
	cfun->machine->func_args = cum->args;
      return gen_rtx_CONST_INT (SImode, 80);
    }
  if (!arg.named)
    return NULL_RTX;

  /* Int128 is passed as 2xint64_t, in LE order */
  if (arg.mode == TImode)
    {
      rtx const8_rtx = gen_rtx_CONST_INT (VOIDmode, 8);

      rtx lsb = gen_rtx_EXPR_LIST (DImode, gen_reg_rtx (DImode), const0_rtx);
      rtx msb = gen_rtx_EXPR_LIST (DImode, gen_reg_rtx (DImode), const8_rtx);
      maybe_push_arg (lsb);
      maybe_push_arg (msb);
      rtvec para = gen_rtvec (2, lsb, msb);
      return gen_rtx_PARALLEL (TImode, para);
    }
  rtx reg = gen_reg_rtx (arg.mode);
  maybe_push_arg (reg);
  return reg;
}

/* TARGET_LIBCALL_VALUE */
rtx wasm_libcall_value (machine_mode m, const_rtx sym)
{
  tree rtype = lang_hooks.types.type_for_mode (m, false);
  if (cfun && cfun->machine->call)
    record_libcall (sym, rtype);
  return wasm_function_value (rtype, NULL_TREE, false);
}

/* TARGET_PASS_BY_REFERENCE */
bool
wasm_pass_by_reference (cumulative_args_t, const function_arg_info &arg)
{
  if (arg.type)
    {
      if (TREE_CODE (arg.type) == COMPLEX_TYPE)
	return true;
      if (TREE_CODE (arg.type) == VECTOR_TYPE)
	return true;
    }
  if (COMPLEX_MODE_P (arg.mode))
    return true;
  if (VECTOR_MODE_P (arg.mode))
    return true;
  if (arg.aggregate_type_p ())
    return true;
  return false;
}

/* TARGET_RETURN_IN_MEMORY */
bool
wasm_return_in_memory (const_tree type, const_tree ARG_UNUSED (fntype))
{
  if (TREE_CODE (type) == COMPLEX_TYPE)
    return true;
  if (TREE_CODE (type) == VECTOR_TYPE)
    return true;
  if (VECTOR_MODE_P (TYPE_MODE (type)))
    return true;
  if (COMPLEX_MODE_P (TYPE_MODE (type)))
    return true;
  return false;
}
