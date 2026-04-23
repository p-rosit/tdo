#ifndef TDO_RUN_H
#define TDO_RUN_H
#include "test.h"
#include <stdio.h>

struct TdoRun;

void tdo_json_escaped(FILE *file, struct TdoString string);
void tdo_log_dump(struct TdoLog log, FILE *file, char const *name);
enum TdoError tdo_string_previous_line(struct TdoString *line, struct TdoString string, size_t index);
enum TdoError tdo_parse_size_t(size_t *number, char const *string);

enum TdoError tdo_run_report_assemble_step(struct TdoString *step, struct TdoArena *arena, struct TdoString step_name, struct TdoSymbol symbol);

void tdo_run_report_status(struct TdoRun *run, struct TdoArena *arena, FILE *file, int status, double duration);
void tdo_run_report_exit(struct TdoRun *run, FILE *file, char const *step, TdoProcessStatus status, double duration);
void tdo_run_report_error(struct TdoTest test, FILE *file, char const *step, char const *error, double duration);
void tdo_status_error(FILE *file, char const *fmt, ...);

void tdo_assert_library_loaded(struct TdoFile *file, FILE *status);
TdoTestSymbol *tdo_symbol_get(struct TdoSymbol symbol, struct TdoArena *arena, char const *name, FILE *status);

void tdo_run_fixtures(struct TdoTest *test, enum TdoFixtureKind kind, struct TdoArena *arena, FILE *status);
void tdo_run_single(struct TdoTest *test, struct TdoArena *arena, FILE *status);

enum TdoError tdo_run_all(struct TdoArguments args, FILE *output, struct TdoArena *arena, struct TdoArray tests);


#endif
