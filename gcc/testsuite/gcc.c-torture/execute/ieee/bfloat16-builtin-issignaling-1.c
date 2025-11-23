#define CONCATX(X, Y) X ## Y
#define CONCAT(X, Y) CONCATX (X, Y)

#define TYPE __bf16
#define CST(C) CONCAT (C, bf16)
#define FN(F) CONCAT (F, f16b)
#define NAN(x) ((__bf16) __builtin_nanf (x))
#define INF ((__bf16) __builtin_inff ())
#define EXT 0

#include "builtin-issignaling-1.c"
