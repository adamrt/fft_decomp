
/* Four effect-pool/state initializers, called at effect setup and teardown. */
void battle_effect_reset_subsystems(void) {
    battle_effect_init_render_state();
    battle_effect_init_state_records();
    battle_effect_init_record_chain();
    battle_effect_init_free_list();
}
