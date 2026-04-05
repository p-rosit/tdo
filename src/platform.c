#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    #include "platform/posix.c"
#elif defined(_WIN32)
    #include "platform/windows.c"
#else
    #error "Unknown platform"
#endif
