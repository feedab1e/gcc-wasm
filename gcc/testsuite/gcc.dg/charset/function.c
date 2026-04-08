/* { dg-do compile }
   { dg-require-iconv "IBM1047" }
   { dg-final { scan-assembler-not "\"foobar\"" { xfail wasm*-*-* } } } */

const char *str;

void foobar (void)
{
  str = __FUNCTION__;
}
