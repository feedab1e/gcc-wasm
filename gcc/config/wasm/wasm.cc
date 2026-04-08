#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "df.h"
#include "memmodel.h"
#include "stringpool.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "output.h"
#include "varasm.h"
#include "calls.h"
#include "explow.h"
#include "expr.h"
#include "builtins.h"

#include "target.h"
#include "common/common-target.h"

static machine_function *
wasm_init_machine_status (void)
{
  machine_function *p = ggc_cleared_alloc<machine_function> ();
  p->regs_ever_live = BITMAP_GGC_ALLOC ();
  return p;
}

#define TARGET_OPTION_OVERRIDE wasm_option_override
static void
wasm_option_override (void)
{
  init_machine_status = wasm_init_machine_status;
}

void wasm_asm_init_sections ();
#define TARGET_ASM_INIT_SECTIONS wasm_asm_init_sections
bool wasm_assemble_integer (rtx x, unsigned size, int align);
#define TARGET_ASM_INTEGER wasm_assemble_integer
void wasm_assemble_decl_end ();
#define TARGET_ASM_DECL_END wasm_assemble_decl_end
void wasm_handle_import (FILE *stream, const char *name, const_tree decl);
#define TARGET_ASM_ASSEMBLE_UNDEFINED_DECL wasm_handle_import
void wasm_handle_libcall (rtx symbol);
#define TARGET_ASM_EXTERNAL_LIBCALL wasm_handle_libcall
void wasm_assemble_data_begin (FILE *stream, tree decl, const char *name,
			       HOST_WIDE_INT size, HOST_WIDE_INT align,
			       bool pub);
#define TARGET_ASM_DECLARE_CONSTANT_NAME \
  [] (FILE *stream, const char *name, const_tree, HOST_WIDE_INT size) \
  { return wasm_assemble_data_begin (stream, 0, name, size, 0, false); }

#define TARGET_ASM_GLOBALIZE_LABEL [](FILE *, const char *) {}
#define TARGET_ASM_ASSEMBLE_VISIBILITY [](tree, int) {}
#define TARGET_ASM_CONSTRUCTOR [] (rtx, int) {}
#define TARGET_ASM_DESTRUCTOR [] (rtx, int) {}
#define TARGET_USE_LATE_PROLOGUE_EPILOGUE [] { return true; }

#define TARGET_ASM_FILE_START wasm_asm_file_start
void wasm_asm_file_start ();
#define TARGET_ASM_FILE_END wasm_asm_file_end
void wasm_asm_file_end ();

bool wasm_valid_punct_p (unsigned char code);
#define TARGET_PRINT_OPERAND_PUNCT_VALID_P wasm_valid_punct_p
void wasm_print_operand (FILE *stream, rtx value, int mode);
#define TARGET_PRINT_OPERAND wasm_print_operand
void wasm_output_internal_label (FILE *stream, const char *pfx, size_t no);
#define TARGET_ASM_INTERNAL_LABEL wasm_output_internal_label
#define TARGET_HAVE_NAMED_SECTIONS false

bool wasm_regno_mode_ok (unsigned regno, machine_mode mode);
#define TARGET_HARD_REGNO_MODE_OK wasm_regno_mode_ok
bool wasm_can_change_mode_class (machine_mode from, machine_mode to, reg_class_t);
#define TARGET_CAN_CHANGE_MODE_CLASS wasm_can_change_mode_class
bool wasm_legitimate_address_p (machine_mode, rtx x, bool, code_helper);
#define TARGET_LEGITIMATE_ADDRESS_P wasm_legitimate_address_p
void wasm_mark_arg_regnos (bitmap regnos);
#define TARGET_MARK_ARG_REGNOS wasm_mark_arg_regnos

void wasm_start_call_args (cumulative_args_t args);
#define TARGET_START_CALL_ARGS wasm_start_call_args
void wasm_call_args (cumulative_args_t args, rtx arg, tree fntype);
#define TARGET_CALL_ARGS wasm_call_args
void wasm_end_call_args (cumulative_args_t);
#define TARGET_END_CALL_ARGS wasm_end_call_args
bool wasm_pass_by_reference(cumulative_args_t, const function_arg_info &arg);
#define TARGET_PASS_BY_REFERENCE wasm_pass_by_reference
bool wasm_return_in_memory (const_tree type, const_tree fntype);
#define TARGET_RETURN_IN_MEMORY wasm_return_in_memory
#define TARGET_SPLIT_COMPLEX_ARG [] (auto ...) { return false; }

#define TARGET_SCALAR_MODE_SUPPORTED_P [](scalar_mode m) \
  { return default_scalar_mode_supported_p(m) && m != TImode; }

#define TARGET_EXCEPT_UNWIND_INFO [] (auto ...) { return UI_NONE; }

#define TARGET_HAVE_STRUB_SUPPORT_FOR hook_bool_tree_false
#define TARGET_ALLOCATE_STACK_SLOTS_FOR_ARGS hook_bool_void_false
#define TARGET_STRICT_ARGUMENT_NAMING [] (auto ...) { return true; }
unsigned int wasm_vararg_align (machine_mode mode, const_tree type);
#define TARGET_FUNCTION_ARG_BOUNDARY wasm_vararg_align
void wasm_function_arg_advance (cumulative_args_t, const function_arg_info &);
#define TARGET_FUNCTION_ARG_ADVANCE wasm_function_arg_advance
rtx wasm_function_value (const_tree type, const_tree ARG_UNUSED (func), bool);
#define TARGET_FUNCTION_VALUE wasm_function_value
rtx wasm_function_arg (cumulative_args_t cum_v, const function_arg_info &arg);
#define TARGET_FUNCTION_ARG wasm_function_arg
rtx wasm_function_incoming_arg (cumulative_args_t cum_v,
				const function_arg_info &arg);
#define TARGET_FUNCTION_INCOMING_ARG wasm_function_incoming_arg
rtx wasm_libcall_value (machine_mode, const_rtx);
#define TARGET_LIBCALL_VALUE wasm_libcall_value

#include "target-def.h"
gcc_target targetm = TARGET_INITIALIZER;

#include "common/common-target-def.h"
struct gcc_targetm_common targetm_common = TARGETM_COMMON_INITIALIZER;
