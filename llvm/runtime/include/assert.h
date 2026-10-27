/* fpga_cpu C runtime: <assert.h> */
#undef assert
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
void __assert_fail(const char *expr, const char *file, int line) __attribute__((noreturn));
#define assert(x) ((x) ? (void)0 : __assert_fail(#x, __FILE__, __LINE__))
#endif
