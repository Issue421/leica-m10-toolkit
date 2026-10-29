#ifndef TOUCH_CALLER_RUNTIME_INTERNAL_H
#define TOUCH_CALLER_RUNTIME_INTERNAL_H
#include "caller_runtime.h"
/* Private cross-file seam, not a caller reservation/recovery API. */
trc_status trc_private_begin(trc_pool *, trc_handle *, trc_slot **);
void trc_private_finish(trc_slot *);
#endif
