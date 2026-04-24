#include <unistd.h>

#ifndef TDO_FORK_AMOUNT
    #error "An amount of successful calls to fork must be defined"
#endif

static int fork_amount = 0;

int tdo_mock_fork(void) {
    if (fork_amount++ < TDO_FORK_AMOUNT) {
        return fork();
    }
    return -1;
}
