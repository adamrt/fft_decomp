/* Always 0. Callers poll it after opening music and start the track once it returns 0. */
int main_sound_is_music_loading_stub(void) {
    return 0;
}
