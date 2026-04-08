(define_c_enum "unspecv" [
  UNSPECV_CALL_ADDRESS_BARRIER
])

(define_mode_iterator QHSDI [QI HI SI DI])
(define_mode_iterator QHSDI2 [QI HI SI DI])
(define_mode_iterator QHSDISDF [QI HI SI DI SF DF])
(define_mode_iterator F [SF DF])
(define_mode_iterator REG [SI DI])
(define_mode_iterator REGF [SI DI SF DF])
(define_mode_attr REG2F [(SI "SF") (DI "DF") (SF "SI") (DF "DI")])
(define_mode_attr reg2f [(SI "sf") (DI "df") (SF "si") (DF "di")])
(define_mode_attr reg2f_types [
  (SF "i32") (DF "i64")
  (SI "f32") (DI "f64")])
(define_mode_attr types [
  (QI "i8") (HI "i16")
  (SI "i32") (DI "i64")
  (SF "f32") (DF "f64")])
(define_mode_attr size [
  (QI "8") (HI "16")
  (SI "32") (DI "64")
  (SF "32") (DF "64")])
(define_mode_attr promote_type [
  (QI "i32") (HI "i32")
  (SI "i32") (DI "i64")
  (SF "f32") (DF "f32")])
(define_mode_attr promote_mode [
  (QI "SI") (HI "SI")
  (SI "SI") (DI "DI")
  (SF "SF") (DF "DF")])
(define_mode_iterator SUBREGSI [QI HI])
(define_mode_iterator SUBREGDI [QI HI SI])
(define_mode_iterator P [(SI "Pmode == SImode") (DI "Pmode == DImode")])

(define_code_iterator irelop [le ge lt gt leu geu ltu gtu eq ne])
(define_code_iterator frelop [le ge lt gt eq ne])

(define_code_iterator iunop [clz ctz popcount])

(define_code_iterator iconvop [sign_extend truncate])

(define_code_iterator ibinop [
  and ior xor
  plus minus mult
  div udiv mod umod
  ashift ashiftrt lshiftrt
  rotate rotatert])

(define_code_iterator fbinop [plus minus mult div smin smax copysign])

(define_code_iterator funop [abs neg sqrt])

(define_code_attr opname [
  (eq  "eq") (ne  "ne")
  (le  "le_s") (ge  "ge_s") (lt  "lt_s") (gt  "gt_s")
  (leu "le_u") (geu "ge_u") (ltu "lt_u") (gtu "gt_u")
  (clz "clz") (ctz "ctz") (popcount  "popcnt")
  (and "and") (ior "or") (xor "xor")
  (plus "add") (minus "sub") (mult "mul")
  (div "div_s") (udiv "div_u") (mod "rem_s") (umod "rem_u")
  (ashift "shl") (ashiftrt "shr_s") (lshiftrt "shr_u")
  (rotate "rotl") (rotatert "rotr")
  (sign_extend "extend") (zero_extend "zero_extend")
  (truncate "trunc")
  (abs "abs") (neg "neg") (sqrt "sqrt")])
(define_code_attr opnamef [
  (eq  "eq") (ne  "ne") (le  "le") (ge  "ge") (lt  "lt") (gt  "gt")
  (plus "add") (minus "sub") (mult "mul") (div "div") (smin "min") (smax "max")
  (abs "abs") (neg "neg") (sqrt "sqrt") (copysign "copysign")])
(define_code_attr opname_rt [
  (eq "eq") (ne "ne")
  (le "le") (ge "ge") (lt "lt") (gt "gt")
  (leu "leu") (geu "geu") (ltu "ltu") (gtu "gtu")
  (clz "clz") (ctz "ctz") (popcount  "popcount")
  (and "and") (ior "ior") (xor "xor")
  (plus "add") (minus "sub") (mult "mul")
  (div "div") (udiv "udiv") (mod "mod") (umod "umod")
  (ashift "ashl") (ashiftrt "ashr") (lshiftrt "lshr")
  (rotate "rotl") (rotatert "rotr")
  (sign_extend "extend") (zero_extend "zero_extend")
  (truncate "ftrunc")
  (smax "smax") (smin "smin") (copysign "copysign")
  (abs "abs") (neg "neg") (sqrt "sqrt")])
