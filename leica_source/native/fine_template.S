/* NOT-INSTALLABLE source fragment. Ordinary M10 4.26.36.10 ONLY.
 * Text origin is421A5CCC; first2 bytes excluded, so entry421A5CCE has
 * the true VMA alignment. No target execution or image is authorized.
 * K_R/K_G/K_B are immutable build-time integer literals, never runtime config.
 */
.syntax unified
.cpu arm926ej-s
.thumb
.section .text,"ax",%progbits
.space 2
.Lentry:
 push {r2,r3,r5,r7,lr}
 sub sp,#16
 ldr r0,.Lmatrix_offset
 adds r0,r4,r0
 movs r1,#1
 movs r2,#1
 movs r3,#0
 movs r5,#1
 movs r7,#0
 /* Same eighteen ordered words, packed as 5+3+5+3+2. */
 stmia r0!,{r1,r2,r3,r5,r7}
 stmia r0!,{r1,r3,r5}
 stmia r0!,{r1,r2,r3,r5,r7}
 stmia r0!,{r1,r3,r5}
 stmia r0!,{r1,r2}
 movs r5,r0
 movs r7,#1
 lsls r7,r7,#8
 /* Recreate original stores/reads in their exact native order. */
 str r7,[r5,#0]
 movs r0,#215
 lsls r0,r0,#2
 adds r0,r6,r0
 movs r3,#0
 ldrsh r1,[r0,r3]
 str r1,[r5,#4]
 str r7,[r5,#8]
 movs r3,#2
 ldrsh r2,[r0,r3]
 str r2,[r5,#12]
 str r7,[r5,#16]
 movs r3,#4
 ldrsh r3,[r0,r3]
 str r3,[r5,#20]
 cmp r1,#0
 ble .Lexit
 cmp r2,#0
 ble .Lexit
 cmp r3,#0
 ble .Lexit
 movs r7,r2
 /* Reject optional arithmetic if destination24 overlaps declared S[0..718).
  * Native original output already exists; this adds no write on rejection. */
 movs r0,r5
 subs r0,r0,r6
 movs r3,#227
 lsls r3,r3,#3
 cmp r0,r3
 blo .Lexit
 movs r0,r6
 subs r0,r0,r5
 cmp r0,#24
 blo .Lexit
 ldr r0,.Lk_g
 muls r7,r0,r7
 ldr r0,.Lk_r
 muls r1,r0,r1
 movs r0,r7
 bl .Lreduce
 str r0,[sp,#0]
 str r1,[sp,#4]
 ldr r1,[r5,#20]
 ldr r0,.Lk_b
 muls r1,r0,r1
 movs r0,r7
 bl .Lreduce
 str r0,[sp,#8]
 str r1,[sp,#12]
 /* All4 non-green words now staged. No fallible operation remains. */
 ldr r0,[sp,#0]
 ldr r1,[sp,#4]
 str r0,[r5,#0]
 str r1,[r5,#4]
 movs r0,#1
 str r0,[r5,#8]
 str r0,[r5,#12]
 ldr r0,[sp,#8]
 ldr r1,[sp,#12]
 str r0,[r5,#16]
 str r1,[r5,#20]
.Lexit:
 add sp,#16
 pop {r2,r3,r5,r7}
 pop {r0}
 mov lr,r0
 b .Lcontinue
 /* Positive exact u32 pair reduction; preserve all r4-r7. */
.Lreduce:
 push {r4,r5,r6,lr}
 movs r4,r0
 movs r5,r1
.Lgcd:
 movs r6,r1
 bl .Ldivmod
 movs r0,r6
 cmp r1,#0
 bne .Lgcd
 movs r0,r4
 movs r1,r6
 bl .Ldivmod
 movs r4,r0
 movs r0,r5
 movs r1,r6
 bl .Ldivmod
 movs r1,r0
 movs r0,r4
 pop {r4,r5,r6,pc}
 /* r0=n,r1=d>0 -> r0=quotient,r1=remainder. n,d<=33553408 (<2^25).
  * After any alignment shift d<=2*n<2^26; initially d<2^25.
  * At most25 alignment shifts, then26 division steps including terminal.
  * Remainder only subtracts; no 33-bit intermediate is required. */
.Ldivmod:
 movs r2,#0
 movs r3,#1
.Lalign:
 cmp r1,r0
 bhi .Ldivide
 lsls r1,r1,#1
 lsls r3,r3,#1
 b .Lalign
.Ldivide:
 lsrs r1,r1,#1
 lsrs r3,r3,#1
 beq .Ldone
 cmp r0,r1
 bcc .Ldivide
 subs r0,r0,r1
 orrs r2,r3
 b .Ldivide
.Ldone:
 movs r1,r0
 movs r0,r2
 bx lr
.balign 4
.Lmatrix_offset:
 .word 0x22d08
.Lk_r:
 .word @K_R@
.Lk_g:
 .word @K_G@
.Lk_b:
 .word @K_B@
.org 260,0
.Lcontinue:
