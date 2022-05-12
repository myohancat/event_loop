# Event loop

Simple Event loop with select() api.

```c
#include "event_loop.h"

#include <signal.h>

static void _sig_handler(int signum)
{
    (void)signum;

    printf("SIGINT - terminate program\n");
    EventLoop::getInstance().terminate();
}

int main(void)
{
    signal(SIGINT, _sig_handler);

    // TODO. IMPLEMENTS HERE

    while(EventLoop::getInstance().loop()) { }

    // TODO. IMPLEMENTS HERE

    return 0;
}

```
