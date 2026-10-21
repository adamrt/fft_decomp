/* SCUS_942.21 0x80011bb0..0x80011bbf (file 0x23b0..0x23bf).
 * a0 points to the word that keeps the game loop's stack pointer: store sp
 * there for system_game_loop_stack_restore, which follows it. The two are one
 * hand-written pair; both copy a0 into t0 first and leave the delay slot empty,
 * which a compiler would not do, and C cannot read sp.
 */
	.set	noreorder
	.set	noat
	.text
	.align	2
	.globl	system_stack_pointer_store
	.ent	system_stack_pointer_store
system_stack_pointer_store:
	addu	$t0, $a0, $zero
	sw	$sp, 0($t0)
	jr	$ra
	nop
	.end	system_stack_pointer_store
	.size	system_stack_pointer_store, . - system_stack_pointer_store
