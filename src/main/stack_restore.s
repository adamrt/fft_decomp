/* SCUS_942.21 0x80011bc0..0x80011bcf (file 0x23c0..0x23cf).
 * a0 points to a saved stack address. Restore sp and jump to system_game_loop_run
 * without changing ra. Assembly because replacing $sp is a stack-management
 * ABI boundary that C cannot express; system_stack_pointer_store (stack_save.s)
 * is the other half of the pair.
 */
	.set	noreorder
	.set	noat
	.text
	.align	2
	.globl	system_game_loop_stack_restore
	.ent	system_game_loop_stack_restore
system_game_loop_stack_restore:
	addu	$t0, $a0, $zero
	lw	$sp, 0($t0)
	j	system_game_loop_run
	nop
	.end	system_game_loop_stack_restore
	.size	system_game_loop_stack_restore, . - system_game_loop_stack_restore
