/* { dg-additional-options "-std=gnu17" } */
 
long __attribute__((noipa))
sink1(void)
{
  return 0;
}
long __attribute__((noipa))
sink2(void*)
{
  return 0;
}
 
 
static inline void lstrcpynW( short *d, const short *s, int n )
{
    unsigned int count = n;
 
    while ((count > 1) && *s)
    {
        count--;
        *d++ = *s++;
    }
    if (count) *d = 0;
}
 
int __attribute__((noinline,noclone))
badfunc(int u0, int u1, int u2, int u3,
  short *fsname, unsigned int fsname_len)
{
    static const short ntfsW[] = {'N','T','F','S',0};
    char superblock[2048+3300];
    int ret = 0;
    short *p;
 
    if (sink1())
        return 0;
    p = (void *)sink1();
    if (p != 0)
        goto done;
 
    sink2(superblock);
 
    lstrcpynW(fsname, ntfsW, fsname_len);
 
    ret = 1;
done:
    return ret;
}
 
 
int main()
{
    short buf[6];
    return !badfunc(0, 0, 0, 0, buf, 6);
}
