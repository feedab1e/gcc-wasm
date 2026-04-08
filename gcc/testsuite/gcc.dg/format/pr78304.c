/* { dg-do compile { target inttypes_types } } */
/* { dg-skip-if "wasi-libc does not support PRIuN" { wasm*-*-* } } */
/* { dg-options "-O2 -Wall -Wextra" } */

#include <inttypes.h>
#include <stdio.h>

void test (const char *msg)
{
  printf ("size: %" PRIu32 "\n", msg); /* { dg-warning "expects argument of type" } */
}
