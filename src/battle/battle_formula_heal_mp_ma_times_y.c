
/* Formula 0x54: heal (MA * Y) MP. */
void battle_formula_heal_mp_ma_times_y(void) {
    battle_formula_store_ma_and_y();
    battle_formula_calculate_magical_xa_times_ya();
    battle_formula_convert_hp_damage_to_mp_recovery();
}
