
void attack_thread_stop_after_yield(void) {
    battle_thread_yield();
    battle_thread_exit_current();
}
