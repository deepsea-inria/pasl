/*
 * Internal __jmp_buf layout
 */
#define JB_RBX  0
#define JB_RBP  1
#define JB_R12  2
#define JB_R13  3
#define JB_R14  4
#define JB_R15  5
#define JB_RSP  6
#define JB_PC   7

        .file "native.S"
        .text

        /* _pasl_cxt_save(__jmp_buf env) */
#ifdef TARGET_MAC_OS
.globl __pasl_cxt_save
        .align 4
#else
.globl _pasl_cxt_save
        .type _pasl_cxt_save, @function
        .align 16
#endif
#ifdef TARGET_MAC_OS
__pasl_cxt_save:
#else
_pasl_cxt_save:
#endif
        /*
         * Save registers.
         */
        movq %rbx, (JB_RBX*8)(%rdi)
        movq %rbp, (JB_RBP*8)(%rdi)
        movq %r12, (JB_R12*8)(%rdi)
        movq %r13, (JB_R13*8)(%rdi)
        movq %r14, (JB_R14*8)(%rdi)
        movq %r15, (JB_R15*8)(%rdi)
        /* Save SP */
        leaq 8(%rsp), %rdx
        movq %rdx, (JB_RSP*8)(%rdi)
        /* Save PC we are returning to */
        movq (%rsp), %rax
        movq %rax, (JB_PC*8)(%rdi)
        xorq %rax, %rax
        ret
#ifndef TARGET_MAC_OS
        .size _pasl_cxt_save, .-_pasl_cxt_save
#endif


/****************************************************************/

        /* _pasl_cxt_restore(__jmp_buf env, void* val) */
#ifdef TARGET_MAC_OS
.globl __pasl_cxt_restore
        .align 4
#else
.globl _pasl_cxt_restore
        .type _pasl_cxt_restore, @function
        .align 16
#endif
#ifdef TARGET_MAC_OS
__pasl_cxt_restore:
#else        
_pasl_cxt_restore:
#endif
        /*
         * Restore registers.
         */
        movq (JB_RBX*8)(%rdi), %rbx
        movq (JB_RBP*8)(%rdi), %rbp
        movq (JB_R12*8)(%rdi), %r12
        movq (JB_R13*8)(%rdi), %r13
        movq (JB_R14*8)(%rdi), %r14
        movq (JB_R15*8)(%rdi), %r15
        /* Set return value */
        test %rsi, %rsi
        mov $01, %rax
        cmove %rax, %rsi
        mov %rsi, %rax
        movq (JB_PC*8)(%rdi), %rdx
        movq (JB_RSP*8)(%rdi), %rsp
        /* Jump to saved PC */
        jmpq *%rdx
#ifndef TARGET_MAC_OS
        .size _pasl_cxt_restore, .-_pasl_cxt_restore
#endif