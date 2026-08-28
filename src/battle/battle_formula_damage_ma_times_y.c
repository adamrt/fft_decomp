
/* Formula 0x4E: damage (MA * Y). */
void battle_formula_damage_ma_times_y(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_store_ma_and_y();
        battle_formula_calculate_truth_magical_damage();
    }
}
