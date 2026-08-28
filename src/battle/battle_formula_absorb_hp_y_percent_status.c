
/* Formula 0x47: absorb (Y)% of max HP, 100% status. */
void battle_formula_absorb_hp_y_percent_status(void) {
    battle_formula_calculate_hp_percent_damage();
    battle_formula_apply_hp_absorption();
    battle_formula_apply_status();
}
