#ifndef LVF_H
#define LVF_H

/* No native addresses, allocation, menu reads or target installation. */
#include <stdint.h>

enum { LVF_OBJECTS = 7, LVF_LINES = 4, LVF_UPRIGHT = 1 };
enum lvf_role { LVF_ROOT, LVF_HORIZONTAL_GROUP, LVF_TOP, LVF_BOTTOM,
                LVF_VERTICAL_GROUP, LVF_LEFT, LVF_RIGHT };
enum lvf_mode { LVF_PRESET, LVF_USER1, LVF_USER2 };
enum lvf_status { LVF_OK, LVF_ARGUMENT, LVF_DEPENDENCY, LVF_DISPLAY,
                  LVF_BINDING, LVF_LEASE, LVF_IO, LVF_RECOVERY, LVF_BUSY };

struct lvf_rect { int32_t x, y, width, height; };
struct lvf_config {
    uint32_t generation; /* caller's consistent snapshot identifier; not a lock */
    uint8_t enabled, mode, preset;
    uint16_t user_width[2], user_height[2]; /* settings 52/54 and 53/55, 0.01 mm */
    uint16_t color; /* adapter-qualified native 16-bit line color, no menu read */
};
struct lvf_display {
    uint16_t logical_width, logical_height, scale, pixel_width, pixel_height;
    uint8_t device_code, orientation;
    /* Nonzero only after backend qualifies top-left axes, scale, clipping,
       parent visibility and its native-setter translation for this surface. */
    uint8_t qualified;
};
struct lvf_description {
    uint16_t native_id, parent_native_id;
    uint8_t native_type, anchor_x, anchor_y;
    void *parent;
    int32_t width, height, relative_width, relative_height;
};
struct lvf_line {
    /* Native logical anchored setter values, BEFORE GUI scaling. */
    int32_t x, y, width, height;
    /* Complete resolved pixel-space rectangle, BEFORE parent clipping.
       Backend must not scale these values a second time. */
    struct lvf_rect pixels;
};
struct lvf_frame {
    uint32_t generation;
    struct lvf_rect nominal_gate, strict_aperture; /* logical reference units */
    struct lvf_line line[LVF_LINES]; /* top, bottom, left, right */
};
struct lvf_binding;
struct lvf_backend {
    /* All callbacks return 0 on success. No callback may reenter this object.
       GUI calls (including native stock callback) must be serialized.
       inspect is read-only. begin is all-or-none: capture ALL modified fields
       and redraw state, hold exclusive stable handles, settle stock hide seam,
       return nonzero lease token. No camera/native glue is supplied here. */
    int (*inspect)(void *, void *, struct lvf_description *);
    int (*begin)(void *, const struct lvf_binding *, uint64_t *);
    /* check must requalify ownership, handle identity/roles/parents/anchors,
       display configuration and stock-hide scheduling for the whole lease. */
    int (*check)(void *, uint64_t);
    int (*visible)(void *, uint64_t, void *, uint8_t);
    /* apply may partially change geometry/color on failure; root is hidden. */
    int (*apply)(void *, uint64_t, void *, const struct lvf_line *, uint16_t);
    int (*redraw)(void *, uint64_t, void *);
    /* Restore exact saved native state, redraw, then release lease. Failure
       retains lease and recovery responsibility. Borrowed objects never die. */
    int (*restore_release)(void *, uint64_t);
};
struct lvf_binding {
    void *object[LVF_OBJECTS];
    struct lvf_display display;
};
struct lvf {
    struct lvf_backend backend;
    struct lvf_binding binding;
    void *context;
    uint64_t lease;
    uint8_t opened, busy, recovery;
};

/* Initialize a zeroed lvf once. APIs, config writes and all native GUI work
   share one serialized owner. The config argument must be a consistent caller
   snapshot with no concurrent writer during the initial C struct copy. A C
   copy does NOT make reads of unrelated mutable settings atomic. Later callback
   changes to the caller's config cannot affect the current frame. */
enum lvf_status lvf_open(struct lvf *, const struct lvf_backend *, void *,
                         const struct lvf_binding *);
/* out is written only after a successful enabled frame. Disabled/failed calls
   leave it untouched. On IO failure the component best-effort hides the root;
   RECOVERY requires close/recovery before another draw. */
enum lvf_status lvf_draw(struct lvf *, const struct lvf_config *, struct lvf_frame *);
enum lvf_status lvf_close(struct lvf *);

#endif
