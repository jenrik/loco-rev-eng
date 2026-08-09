/* stub_func.c — Debug stub with caller resolution */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#ifndef _WIN32
/* Host: dladdr()/dlfcn.h have no Windows equivalent used here. */

__attribute__((constructor)) static void early_init(void) {
    setvbuf(stderr, NULL, _IONBF, 0);
}

extern "C" void __attribute__((used)) stub_crash_handler(void);
extern "C" void __attribute__((used)) stub_crash_handler(void) {
    void* retaddr = __builtin_return_address(0);
    Dl_info info;
    if (dladdr(retaddr, &info)) {
        fprintf(stderr, "[STUB] called from %s (%s) offset %p\n",
                info.dli_fname ? info.dli_fname : "?",
                info.dli_sname ? info.dli_sname : "?",
                (void*)((char*)retaddr - (char*)info.dli_fbase));
    } else {
        fprintf(stderr, "[STUB] called from %p (unresolved)\n", retaddr);
    }
    
    /* Also check the caller's caller */
    void* caller2 = __builtin_return_address(1);
    if (dladdr(caller2, &info)) {
        fprintf(stderr, "[STUB]   caller's caller: %s (%s)\n",
                info.dli_fname ? info.dli_fname : "?",
                info.dli_sname ? info.dli_sname : "?");
    }
    
    fflush(stderr);
    abort();
}

char __attribute__((used)) stub_data_page[4096] = {0};

#endif /* _WIN32 */
