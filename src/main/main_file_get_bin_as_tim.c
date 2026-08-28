
void* main_file_get_bin_as_tim(int sector, int size) {
    void* result = main_file_get_tim(sector, size);

    if (result == 0) {
        main_system_handle_animation_exception(2);
    }
    return result;
}
