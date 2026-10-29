#include "m10_asc.h"
/* Qualified ordinary-M10 scheduler/idle/recovery imports are not established.
   Closed production binding cannot acquire even an input view. No side effects. */
m10_status m10_native_acquire_current(m10_native_inputs *p){(void)p;return M10_UNQUALIFIED;}
m10_status m10_native_enter_confirmed_idle(void){return M10_UNQUALIFIED;}
m10_status m10_native_invalidate_both_caches(void){return M10_UNQUALIFIED;}
m10_status m10_native_verify_idle_cacheinvalid(void){return M10_UNQUALIFIED;}
m10_status m10_native_write_cc1(const m10_field f[9]){(void)f;return M10_UNQUALIFIED;}
m10_status m10_native_transfer_gamma(const m10_gamma_view *v){(void)v;return M10_UNQUALIFIED;}
m10_status m10_native_write_gamma_control(uint32_t m,uint32_t v){(void)m;(void)v;return M10_UNQUALIFIED;}
m10_status m10_native_check_current_epochs(m10_epochs e){(void)e;return M10_UNQUALIFIED;}
m10_status m10_native_verify_complete_idle(const m10_plan *p){(void)p;return M10_UNQUALIFIED;}
m10_status m10_native_publish_and_resume(const m10_plan *p){(void)p;return M10_UNQUALIFIED;}
m10_status m10_native_verify_running(const m10_plan *p){(void)p;return M10_UNQUALIFIED;}
m10_status m10_native_abort_quiescent_cacheinvalid(void){return M10_UNQUALIFIED;}
void m10_native_release_current(void){}
