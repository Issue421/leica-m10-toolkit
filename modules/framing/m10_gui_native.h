#ifndef M10_GUI_NATIVE_H
#define M10_GUI_NATIVE_H
#include "lvf.h"
#include <stddef.h>
#include <stdint.h>

enum { M10GUI_COUNT = 0x5ec, M10GUI_FIRST = 222, M10GUI_STRIDE = 0xbc };
/* Native pointer-bearing fields are 32-bit ADDRESSES even on a 64-bit host. */
struct m10gui_record {
    /* M10 native LDRH(+0); +2 is scheduler state; +3 remains opaque.
       These bytes are read-only to this backend and are never qualification. */
    uint16_t id;
    uint8_t state, opaque3;
    int32_t offset_x, offset_y, inset_x, inset_y, relative_x, relative_y;
    int32_t pixel_width, pixel_height, width, height, relative_width, relative_height;
    int32_t left, top, right, bottom;
    uint32_t opaque44[4];
    int32_t clip_left, clip_top, clip_right, clip_bottom;
    uint32_t opaque64;
    uint8_t dirty, type, requested, effective, anchor_x, anchor_y, opaque6e[2];
    uint32_t callback, parent, descendants, child_count, children;
    uint16_t color, other_style;
    uint8_t opaque88[0x34];
};
struct m10gui_manager { uint8_t opaque[8], dirty, opaque9[7]; int32_t scale; };
_Static_assert(sizeof(struct m10gui_record) == M10GUI_STRIDE, "native record stride");
#define M10GUI_OFF(f,o) _Static_assert(offsetof(struct m10gui_record,f)==(o), "native " #f)
M10GUI_OFF(id,0x00); M10GUI_OFF(state,0x02); M10GUI_OFF(opaque3,0x03);
M10GUI_OFF(inset_x,0x0c); M10GUI_OFF(inset_y,0x10); M10GUI_OFF(relative_x,0x14);
M10GUI_OFF(pixel_width,0x1c); M10GUI_OFF(width,0x24); M10GUI_OFF(relative_width,0x2c);
M10GUI_OFF(left,0x34); M10GUI_OFF(clip_left,0x54); M10GUI_OFF(dirty,0x68);
M10GUI_OFF(type,0x69); M10GUI_OFF(requested,0x6a); M10GUI_OFF(effective,0x6b);
M10GUI_OFF(anchor_x,0x6c); M10GUI_OFF(callback,0x70); M10GUI_OFF(parent,0x74);
M10GUI_OFF(descendants,0x78); M10GUI_OFF(child_count,0x7c); M10GUI_OFF(children,0x80);
M10GUI_OFF(color,0x84); M10GUI_OFF(other_style,0x86);
#undef M10GUI_OFF
_Static_assert(offsetof(struct m10gui_manager,dirty)==8, "manager dirty");
_Static_assert(offsetof(struct m10gui_manager,scale)==0x10, "manager scale");

/* Qualified integration must provide these mappings to actual stable GUI data.
   Host fixtures use ordinary arrays; their synthetic addresses are NOT RAM. */
struct m10gui_memory {
    struct m10gui_record *table; /* M10GUI_COUNT contiguous native-layout records */
    struct m10gui_manager *manager;
    uint32_t table_address;
    const volatile uint32_t *child[3]; /* root, horizontal group, vertical group */
    uint32_t child_address[3];
};
struct m10gui_domain { struct lvf_display display; int32_t root_left, root_top; };
struct m10gui_saved_line { int32_t x,y,w,h; uint16_t color; };
struct m10gui_redraw_receipt { uint8_t manager_dirty, family_dirty[LVF_OBJECTS]; };
struct m10gui {
    struct m10gui_memory memory;
    void *owner; /* external real GUI integration, not a caller qualification flag */
    struct lvf_binding binding;
    struct m10gui_domain domain;
    struct m10gui_saved_line saved[LVF_LINES];
    struct m10gui_redraw_receipt redraw_before;
    uint8_t requests[LVF_OBJECTS];
    uint32_t callbacks[LVF_OBJECTS];
    uint64_t lease;
    uint8_t active;
};
/* Only named typed imports. No firmware address casts, linker script, installer,
   target runner or default/mock implementations are part of production source. */
int32_t m10gui_native_set_x(uint32_t id, int32_t logical_inset); /* 0/1 change flag */
int32_t m10gui_native_set_y(uint32_t id, int32_t logical_inset); /* 0/1 change flag */
int32_t m10gui_native_set_size(uint32_t id, int32_t w, int32_t h); /* 0/1 change flag */
void m10gui_native_set_color(uint32_t id, uint16_t color); /* type 0 only */
void m10gui_native_set_visible(uint32_t id, uint32_t request);
void m10gui_native_dirty(struct m10gui_record *object);

/* UNIMPLEMENTED real owner/display/stock-hide integration boundary.
   Calls must already execute on the real serialized GUI owner. inspect proves
   stable readable mappings for this operation. acquire is all-or-none and holds
   identities, layout/domain, normal redraw state and stock-hide scheduling.
   acquire must not mutate family fields before this backend snapshots them.
   check requalifies those facts and returns observed domain (including axes,
   orientation, clipping and parent visibility). A token is only an external
   lease reference: this source cannot create native qualification from it.
   abort only follows an acquire with NO native mutations.
   finish must preserve/merge preexisting and newly required redraw work through
   the real GUI owner, restore stock scheduling, and release all-or-none. Failure
   retains ownership/recovery. It must NEVER memcpy dirty receipts into RAM. */
int m10gui_guard_owner_inspect(void *owner, const struct m10gui_memory *, struct m10gui_domain *);
int m10gui_guard_owner_acquire(void *owner, const struct m10gui_memory *, uint64_t *lease);
int m10gui_guard_owner_check(void *owner, uint64_t lease, const struct m10gui_memory *, struct m10gui_domain *);
void m10gui_guard_owner_abort(void *owner, uint64_t lease);
int m10gui_guard_owner_finish(void *owner, uint64_t lease, const struct m10gui_redraw_receipt *);

/* Initialize a zeroed context; an active borrowed family cannot be reset. */
int m10gui_init(struct m10gui *, const struct m10gui_memory *, void *owner);
int m10gui_binding(struct m10gui *, struct lvf_binding *);
const struct lvf_backend *m10gui_backend(void);
#endif
