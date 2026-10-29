.syntax unified
.arch armv5te
.thumb
/* Original loader entry and caller ABI. The 0x1e4-byte original owned
 * function and private literal window are the only replacement surface.
 * No target execution. All external symbols resolve to original M10 ROM. */
.section .text.loader,"ax",%progbits
.global loader
.type loader,%function
.thumb_func
loader:
 push {r0,r4,r5,r6,r7,lr}
 sub sp,#32
 movs r7,#0
 ldr r0,count_ptr
 ldrh r0,[r0]
 subs r0,#1
 lsls r0,r0,#16
 asrs r4,r0,#16
 movs r0,#0
 str r0,[sp,#16]
 str r0,[sp,#20]
 b loop_check
/* Recompute the descriptor in exactly the original registry/index domain. */
.macro ENTRY
 movs r5,#33
 lsls r5,r5,#4
 muls r5,r4
 ldr r0,registry_ptr
 ldr r0,[r0]
 adds r5,r5,r0
.endm
loop:
 ENTRY
 movs r1,r5
 adds r1,#8
 movs r2,#255
 adds r2,#1
 ldr r0,[sp,#32]
 bl ROM_strncmp
 cmp r0,#0
 bne early_next
 /* Exact B2Y basename, including NUL; no prefix-only classification. */
 ldr r0,[sp,#32]
 adr r1,b2y_name
 movs r2,#8
 bl ROM_strncmp
 str r0,[sp,#28]
 cmp r0,#0
 bne cache
 /* Reject both words of a mismatched B2Y registration before old RAM ptr. */
 ldr r0,[r5,#4]
 cmp r0,#0
 bne early_done
 ldr r0,[r5]
 ldr r1,expected_length
 cmp r0,r1
 bne early_done
 b cache
early_done:
 b done
early_return:
 b return_entry
early_next:
 b next
cache:
 movs r0,#65
 lsls r0,r0,#3
 ldr r0,[r5,r0]
 cmp r0,#0
 bne early_return
 movs r0,#255
 adds r0,#1
 bl ROM_path_alloc
 str r0,[sp,#24]
 cmp r0,#0
 beq early_done
 adr r1,calib_prefix
 bl ROM_strcpy
 movs r1,r5
 adds r1,#8
 ldr r0,[sp,#24]
 bl ROM_strcat
 ldr r0,[sp,#24]
 bl ROM_nor_lookup
 str r0,[sp,#8]
 cmp r0,#0
 beq file_read
 bl ROM_nor_base
 ldr r2,[sp,#8]
 movs r1,#255
 adds r1,#249
 ldr r1,[r2,r1]
 adds r1,r0,r1
 movs r0,#255
 adds r0,#253
 ldr r0,[r2,r0]
 adds r0,r1,r0
 subs r6,r0,#7
 subs r6,#255
 subs r6,#250
 movs r2,#255
 adds r2,#1
 movs r1,r6
 ldr r0,[sp,#24]
 bl ROM_strncmp
 cmp r0,#0
 bne file_read
 ldr r2,[sp,#8]
 movs r0,#255
 adds r0,#249
 ldr r1,[r6,r0]
 ldr r0,[r2,r0]
 cmp r1,r0
 bne file_read
 movs r0,#255
 adds r0,#253
 ldr r1,[r6,r0]
 ldr r0,[r2,r0]
 cmp r1,r0
 bne file_read
 bl ROM_nor_base
 movs r1,#255
 adds r1,#249
 ldr r1,[r6,r1]
 adds r1,r0,r1
 movs r0,#65
 lsls r0,r0,#3
 str r1,[r5,r0]
 movs r2,r5
 adds r2,#8
 ldr r1,nor_log
 movs r0,#2
 bl ROM_log
 b return_entry
file_read:
 add r0,sp,#16
 str r0,[sp]
 movs r1,r5
 ldmia r1!,{r2,r3}
 str r2,[sp,#8]
 str r3,[sp,#12]
 ldr r0,[sp,#24]
 bl ROM_read_alloc
 movs r6,r0
 ENTRY
 ldr r0,[sp,#28]
 cmp r0,#0
 beq b2y_read
 /* Original non-B2Y publication and short-read behavior. */
 movs r0,#65
 lsls r0,r0,#3
 str r6,[r5,r0]
 b compare_read
b2y_read:
 cmp r6,#0
 beq done
 /* Reconfirm the descriptor source identity before publication. */
 ldr r0,[r5,#4]
 cmp r0,#0
 bne release_new
 ldr r0,[r5]
 ldr r1,expected_length
 cmp r0,r1
 bne release_new
compare_read:
 ldr r0,[r5]
 ldr r1,[sp,#16]
 eors r0,r1
 ldr r1,[r5,#4]
 ldr r2,[sp,#20]
 eors r1,r2
 orrs r0,r1
 beq publish
 ldr r0,[sp,#28]
 cmp r0,#0
 beq release_new
 /* Preserve original non-B2Y mismatch log and returned descriptor. */
 movs r0,r5
 adds r0,#8
 str r0,[sp,#8]
 movs r0,r5
 ldmia r0,{r0,r1}
 str r0,[sp]
 str r1,[sp,#4]
 ldr r1,file_log
 movs r0,#4
 ldr r2,[sp,#16]
 ldr r3,[sp,#20]
 bl ROM_log
 b return_entry
publish:
 movs r0,#65
 lsls r0,r0,#3
 str r6,[r5,r0]
return_entry:
 movs r7,r5
 b done
release_new:
 /* Only the unpublished result from this file allocation is released.
  * r0 is its saved requested low length, r1 its new buffer, matching remove. */
 ldr r0,[sp,#8]
 movs r1,r6
 bl ROM_release
 b done
next:
 subs r0,r4,#1
 lsls r0,r0,#16
 asrs r4,r0,#16
loop_check:
 cmp r4,#0
 blt done
 ENTRY
 adds r5,#8
 cmp r5,#0
 beq done
 b loop
done:
 movs r0,r7
 add sp,#36
 pop {r4,r5,r6,r7,pc}
.balign 4
count_ptr: .word 0x408cb428
registry_ptr: .word 0x408cb424
expected_length: .word 1678832
nor_log: .word 0x4283d220
file_log: .word 0x4283d264
calib_prefix: .asciz "/data/calib/"
.balign 4
b2y_name: .asciz "B2Y.bin"
.org 484
.size loader,.-loader
