#include <signal.h>
#include "../include/profile.h"

void profile_handler(int sig, siginfo_t* info, void* ucontext) {
    StackTrace trace;
    trace.depth = 0;

    uintptr_t* rbp = (uintptr_t*)__builtin_frame_address(0);

    while (rbp && trace.depth < 64){
        uintptr_t ret_addr = *(rbp + 1);
        if (ret_addr == 0) break;

        trace.frames[trace.depth++] = ret_addr;

        // Move to the previous frame
        uintptr_t next_rbp = *rbp;

        if (next_rbp <= rbp) break; // Prevent infinite loop
        rbp = (uintptr_t*)next_rbp;
    }
}

void setup_signal_hanler(){
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = profile_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);
}
