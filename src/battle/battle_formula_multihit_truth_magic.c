
void battle_formula_multihit_truth_magic(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_calculate_truth_damage();
    }
}
