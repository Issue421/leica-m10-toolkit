#include "lvf.h"

struct preset { int32_t a[8]; struct lvf_rect gate; };
static const struct preset presets[8] = {
    {{-1,-1,88,88,-1,88,720,304}, {0,90,720,300}},
    {{-1,-1,44,44,-1,44,720,392}, {0,46,720,388}},
    {{-1,-1,36,36,-1,36,720,408}, {0,38,720,404}},
    {{38,38,-1,-1,38,-1,644,480}, {40,0,640,480}},
    {{118,118,138,138,118,138,484,204}, {120,140,480,200}},
    {{118,118,108,108,118,108,484,264}, {120,110,480,260}},
    {{118,118,103,103,118,103,484,274}, {120,105,480,270}},
    {{145,145,78,78,145,78,430,324}, {147,80,426,320}}
};
static const uint8_t line_role[LVF_LINES] = {LVF_TOP,LVF_BOTTOM,LVF_LEFT,LVF_RIGHT};

static int32_t scaled(int32_t v, int32_t scale) { return v * scale / 480; }
static int32_t even_dimension(uint16_t v) {
    /* Exhaustively checked against pinned binary32 /100,*extent,/mm oracle
       for every legal value (width 1..3600, height 1..2400). */
    int32_t n = ((int32_t)v + 2) / 5;
    return n + (n & 1);
}
static int valid_display(const struct lvf_display *d) {
    int32_t s = d->device_code == 2U ? 682 : 480;
    return d->qualified == 1U && d->device_code <= 2U &&
        d->orientation == LVF_UPRIGHT && d->logical_width == 720U &&
        d->logical_height == 480U && d->scale == s &&
        d->pixel_width == scaled(720,s) && d->pixel_height == s;
}
static int valid_descriptions(const struct lvf_backend *b, void *ctx,
                              const struct lvf_binding *binding) {
    static const uint8_t parent[LVF_OBJECTS] = {0,0,1,1,0,4,4};
    static const uint8_t types[LVF_OBJECTS] = {6,6,0,0,6,0,0};
    static const int32_t w[LVF_OBJECTS] = {720,0,720,720,0,2,2};
    static const int32_t h[LVF_OBJECTS] = {480,0,2,2,0,480,480};
    unsigned i, j;
    for (i=0; i<LVF_OBJECTS; ++i) {
        struct lvf_description d = {0};
        if (!binding->object[i]) return 0;
        for (j=0; j<i; ++j)
            if (binding->object[i] == binding->object[j]) return 0;
        if (b->inspect(ctx,binding->object[i],&d) != 0 ||
            d.native_id != 222U+i ||
            d.parent_native_id != (i == LVF_ROOT ? 0U : 222U+parent[i]) ||
            d.native_type != types[i] ||
            d.anchor_x != (i == LVF_RIGHT ? 2U : 0U) ||
            d.anchor_y != (i == LVF_ROOT ? 1U : (i == LVF_BOTTOM ? 2U : 0U)) ||
            (i != LVF_ROOT && d.parent != binding->object[parent[i]]) ||
            (i == LVF_ROOT && !d.parent) || d.width != w[i] || d.height != h[i] ||
            d.relative_width != ((i == 1U || i == 4U) ? 10000 : 0) ||
            d.relative_height != ((i == 1U || i == 4U) ? 10000 : 0)) return 0;
        if (i == LVF_ROOT)
            for (j=0; j<LVF_OBJECTS; ++j)
                if (d.parent == binding->object[j]) return 0;
    }
    return 1;
}
static enum lvf_status compose(const struct lvf_config *c, int32_t scale,
                                struct lvf_frame *f) {
    struct preset p;
    int32_t bottom_w, right_h, thick, pw, ph;
    unsigned i;
    if (c->enabled > 1U || c->mode > LVF_USER2 || c->preset >= 8U) return LVF_ARGUMENT;
    for (i=0; i<2U; ++i)
        if (c->user_width[i] < 1U || c->user_width[i] > 3600U ||
            c->user_height[i] < 1U || c->user_height[i] > 2400U) return LVF_ARGUMENT;
    if (c->mode == LVF_PRESET) p = presets[c->preset];
    else {
        unsigned u = c->mode == LVF_USER1 ? 0U : 1U;
        int32_t w = even_dimension(c->user_width[u]);
        int32_t h = even_dimension(c->user_height[u]);
        int32_t mx = (720-w)/2, my = (480-h)/2;
        int32_t x = mx ? mx-2 : -1, y = my ? my-2 : -1;
        struct preset user = {{x,x,y,y,x,y,w+4,h+4},{mx,my,w,h}};
        p = user;
    }
    bottom_w = p.a[6]; right_h = p.a[7];
    if (scale == 682 && c->mode == LVF_PRESET) {
        if (c->preset < 3U) bottom_w = 721;
        else if (c->preset == 3U) { bottom_w = 645; right_h = 481; }
        else if (c->preset < 7U) bottom_w = 485;
        else right_h = 325;
    }
    f->generation = c->generation; f->nominal_gate = p.gate;
    /* Strict inner aperture stays in logical coordinates; nominal gate is not
       replaced by full-frame boundary strips or a sensor/JPEG ROI. */
    {
        int32_t x = p.a[1]+2, y = p.a[2]+2;
        int32_t rx = 720-p.a[4]-2, by = 480-p.a[5]-2;
        if (x < 0) x=0;
        if (y < 0) y=0;
        if (rx > 720) rx=720;
        if (by > 480) by=480;
        f->strict_aperture = (struct lvf_rect){x,y,rx-x,by-y};
    }
    f->line[0] = (struct lvf_line){p.a[0],p.a[2],p.a[6],2,{0,0,0,0}};
    f->line[1] = (struct lvf_line){p.a[0],p.a[5],bottom_w,2,{0,0,0,0}};
    f->line[2] = (struct lvf_line){p.a[1],p.a[3],2,p.a[7],{0,0,0,0}};
    f->line[3] = (struct lvf_line){p.a[4],p.a[3],2,right_h,{0,0,0,0}};
    thick = scaled(2,scale); pw=scaled(720,scale); ph=scaled(480,scale);
    for (i=0; i<LVF_LINES; ++i) {
        struct lvf_line *l=&f->line[i];
        l->pixels = (struct lvf_rect){scaled(l->x,scale),scaled(l->y,scale),
                                     scaled(l->width,scale),scaled(l->height,scale)};
    }
    f->line[1].pixels.y=ph-scaled(p.a[5],scale)-thick;
    f->line[3].pixels.x=pw-scaled(p.a[4],scale)-thick;
    return LVF_OK;
}
enum lvf_status lvf_open(struct lvf *s, const struct lvf_backend *backend,
                         void *ctx, const struct lvf_binding *binding) {
    struct lvf_backend b;
    struct lvf_binding q;
    uint64_t token=0;
    if (!s || !backend || !binding) return LVF_ARGUMENT;
    if (s->busy) return LVF_BUSY;
    if (s->opened) return LVF_LEASE;
    b=*backend; q=*binding;
    if (!b.inspect || !b.begin || !b.check || !b.visible || !b.apply ||
        !b.redraw || !b.restore_release) return LVF_DEPENDENCY;
    if (!valid_display(&q.display)) return LVF_DISPLAY;
    s->busy=1;
    if (!valid_descriptions(&b,ctx,&q)) { s->busy=0; return LVF_BINDING; }
    if (b.begin(ctx,&q,&token) != 0 || token == 0U) {
        s->busy=0; return LVF_LEASE;
    }
    s->backend=b; s->binding=q; s->context=ctx; s->lease=token;
    s->opened=1; s->recovery=0; s->busy=0;
    return LVF_OK;
}
static enum lvf_status hide(struct lvf *s, enum lvf_status status) {
    int a=s->backend.visible(s->context,s->lease,s->binding.object[LVF_ROOT],0);
    int b=s->backend.redraw(s->context,s->lease,s->binding.object[LVF_ROOT]);
    if (a != 0 || b != 0) { s->recovery=1; return LVF_RECOVERY; }
    return status;
}
enum lvf_status lvf_draw(struct lvf *s, const struct lvf_config *snapshot,
                         struct lvf_frame *out) {
    struct lvf_config c;
    struct lvf_frame frame = {0};
    enum lvf_status result;
    unsigned i;
    if (!s || !snapshot) return LVF_ARGUMENT;
    if (s->busy) return LVF_BUSY;
    if (!s->opened) return LVF_LEASE;
    if (s->recovery) return LVF_RECOVERY;
    c=*snapshot; /* before first callback; caller must deliver coherent copy */
    s->busy=1;
    if (s->backend.check(s->context,s->lease) != 0) {
        /* Lost ownership: do not write a now-unowned handle. */
        s->recovery=1; s->busy=0; return LVF_LEASE;
    }
    result=compose(&c,s->binding.display.scale,&frame);
    if (result != LVF_OK || !c.enabled) {
        result=hide(s,result); s->busy=0; return result;
    }
    if (s->backend.visible(s->context,s->lease,s->binding.object[LVF_ROOT],0) != 0)
        goto io_fail;
    for (i=0; i<LVF_LINES; ++i)
        if (s->backend.apply(s->context,s->lease,s->binding.object[line_role[i]],
                              &frame.line[i],c.color) != 0) goto io_fail;
    for (i=1; i<LVF_OBJECTS; ++i)
        if (s->backend.visible(s->context,s->lease,s->binding.object[i],1) != 0)
            goto io_fail;
    if (s->backend.visible(s->context,s->lease,s->binding.object[LVF_ROOT],1) != 0 ||
        s->backend.redraw(s->context,s->lease,s->binding.object[LVF_ROOT]) != 0)
        goto io_fail;
    if (out) *out=frame;
    s->busy=0; return LVF_OK;
io_fail:
    result=hide(s,LVF_IO); s->busy=0; return result;
}
enum lvf_status lvf_close(struct lvf *s) {
    enum lvf_status result;
    if (!s) return LVF_ARGUMENT;
    if (s->busy) return LVF_BUSY;
    if (!s->opened) return LVF_OK;
    s->busy=1;
    if (s->backend.check(s->context,s->lease) != 0) {
        s->recovery=1; s->busy=0; return LVF_LEASE;
    }
    result=hide(s,LVF_OK);
    if (s->backend.restore_release(s->context,s->lease) != 0) {
        s->recovery=1; s->busy=0; return LVF_RECOVERY;
    }
    if (result == LVF_RECOVERY) result=LVF_IO; /* restore completed recovery */
    s->opened=0; s->recovery=0; s->lease=0; s->busy=0;
    return result;
}
