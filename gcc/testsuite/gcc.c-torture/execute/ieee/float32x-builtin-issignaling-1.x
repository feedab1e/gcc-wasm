if ![check_effective_target_float32x_runtime] {
    return 1
}

lappend additional_flags "-fsignaling-nans"

if { [check_effective_target_ia32] && [check_effective_target_sse2_runtime] } {
    lappend additional_flags "-msse2 -mfpmath=sse"
}

return 0
