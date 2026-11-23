/* SCUS_942.21 0x80011bc0..0x80011bcf (file 0x23c0..0x23cf).
 * a0 points to a saved stack address. Restore sp and jump to system_run_game_loop
 * without changing ra. Assembly because replacing $sp is a stack-management
 * ABI boundary that C cannot express; system_store_stack_pointer (stack_save.s)
 * is the other half of the pair.
 */
	.set	noreorder
	.set	noat
	.text
	.align	2
	.globl	system_restore_game_loop_stack
	.ent	system_restore_game_loop_stack
system_restore_game_loop_stack:
	addu	$t0, $a0, $zero
	lw	$sp, 0($t0)
	j	system_run_game_loop
	nop
	.end	system_restore_game_loop_stack
	.size	system_restore_game_loop_stack, . - system_restore_game_loop_stack
