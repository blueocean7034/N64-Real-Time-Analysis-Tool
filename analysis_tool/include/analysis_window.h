#ifndef ANALYSIS_WINDOW_H
#define ANALYSIS_WINDOW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "m64p_types.h"

typedef m64p_error (*core_do_command_func)(m64p_command, int, void*);

void analysis_window_start(core_do_command_func core_cmd);
void analysis_window_stop(void);

#ifdef __cplusplus
}
#endif

#endif // ANALYSIS_WINDOW_H
