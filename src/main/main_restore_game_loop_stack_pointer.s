/* SCUS_942.21 0x80011bc0..0x80011bcf (file 0x23c0..0x23cf).
 * a0 points to a saved stack address. Restore sp and jump to main_system_run_game_loop
 * without changing ra. Assembly because replacing $sp is a stack-management
 * ABI boundary that C cannot express.
 */
	.set	noreorder
	.set	noat
	.text
	.align	2
	.globl	main_restore_game_loop_stack_pointer
	.ent	main_restore_game_loop_stack_pointer
main_restore_game_loop_stack_pointer:
	addu	$t0, $a0, $zero
	lw	$sp, 0($t0)
	j	main_system_run_game_loop
	nop
	.end	main_restore_game_loop_stack_pointer
	.size	main_restore_game_loop_stack_pointer, . - main_restore_game_loop_stack_pointer
