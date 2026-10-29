.syntax unified
.arch armv5te
.thumb
/* In-place static translation. LOOK=0 uses stock condition+0x130/+0x134;
 * LOOK=4/5 is a build-time default only in native B2Y modes 4..7.
 * Entry ABI and ownership are documented in IMPLEMENTATION.md. */
.section .text.cc1,"ax",%progbits
.global cc1
.type cc1,%function
.thumb_func
cc1:
 push {r4,r5,r6,r7,lr}
 sub sp,#100
 movs r4,r0
 movs r6,r1
 movs r5,r4
 adds r5,#212
 ldrh r7,[r4]
 subs r7,#4
 cmp r7,#3
 bhi cc_normal
#if LOOK == 0
 movs r0,#255
 adds r0,#49
 ldr r7,[r4,r0]
#else
 movs r7,#LOOK
#endif
 cmp r7,#4
 beq cc_asc
 cmp r7,#5
 beq cc_asc
cc_normal:
 movs r7,#1
 adr r2,cc_empty
 b cc_cache
cc_asc:
#if LOOK == 0
 movs r2,r4
 adds r2,#255
 adds r2,#53
#else
 adr r2,cc_film
#endif
cc_cache:
 ldr r0,cc_cache_ptr
 ldr r0,[r0]
 cmp r0,r7
 bne cc_reload
 cmp r6,#1
 beq cc_done
cc_reload:
 movs r1,#10
 adr r0,cc_name
 bl ROM_matcher
 cmp r0,#0
 beq cc_missing
 movs r6,r0
 cmp r7,#1
 beq cc_call
 adds r0,#8
 add r1,sp,#52
 movs r2,#9
cc_view_loop:
 ldr r3,[r0]
 str r3,[r1]
 adds r0,#4
 adds r1,#4
 subs r2,#1
 bne cc_view_loop
 ldr r0,[r6,#4]
 str r0,[sp,#88]
 mov r5,sp
cc_call:
 movs r0,r6
 movs r1,#0
 movs r2,r5
 bl ROM_cc1
 ldr r1,cc_cache_ptr
 str r7,[r1]
 b cc_done
cc_missing:
 movs r1,#1
 movs r2,r5
 bl ROM_cc1
 movs r0,#0
 ldr r1,cc_cache_ptr
 str r0,[r1]
cc_done:
 add sp,#100
 pop {r4,r5,r6,r7,pc}
.balign 4
cc_cache_ptr: .word 0x408c51d4
cc_empty: .word 0
cc_name: .asciz "B2Y.bin"
.org 0xa0
cc_film:
#if LOOK == 5
 .asciz "_FILMMODE:ASCCONTEMPORARY"
#else
 .asciz "_FILMMODE:ASCCLASSIC"
#endif
.org 200
.size cc1,.-cc1

.section .text.gamma,"ax",%progbits
.global gamma
.type gamma,%function
.thumb_func
gamma:
 push {r4,r5,r6,r7,lr}
 sub sp,#44
 movs r4,r0
 movs r5,r1
 ldrh r7,[r4]
 movs r6,#0
 movs r0,r7
 subs r0,#4
 cmp r0,#3
 bhi gm_cache
#if LOOK == 0
 movs r0,#255
 adds r0,#49
 ldr r0,[r4,r0]
#else
 movs r0,#LOOK
#endif
 cmp r0,#4
 beq gm_asc_key
 cmp r0,#5
 bne gm_cache
gm_asc_key:
 movs r6,r0
 lsls r0,r0,#8
 orrs r7,r0
gm_cache:
 ldr r0,gm_cache_ptr
 ldrh r0,[r0]
 cmp r0,r7
 bne gm_reload
 cmp r5,#1
 beq gm_done
gm_reload:
 bl ROM_ready
 cmp r0,#1
 bne gm_invalid
 str r7,[sp,#36]
 adds r5,r4,#2
 cmp r6,#0
 beq gm_condition
#if LOOK == 0
 movs r5,r4
 adds r5,#255
 adds r5,#53
#else
 ldr r5,gm_film_ptr
#endif
gm_condition:
 movs r2,r5
 movs r1,#11
 adr r0,gm_name
 bl ROM_matcher
 cmp r0,#0
 beq gm_missing
 str r0,[sp]
 cmp r6,#0
 beq gm_normal_query
 movs r6,#39
 movs r0,#8
 b gm_query_count
gm_normal_query:
 movs r6,#28
 movs r0,#2
gm_query_count:
 str r0,[sp,#40]
 movs r7,#0
 mov r4,sp
 adds r4,#4
gm_query:
 movs r1,r6
 movs r2,r5
 adr r0,gm_name
 bl ROM_matcher
 cmp r0,#0
 beq gm_missing
 str r0,[r4,r7]
 adds r6,#1
 adds r7,#4
 ldr r0,[sp,#40]
 lsls r0,r0,#2
 cmp r7,r0
 bne gm_query
 ldr r0,[sp]
 movs r1,#0
 bl ROM_control
 ldr r0,[sp,#40]
 cmp r0,#2
 beq gm_shared
 movs r6,#0
gm_channel_loop:
 ldr r0,[r4,r6]
 movs r1,#0
 lsrs r2,r6,#3
 adds r2,#1
 movs r3,#4
 tst r6,r3
 bne gm_small
 bl ROM_large_channel
 b gm_next
gm_small:
 bl ROM_small_channel
gm_next:
 adds r6,#4
 cmp r6,#32
 bne gm_channel_loop
 b gm_publish
gm_shared:
 ldr r0,[sp,#4]
 movs r1,#0
 bl ROM_large
 ldr r0,[sp,#8]
 movs r1,#0
 bl ROM_small
gm_publish:
 ldr r0,[sp,#36]
 ldr r1,gm_cache_ptr
 strh r0,[r1]
 b gm_done
gm_missing:
 movs r0,#0
 movs r1,#1
 bl ROM_control
gm_invalid:
 movs r0,#0
 ldr r1,gm_cache_ptr
 strh r0,[r1]
gm_done:
 add sp,#44
 pop {r4,r5,r6,r7,pc}
.balign 4
gm_cache_ptr: .word 0x408c51d8
gm_film_ptr: .word 0x4214e16c
gm_name: .asciz "B2Y.bin"
.org 256
.size gamma,.-gamma

/* Old entries consume r0/r1 only. Internal +2 entries require r2=1..4. */
.macro BANK name,shift,base,large
.section .text.\name,"ax",%progbits
.global \name
.type \name,%function
.thumb_func
\name:
 movs r2,#5
.global \name\()_channel
.thumb_func
\name\()_channel:
 push {r4,lr}
 cmp r1,#0
 bne \name\()_fallback
 cmp r0,#0
 beq \name\()_done
 movs r1,r0
 cmp r2,#5
 bne \name\()_selected
 movs r2,#0
\name\()_selected:
 lsls r2,r2,#\shift
 ldr r0,\name\()_base
 adds r0,r0,r2
 movs r2,#1
 lsls r2,r2,#\shift
 blx ROM_copy
\name\()_done:
 movs r0,#0
 pop {r4,pc}
\name\()_fallback:
 ldr r0,\name\()_mmio
 ldr r1,[r0]
 movs r2,#1
 lsls r2,r2,#24
 bics r1,r2
 str r1,[r0]
 ldr r1,[r0]
 lsls r2,r2,#1
 bics r1,r2
.if \large
 adds r1,r1,r2
.endif
 str r1,[r0]
 movs r0,#1
 pop {r4,pc}
.balign 4
\name\()_base: .word \base
\name\()_mmio: .word 0x20020800
.org 72
.size \name,.-\name
.endm
BANK large,15,0x20a40000,1
BANK small,12,0x20a68000,0
