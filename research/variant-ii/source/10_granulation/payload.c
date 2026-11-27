/* Raw position-independent code, no external references or global data. */
struct slots { int (*add)(int,int); int (*sub)(int,int); const int *a, *b; };
__attribute__((section(".text.add"),used,noinline)) int add(int a,int b) { return a+b; }
__attribute__((section(".text.sub"),used,noinline)) int sub(int a,int b) { return a-b; }
__attribute__((section(".text.program"),used,noinline)) int program(const struct slots *s) { return s->sub(s->add(*s->a,*s->b),2); }
