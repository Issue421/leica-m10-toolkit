#include "style_framing.h"

static void zero_bytes(void *out, size_t n) {
    uint8_t *p=out;
    while(n!=0U) { *p++=0U; --n; }
}
static int valid_config(const struct sfc_config *c) {
    unsigned i;
    if(c->style_enabled>1U || (unsigned)c->style>SFC_NATIVE_FILM ||
       (unsigned)c->mode>M10_STILLLINK || c->native_film_selection>3U ||
       c->guides.enabled>1U || c->guides.mode>LVF_USER2 || c->guides.preset>=8U)
        return 0;
    for(i=0;i<2U;++i)
        if(c->guides.user_width[i]<1U || c->guides.user_width[i]>3600U ||
           c->guides.user_height[i]<1U || c->guides.user_height[i]>2400U) return 0;
    return 1;
}
static int valid_domains(unsigned domains) { return domains!=0U && (domains&~(unsigned)SFC_BOTH)==0U; }
enum sfc_status sfc_freeze(const struct sfc_config *source,struct sfc_config *out) {
    struct sfc_config c;
    if(!source || !out) return SFC_ARGUMENT;
    c=*source;
    if(!valid_config(&c)) return SFC_ARGUMENT;
    *out=c;
    return SFC_OK;
}
static struct sfc_result result_for(unsigned domains) {
    struct sfc_result r;
    zero_bytes(&r,sizeof r); r.requested=domains;
    return r;
}
static void responsibility(const struct sfc *s,struct sfc_result *r) {
    if(s->style_changed || s->style_needs_restore) r->needs_restore|=SFC_STYLE;
    if(s->guides.opened) r->needs_restore|=SFC_GUIDES;
    r->guides_open=s->guides.opened;
}
static void style_result(struct sfc *s,struct sfc_result *r,m10_result native,m10_look target) {
    r->attempted|=SFC_STYLE; r->style=native;
    if(native.status==M10_OK) {
        r->completed|=SFC_STYLE;
        s->style_changed=(uint8_t)(target!=M10_NORMAL);
        s->style_needs_restore=0U;
    } else if(native.stage>=2U || native.recovery_required) {
        /* Even a successful native abort leaves the pipeline quiescent and
           caches invalid, rather than restoring its former running look. */
        s->style_needs_restore=1U;
    }
}
static void finish_status(struct sfc_result *r) {
    int style_bad=(r->attempted&SFC_STYLE)!=0U && r->style.status!=M10_OK;
    int guides_bad=(r->attempted&SFC_GUIDES)!=0U && r->guides!=LVF_OK;
    if(style_bad || guides_bad)
        r->status=r->completed!=0U ? SFC_PARTIAL : (style_bad?SFC_STYLE_FAILED:SFC_GUIDES_FAILED);
}
struct sfc_result sfc_apply(struct sfc *s,const struct sfc_config *source,unsigned domains,
                          const struct sfc_gui_input *gui,struct lvf_frame *frame) {
    struct sfc_result r=result_for(domains);
    struct sfc_config c;
    struct sfc_gui_input frozen_gui;
    m10_look look;
    if(!s || !valid_domains(domains)) { r.status=SFC_ARGUMENT; return r; }
    if(s->busy) { r.status=SFC_BUSY; responsibility(s,&r); return r; }
    if(sfc_freeze(source,&c)!=SFC_OK) { r.status=SFC_ARGUMENT; responsibility(s,&r); return r; }
    r.generation=c.guides.generation;
    /* Validate/copy caller GUI arguments before the first style callback. Actual
       native qualification stays with the existing owner imports at use time. */
    if((domains&SFC_GUIDES)!=0U && c.guides.enabled && !s->guides.opened) {
        if(!gui || !gui->context) { r.status=SFC_ARGUMENT; responsibility(s,&r); return r; }
        frozen_gui=*gui;
    }
    s->busy=1U;
    if((domains&SFC_STYLE)!=0U) {
        if(c.style_enabled && c.style==SFC_NATIVE_FILM) {
            /* This batch's sole film import is unimplemented and closed:
               stage0/UNQUALIFIED, no effects and no film recovery contract.
               Do not feed it into ASC state/restoration bookkeeping. */
            r.attempted|=SFC_STYLE;
            r.style=sfc_native_film_apply(c.native_film_selection,c.mode);
            goto done;
        }
        look=c.style_enabled ? (m10_look)c.style : M10_NORMAL;
        style_result(s,&r,m10_asc_apply(look,c.mode),look);
        if(r.style.status!=M10_OK) goto done;
    }
    if((domains&SFC_GUIDES)!=0U) {
        r.attempted|=SFC_GUIDES;
        if(!c.guides.enabled) r.guides=lvf_close(&s->guides);
        else {
            if(!s->guides.opened)
                r.guides=lvf_open(&s->guides,m10gui_backend(),frozen_gui.context,&frozen_gui.binding);
            if(r.guides==LVF_OK) r.guides=lvf_draw(&s->guides,&c.guides,frame);
        }
        if(r.guides==LVF_OK) r.completed|=SFC_GUIDES;
    }
done:
    s->busy=0U; finish_status(&r); responsibility(s,&r); return r;
}
struct sfc_result sfc_restore(struct sfc *s,unsigned domains,m10_mode mode) {
    struct sfc_result r=result_for(domains);
    if(!s || !valid_domains(domains) || (unsigned)mode>M10_STILLLINK) {
        r.status=SFC_ARGUMENT; return r;
    }
    if(s->busy) { r.status=SFC_BUSY; responsibility(s,&r); return r; }
    s->busy=1U;
    if((domains&SFC_STYLE)!=0U) {
        if(s->style_changed || s->style_needs_restore)
            style_result(s,&r,m10_asc_apply(M10_NORMAL,mode),M10_NORMAL);
        else r.completed|=SFC_STYLE;
    }
    if((domains&SFC_GUIDES)!=0U) {
        r.attempted|=SFC_GUIDES; r.guides=lvf_close(&s->guides);
        if(r.guides==LVF_OK) r.completed|=SFC_GUIDES;
    }
    s->busy=0U; finish_status(&r); responsibility(s,&r); return r;
}
