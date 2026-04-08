
#ifdef RTX_CODE
extern void wasm_expand_prologue ();
extern void wasm_expand_epilogue ();
extern void wasm_expand_call (rtx retval, rtx fn, rtx aux);
extern void wasm_emit_jump (rtx dest, rtx fn);
extern bool wasm_expand_mov (rtx dest, rtx src, machine_mode mode);
extern void wasm_expand_conv (rtx, rtx, rtx_code, bool = true);
extern void wasm_expand_compare (machine_mode m, rtx res, rtx cmp);
extern machine_mode wasm_real_register_mode (rtx reg);
rtl_opt_pass *make_pass_count_labels (gcc::context *ctx);
#endif
