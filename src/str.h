#ifndef TDO_STR_H
#define TDO_STR_H
#include "platform.h"
#include <stdlib.h>

struct TdoString {
    char *bytes;
    size_t length;
};

struct TdoString tdo_string_init(void);
bool tdo_string_append(struct TdoString *string, struct TdoArena *arena, size_t length, char const *data);
bool tdo_string_clone(struct TdoString *copy, struct TdoArena *arena, struct TdoString string);

struct TdoLog {
    TdoFileDescriptor fd;
    struct TdoString data;
    size_t capacity;
};

struct TdoLog tdo_log_init(TdoFileDescriptor fd);
void tdo_log_reset(struct TdoLog *log, TdoFileDescriptor fd);

#endif
