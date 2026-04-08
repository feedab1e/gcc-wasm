(include "attrs.md")

(define_insn "nop"
  [(const_int 0)]
  ""
  "(nop)"
  [])

(define_predicate "symbol_operand"
  (match_code "symbol_ref"))
(define_predicate "funcref_operand"
  (match_code "symbol_ref")
  {
    gcc_assert (GET_CODE (op) == SYMBOL_REF);
    return !SYMBOL_REF_DECL (op) || TREE_CODE (SYMBOL_REF_DECL (op)) == FUNCTION_DECL;
  })
(define_predicate "varref_operand"
  (match_code "symbol_ref")
  {
    gcc_assert (GET_CODE (op) == SYMBOL_REF);
    return TREE_CODE (SYMBOL_REF_DECL (op)) == VAR_DECL;
  })
(define_predicate "subregister_for_si_operand"
  (ior (match_code "reg") (match_code "subreg"))
  {
    rtx x = op;
    if (SUBREG_P (x))
      x = SUBREG_REG (x);
    if (!REG_P (x))
      return false;
    machine_mode m = GET_MODE (x);
    int sign = 0;
    PROMOTE_MODE (m, sign, NULL_TREE);
    return m == SImode;
  })

(define_predicate "subregister_for_di_operand"
  (ior (match_code "reg") (match_code "subreg"))
  {
    rtx x = op;
    if (SUBREG_P (x))
      x = SUBREG_REG (x);
    if (!REG_P (x))
      return false;
    machine_mode m = GET_MODE (x);
    int sign = 0;
    PROMOTE_MODE (m, sign, NULL_TREE);
    return m == DImode;
  })
(define_predicate "wasm_register_operand"
  (match_operand 0 "register_operand")
  {
    machine_mode m = GET_MODE (op);
    PROMOTE_MODE (m, 0, NULL_RTX);
    if (mode == TImode)
      return false;
    return m == wasm_real_register_mode (op);
  })
(define_predicate "wasm_call_register_operand"
  (ior (match_operand 0 "wasm_register_operand")
       (match_test "GET_CODE (op) == UNSPEC_VOLATILE && wasm_register_operand (XVECEXP (op, 0, 0), mode)")))
(define_predicate "wasm_subregister_operand"
  (match_operand 0 "register_operand")
  {
    machine_mode m = wasm_real_register_mode (op);
    return mode != TImode;
  })

(define_predicate "immediate_or_register_operand"
  (ior (match_operand 0 "immediate_operand")
       (match_operand 0 "register_operand")))

(define_predicate "wasm_offset_operand"
  (match_operand 0 "const_int_operand")
  {
    return INTVAL (op) >= 0;
  })

 (define_expand "mov<mode>"
  [(set (match_operand:QHSDISDF 0 "nonimmediate_operand" "")
	(match_operand:QHSDISDF 1 "general_operand" ""))]
  ""
  {
    if (wasm_expand_mov (operands[0], operands[1], <QHSDISDF:MODE>mode))
      DONE;
    else FAIL;
  })

(define_expand "jump"
  [(set (pc)
  (label_ref (match_operand 0 "" "")))]
  ""
{
})

(define_predicate "call_operation"
  (match_code "parallel")
{
  int arg_end = XVECLEN (op, 0);

  for (int i = 1; i < arg_end; i++)
    {
      rtx elt = XVECEXP (op, 0, i);

      if (GET_CODE (elt) != USE || !REG_P (XEXP (elt, 0)))
        return false;
    }
  return true;
})
(define_insn "speculation_barrier" [(const_int 0)] "" "")
(define_expand "prologue"
  [(const_int 777)]
  ""
  {
    wasm_expand_prologue ();
    DONE;
  })
(define_expand "epilogue"
  [(const_int 777)]
  ""
  {
    wasm_expand_epilogue ();
    emit_jump_insn (gen_return ());
    DONE;
  })

;; Calls
(define_expand "call"
  [(call (match_operand:SI 0 "memory_operand" "m")
         (match_operand:SI 1 "" ""))]
  ""
  {
    wasm_expand_call(NULL_RTX, operands[0], operands[1]);
    DONE;
  })

(define_expand "call_value"
  [(set (match_operand 0 "register_operand" "=r")
    (call (match_operand:SI 1 "memory_operand" "m")
          (match_operand:SI 2 "" "")))]
  ""
  {
    wasm_expand_call(operands[0], operands[1], operands[2]);
    DONE;
  })

(define_insn "*call_internal_indirect"
  [(match_parallel 2 "call_operation"
    [(call (mem:SI (match_operand:P 0 "wasm_call_register_operand"))
	       (match_operand 1))])]
  ""
  "(call_indirect (param%T2) (result)%A2 (%M0.get %0))")

(define_insn "*call_value_internal_indirect"
  [(match_parallel 3 "call_operation"
    [(set (match_operand 0 "wasm_register_operand" "")
	      (call (mem:SI (match_operand:P 1 "wasm_call_register_operand"))
		        (match_operand 2)))])]
  ""
  "(%M0.set %0 (call_indirect (param%T3) (result%T0)%A3 (%M1.get %1)))")

(define_insn "*call_internal"
  [(match_parallel 2 "call_operation"
    [(call (mem:SI (match_operand:P 0 "funcref_operand"))
	       (match_operand 1))])]
  ""
  "(call %0%A2)")

(define_insn "*call_value_internal"
  [(match_parallel 3 "call_operation"
    [(set (match_operand 0 "wasm_register_operand" "")
	      (call (mem:SI (match_operand:P 1 "funcref_operand"))
		        (match_operand 2)))])]
  ""
  "(%M0.set %0 (call %1%A3))")

;; Literals
(define_insn "*local_const<mode>"
  [(set (match_operand:QHSDI 0 "register_operand")
	    (match_operand:QHSDI 1 "const_int_operand" ""))]
  ""
  "(%M0.set %0 (<QHSDI:promote_type>.const %1))")

(define_insn "*local_const<mode>"
  [(set (match_operand:F 0 "register_operand")
	    (match_operand:F 1 "immediate_operand" ""))]
  ""
  "(%M0.set %0 (<types>.const %1))")

;; Address taking
(define_insn "*local_const_addr"
  [(set (match_operand:SI 0 "register_operand")
	    (match_operand:SI 1 "immediate_operand"))]
  ""
  "(%M0.set %0 %i1)")

;; Moves
(define_insn "*local_move<mode>"
  [(set (match_operand:QHSDISDF 0 "wasm_subregister_operand" "")
        (match_operand:QHSDISDF 1 "wasm_subregister_operand" ""))]
  "wasm_real_register_mode (operands[0]) == wasm_real_register_mode (operands[1])"
  "(%M0.set %0 (%M1.get %1))")

(define_insn "*local_move<mode>"
  [(set (match_operand:QHSDI 0 "subregister_for_di_operand" "")
        (match_operand:QHSDI 1 "subregister_for_si_operand" ""))]
  ""
  "(%M0.set %0 (i64.extend_i32_s (%M1.get %1)))")

(define_insn "*local_move<mode>"
  [(set (match_operand:QHSDI 0 "subregister_for_si_operand" "")
        (match_operand:QHSDI 1 "subregister_for_di_operand" ""))]
  ""
  "(%M0.set %0 (i32.wrap_i64 (%M1.get %1)))")

;; Comparisons
(define_expand "cstore<mode>4"
  [(set (match_operand:SI 0 "register_operand" "")
        (match_operator 1 "comparison_operator"
		        [(match_operand:REGF 2 "general_operand" "")
			(match_operand:REGF 3 "general_operand" "")]))]
  ""
  {
    wasm_expand_compare (<MODE>mode, operands[0], operands[1]);
    DONE;
  })

(define_insn "*test<code>_<mode>"
  [(set (match_operand:SI 0 "wasm_register_operand")
        (frelop:SI (match_operand:F 1 "wasm_register_operand")
		   (match_operand:F 2 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<types>.<opnamef> (%M1.get %1) (%M2.get %2)))")

(define_insn "*test<code>_<mode>"
  [(set (match_operand:SI 0 "wasm_register_operand")
        (irelop:SI (match_operand:REG 1 "wasm_register_operand")
	           (match_operand:REG 2 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<types>.<opname> (%M1.get %1) (%M2.get %2)))")

;; Control flow
(define_insn "return" [(return)] "" "(return%#)")
(define_insn "trap" [(trap_if (const_int 1) (const_int 0))] "" "(unreachable)")

(define_expand "cbranch<mode>4"
  [(set (pc)
	(if_then_else (match_operator 0 "comparison_operator"
				      [(match_operand:REGF 1 "general_operand" "")
				       (match_operand:REGF 2 "general_operand" "")])
		      (label_ref (match_operand 3 "" ""))
		      (pc)))]
  ""
  {
    rtx cond = gen_reg_rtx (SImode);
    emit_insn (gen_cstore<mode>4 (cond, operands[0], operands[1], operands[2]));
    emit_jump_insn (gen_branch (operands[3], cond));
    DONE;
  })

(define_insn "branch"
  [(set (pc)
	(if_then_else (ne (match_operand:SI 1 "wasm_register_operand" "")
	                  (const_int 0))
		      (label_ref (match_operand 0 "" ""))
		      (pc)))]
  ""
  "(br_if $control (local.set $control (i32.const %l0)) (%M1.get %1))")

(define_insn "*jump"
  [(set (pc) (label_ref (match_operand 0 "" "")))]
  ""
  "(br $control (local.set $control (i32.const %l0)))")

;; Int memops
(define_insn "*store<mode>"
  [(set (match_operand:REGF 0 "memory_operand")
        (match_operand:REGF 1 "wasm_register_operand"))]
  ""
  "(<types>.store %m0 %i1)")

(define_insn "*load<mode>"
  [(set (match_operand:REGF 0 "wasm_register_operand")
        (match_operand:REGF 1 "memory_operand"))]
  ""
  "(%o0 (<types>.load %m1))")

;; I64 subreg memops
(define_insn "*store<SUBREGDI:mode>"
  [(set (match_operand:SUBREGDI 0 "memory_operand")
        (match_operand:SUBREGDI 1 "subregister_for_di_operand"))]
  ""
  "(i64.store<SUBREGDI:size> %m0 %i1)")

(define_insn "*load<SUBREGDI:mode>"
  [(set (match_operand:SUBREGDI 0 "subregister_for_di_operand")
        (match_operand:SUBREGDI 1 "memory_operand"))]
  ""
  "(%o0 (i64.load<SUBREGSI:size>_s %m1))")

;; I32 subreg memops
(define_insn "*store<SUBREGSI:mode>"
  [(set (match_operand:SUBREGSI 0 "memory_operand" "")
        (match_operand:SUBREGSI 1 "subregister_for_si_operand" ""))]
  ""
  "(i32.store<SUBREGSI:size> %m0 %i1)")

(define_insn "*load<SUBREGSI:mode>"
  [(set (match_operand:SUBREGSI 0 "subregister_for_si_operand")
        (match_operand:SUBREGSI 1 "memory_operand"))]
  ""
  "(%o0 (i32.load<SUBREGSI:size>_s %i1))")

;; Binary integer
(define_expand "<opname_rt><mode>3"
  [(set (match_operand:REG 0 "register_operand" "")
	(ibinop:REG (match_operand:REG 1 "immediate_or_register_operand" "")
                (match_operand:REG 2 "immediate_or_register_operand" "")))]
  ""
  {
    if (!subregister_for_<REG:mode>_operand(operands[1], <REG:MODE>mode))
      operands[1] = force_reg (<REG:MODE>mode, operands[1]);
    if (!subregister_for_<REG:mode>_operand(operands[2], <REG:MODE>mode))
      operands[2] = force_reg (<REG:MODE>mode, operands[2]);
  })

(define_insn "*<opname_rt><mode>3"
  [(set (match_operand:REG 0 "wasm_register_operand")
	    (ibinop:REG (match_operand:REG 1 "wasm_register_operand")
                    (match_operand:REG 2 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<types>.<ibinop:opname> (%M1.get %1) (%M2.get %2)))")

;; Unary integer
(define_expand "<opname_rt><mode>2"
  [(set (match_operand:REG 0 "register_operand")
     	(iunop:REG (match_operand:REG 1 "immediate_or_register_operand")))]
  ""
  {
    if (!subregister_for_<REG:mode>_operand(operands[1], <REG:MODE>mode))
      operands[1] = force_reg (<REG:MODE>mode, operands[1]);
  })

(define_insn "*<opname_rt><mode>2"
  [(set (match_operand:REG 0 "wasm_register_operand")
	    (iunop:REG (match_operand:REG 1 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<types>.<iunop:opname> (%M1.get %1)))")

;; Binary float
(define_expand "<opname_rt><mode>3"
  [(set (match_operand:F 0 "register_operand")
	    (fbinop:F (match_operand:F 1 "immediate_or_register_operand")
                  (match_operand:F 2 "immediate_or_register_operand")))]
  ""
  {
    operands[1] = force_reg (<MODE>mode, operands[1]);
    operands[2] = force_reg (<MODE>mode, operands[2]);
  })

(define_insn "*<opname_rt><mode>3"
  [(set (match_operand:F 0 "wasm_register_operand")
	    (fbinop:F (match_operand:F 1 "wasm_register_operand")
                  (match_operand:F 2 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<types>.<opnamef> (%M1.get %1) (%M2.get %2)))")

;; Unary float
(define_expand "<opname_rt><mode>2"
  [(set (match_operand:F 0 "register_operand")
     	(funop:F (match_operand:F 1 "immediate_or_register_operand")))]
  ""
  {
    operands[1] = force_reg (<MODE>mode, operands[1]);
  })

(define_insn "*<opname_rt><mode>2"
  [(set (match_operand:F 0 "wasm_register_operand")
	    (funop:F (match_operand:F 1 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<types>.<funop:opnamef> (%M1.get %1)))")

;; Integer conv
(define_expand "<iconvop:opname_rt><QHSDI2:mode><QHSDI:mode>2"
  [(set (match_operand:QHSDI 0 "wasm_subregister_operand")
        (iconvop:QHSDI (match_operand:QHSDI2 1 "wasm_subregister_operand")))]
  ""
  {
    wasm_expand_conv (operands[0], operands[1], <iconvop:CODE>, false);
    DONE;
  })

(define_insn "*extend_si_<SUBREGSI:mode><QHSDI:mode>"
  [(set (match_operand:QHSDI 0 "register_operand")
        (sign_extend:QHSDI (match_operand:SUBREGSI 1 "register_operand")))]
  "wasm_real_register_mode (operands[0]) == SImode && wasm_real_register_mode (operands[1]) == SImode"
  "(%M0.set %0 (i32.extend<SUBREGSI:size>_s (%M1.get %1)))")

(define_insn "*extend_di_<SUBREGDI:mode><QHSDI:mode>"
  [(set (match_operand:QHSDI 0 "register_operand")
        (sign_extend:QHSDI (match_operand:SUBREGDI 1 "register_operand")))]
  "wasm_real_register_mode (operands[0]) == DImode && wasm_real_register_mode (operands[1]) == DImode"
  "(%M0.set %0 (i64.extend<SUBREGDI:size>_s (%M1.get %1)))")
(define_insn "*extend_sidi"
  [(set (match_operand:DI 0 "register_operand")
        (sign_extend:DI (match_operand:SI 1 "register_operand")))]
  "REG_P (operands[0]) && REG_P (operands[1])"
  "(%M0.set %0 (i64.extend_i32_s (%M1.get %1)))")
(define_insn "*uextend_sidi"
  [(set (match_operand:DI 0 "register_operand")
        (zero_extend:DI (match_operand:SI 1 "register_operand")))]
  "REG_P (operands[0]) && REG_P (operands[1])"
  "(%M0.set %0 (i64.extend_i32_u (%M1.get %1)))")
(define_insn "*truncate_disi"
  [(set (match_operand:SI 0 "register_operand")
        (truncate:SI (match_operand:DI 1 "register_operand")))]
  "REG_P (operands[0]) && REG_P (operands[1])"
  "(%M0.set %0 (i32.wrap_i64 (%M1.get %1)))")

;; Float conv
(define_insn "extendsfdf2"
  [(set (match_operand:DF 0 "wasm_register_operand")
        (float_extend:DF (match_operand:SF 1 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (f64.promote_f32 (%M1.get %1)))")

(define_insn "truncdfsf2"
  [(set (match_operand:SF 0 "wasm_register_operand")
        (float_truncate:SF (match_operand:DF 1 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (f32.demote_f64 (%M1.get %1)))")

;; Float -> int conv
(define_insn "fix_trunc<F:mode><REG:mode>2"
  [(set (match_operand:REG 0 "wasm_register_operand")
        (fix:REG (match_operand:F 1 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<REG:types>.trunc_sat_<F:types>_s (%M1.get %1)))")

(define_insn "fixuns_trunc<F:mode><REG:mode>2"
  [(set (match_operand:REG 0 "wasm_register_operand")
        (unsigned_fix:REG (match_operand:F 1 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<REG:types>.trunc_sat_<F:types>_u (%M1.get %1)))")

;;  Int -> float conv
(define_insn "float<REG:mode><F:mode>2"
  [(set (match_operand:F 0 "wasm_register_operand")
        (float:F (match_operand:REG 1 "wasm_register_operand")))]
  ""
  "(%M0.set %0 (<F:types>.convert_<REG:types>_s (%M1.get %1)))")

;; Rectify int <-> float cast
;;(define_insn "floatuns<REG:mode><F:mode>2"
;;  [(set (match_operand:F 0 "register_operand")
;;        (unsigned_float:F (match_operand:REG 1 "register_operand")))]
;;  ""
;;  "(%M0.set %0 (<F:types>.convert_<REG:types>_u (%M1.get %1)))")
;;
;;(define_subst "swap_subreg_s_<mode>"
;;  [(set (match_operand:REGF 0)
;;        (subreg:REGF (match_operand:<REGF:REG2F> 1) 0))]
;;  ""
;;  [(set (subreg:<REGF:REG2F> (match_dup 0) 0)
;;        (match_dup 1))])
;;(define_subst_attr "swap_subregsi" "swap_subreg_s_si" "_from" "_to")
;;(define_subst_attr "swap_subregdi" "swap_subreg_s_di" "_from" "_to")
;;(define_subst_attr "swap_subregsf" "swap_subreg_s_sf" "_from" "_to")
;;(define_subst_attr "swap_subregdf" "swap_subreg_s_df" "_from" "_to")

;; Float -> int cast
(define_insn "*cast<REG:reg2f><REG:mode>"
  [(set (match_operand:REG 0 "wasm_register_operand")
        (subreg:REG (match_operand:<REG:REG2F> 1 "wasm_register_operand") 0))]
  ""
  "(%M0.set %0 (<REG:types>.reinterpret_<reg2f_types> (%M1.get %1)))")

(define_insn "*cast<REG:reg2f><REG:mode>"
  [(set (subreg:<REG:REG2F> (match_operand:REG 0 "wasm_register_operand") 0)
        (match_operand:<REG:REG2F> 1 "wasm_register_operand"))]
  ""
  "(%M0.set %0 (<REG:types>.reinterpret_<reg2f_types> (%M1.get %1)))")

;; Int -> float cast
(define_insn "*cast<mode><reg2f>"
  [(set (match_operand:<REG2F> 0 "wasm_register_operand")
        (subreg:<REG2F> (match_operand:REG 1 "wasm_register_operand") 0))]
  ""
  "(%M0.set %0 (<reg2f_types>.reinterpret_<REG:types> (%M1.get %1)))")

(define_insn "*cast<mode><reg2f>"
  [(set (subreg:REG (match_operand:<REG2F> 0 "wasm_register_operand") 0)
        (match_operand:REG 1 "wasm_register_operand"))]
  ""
  "(%M0.set %0 (<reg2f_types>.reinterpret_<REG:types> (%M1.get %1)))")