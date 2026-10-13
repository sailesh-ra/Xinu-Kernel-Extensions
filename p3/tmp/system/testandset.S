/* testandset.S - test_and_set */

/*------------------------------------------------------------------------
 * test_and_set  -  Atomically exchange a value in memory with a new value
 *
 * C Signature: uint32 test_and_set(uint32 *ptr, uint32 new_value);
 *
 * Conceptually implements (but atomically):
 *     uint32 old_value = *ptr;
 *     *ptr = new_value;
 *     return old_value;
 *------------------------------------------------------------------------
 */
	.text					        /* Code section */
	.globl	test_and_set			/* Make function globally visible to linker */

test_and_set:


        pushl   %ebp                /* Save the old base pointer         */
        movl    %esp, %ebp          /* Save old sp into new base pointer */
        
        /* Main exchange processes */
        movl  12(%ebp), %eax        /* set eax to new_value */
		movl  8(%ebp), %edx         /* set edx to ptr */
		xchgl %eax, (%edx)		    /* exchange the values */

        /* Clean up stack frame */
        movl    %ebp, %esp          /* Restore the stack pointer        */
        popl    %ebp                /* Restore the old base pointer     */

        ret