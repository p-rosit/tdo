#include "platform/interface.h"

#if defined(TDO_POSIX)
    #include "platform/posix.c"
#elif defined(TDO_WINDOWS)
    #include "platform/windows.c"
#else
    #error "Unknown platform"
#endif
