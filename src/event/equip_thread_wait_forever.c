
void equip_thread_wait_forever(void) {
    for (;;) {
        battle_thread_wait_frames(1);
    }
}
