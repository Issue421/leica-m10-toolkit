#include "m10_gui.h"
static const struct m10gui_profile profile = {
    "Leica M10", "4.26.36.10",
    "393d3c13a13e88ef43b58a07585dce06ce667f7c596da849f41af4653d9a0460",
    9898708U, 0x4082e93cU, 0x40993a48U,
    {0x4082ca88U,0x4082ca90U,0x4082ca98U}, 0x421133d5U
};
const struct m10gui_profile *m10gui_fixed_profile(void) { return &profile; }
/* Extra M10 structural guard, AFTER the real owner permits mapped reads.
   In particular, P's registered 4211500D stub must not pass this adapter.
   These known values alone do not prove firmware authenticity or ownership. */
static int m10_shape(const struct m10gui_memory *m) {
    unsigned i;
    const volatile struct m10gui_record *root;
    if(!m || !m->table || m->table_address!=profile.table_address) return 0;
    for(i=0;i<3U;++i)
        if(m->child_address[i]!=profile.child_address[i]) return 0;
    root=&m->table[M10GUI_FIRST];
    return root->callback==profile.stock_callback;
}
int m10gui_guard_owner_inspect(void *o,const struct m10gui_memory *m,struct m10gui_domain *d) {
    return m10gui_owner_inspect(o,&profile,m,d)!=0 || !m10_shape(m) ? 1:0;
}
int m10gui_guard_owner_acquire(void *o,const struct m10gui_memory *m,uint64_t *lease) {
    return m10gui_owner_acquire(o,&profile,m,lease);
}
int m10gui_guard_owner_check(void *o,uint64_t lease,const struct m10gui_memory *m,struct m10gui_domain *d) {
    return m10gui_owner_check(o,&profile,lease,m,d)!=0 || !m10_shape(m) ? 1:0;
}
void m10gui_guard_owner_abort(void *o,uint64_t lease) { m10gui_owner_abort(o,&profile,lease); }
int m10gui_guard_owner_finish(void *o,uint64_t lease,const struct m10gui_redraw_receipt *receipt) {
    return m10gui_owner_finish(o,&profile,lease,receipt);
}
