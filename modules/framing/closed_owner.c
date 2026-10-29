/* Optional CLOSED production owner. All paths deny without mapped reads,
   acquiring a lease, touching output values, or invoking any native primitive.
   Replace only with a real externally qualified integration, never a host flag. */
#include "m10_gui.h"
int m10gui_owner_inspect(void *o,const struct m10gui_profile *p,const struct m10gui_memory *m,struct m10gui_domain *d) {
    (void)o;(void)p;(void)m;(void)d;return 1;
}
int m10gui_owner_acquire(void *o,const struct m10gui_profile *p,const struct m10gui_memory *m,uint64_t *lease) {
    (void)o;(void)p;(void)m;(void)lease;return 1;
}
int m10gui_owner_check(void *o,const struct m10gui_profile *p,uint64_t lease,const struct m10gui_memory *m,struct m10gui_domain *d) {
    (void)o;(void)p;(void)lease;(void)m;(void)d;return 1;
}
void m10gui_owner_abort(void *o,const struct m10gui_profile *p,uint64_t lease) { (void)o;(void)p;(void)lease; }
int m10gui_owner_finish(void *o,const struct m10gui_profile *p,uint64_t lease,const struct m10gui_redraw_receipt *r) {
    (void)o;(void)p;(void)lease;(void)r;return 1;
}
