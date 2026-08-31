#ifndef CALLFUNC_H
#define CALLFUNC_H
/*
 call C function with argument list created at runtime.
 See lib/armutil/callfunc.S for documentation
*/
// Note: used in modules and platform independent code. 
// Do not add platform dependent stuff in here (#ifdef/#endif compile options or camera dependent values)

// 'long_call' attribute required to ensure correct calling from thumb code
// GCC 4.7.3 on OS/X does not generate thumb-interwork for this function ????
unsigned __attribute__((long_call)) call_func_ptr(void *func, const unsigned *args, unsigned n_args);

#endif // CALLFUNC_H
