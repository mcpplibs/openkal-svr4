/* One system call of System V on i386.
 *
 *   long oks_call(long nr, long a, long b, long c, long d, long e, long f,
 *                 long* second, long* error);
 *
 * THE ENTRY IS THE CALL GATE, `lcall $7,$0': the number in %eax, the arguments
 * on the stack beneath one slot (the return address a C library's stub would
 * have left there). The kernel reports a failure by the carry flag, with the
 * error number in %eax; a success leaves the carry clear, the result in %eax
 * and a second result, where there is one (fork, wait, pipe), in %edx.
 *
 * The two outcomes are kept apart rather than folded into one value: a result
 * is a whole word here (an address above 2 GiB is a mapping's), so no value of
 * the result can stand for a failure.
 */
    .text
    .globl oks_call
    .type  oks_call, @function
oks_call:
    pushl %ebp
    movl  %esp, %ebp
    pushl 32(%ebp)
    pushl 28(%ebp)
    pushl 24(%ebp)
    pushl 20(%ebp)
    pushl 16(%ebp)
    pushl 12(%ebp)
    pushl $0
    movl  8(%ebp), %eax
    lcall $7, $0
    movl  40(%ebp), %ecx
    jnc   1f
    movl  %eax, (%ecx)
    xorl  %eax, %eax
    jmp   2f
1:  movl  $0, (%ecx)
2:  movl  36(%ebp), %ecx
    testl %ecx, %ecx
    jz    3f
    movl  %edx, (%ecx)
3:  leave
    ret
    .size oks_call, .-oks_call
    .section .note.GNU-stack,"",@progbits
