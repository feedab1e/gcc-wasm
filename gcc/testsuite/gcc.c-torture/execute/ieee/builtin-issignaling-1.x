# WebAssembly has no concept of signaling NaNs.
if [istarget "wasm*-*-*"] {
    set torture_eval_before_execute {
	global compiler_conditional_xfail_data
	set compiler_conditional_xfail_data {
	    "WebAssembly has no concept of signaling NaNs."
	    { "*-*-*" }
	    {}
	    {}
	}
    }
}

lappend additional_flags "-fsignaling-nans"

if { [check_effective_target_ia32] && [check_effective_target_sse2_runtime] } {
    lappend additional_flags "-msse2 -mfpmath=sse"
}

return 0
