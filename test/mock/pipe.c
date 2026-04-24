#include <unistd.h>

#ifndef TDO_PIPE_AMOUNT
    #error "An amount of successful calls to pipe must be defined"
#endif

static int pipe_amount = 0;

int tdo_mock_pipe(int fds[2]) {
    if (pipe_amount++ < TDO_PIPE_AMOUNT) {
        return pipe(fds);
    }
    return 1;
}
