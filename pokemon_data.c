#include "pokemon_data.h"
#include <string.h> // For memset, strncpy, strlen
#include <stdio.h>  // For NULL (though not strictly needed for this file)
// No math.h needed as calculations are pre-simplified for L1 0EV/0IV Magikarp

// Helper to set 3-byte experience (little-endian)
static void set_experience(uint8_t exp_field[3], uint32_t experience) {
    exp_field[0] = (uint8_t)(experience & 0xFF);
    exp_field[1] = (uint8_t)((experience >> 8) & 0xFF);
    exp_field[2] = (uint8_t)((experience >> 16) & 0xFF);
}

void create_tradeable_magikarp_lvl1(Gen1PokemonPartyData* pkm, const char* ot_name_param, uint16_t ot_id_param) {
    // Zero out the entire struct initially to catch any unassigned fields.
    // Specific fields will be explicitly set.
    memset(pkm, 0, sizeof(Gen1PokemonPartyData));

    // Set Species ID
    pkm->species_id = MAGIKARP_ID;

    // Set Level (both level fields)
    pkm->level = 1;
    pkm->level_in_box = 1;

    // Set Moves (Splash, 0, 0, 0)
    pkm->move1_id = SPLASH_ID;
    pkm->move2_id = 0; // No move
    pkm->move3_id = 0; // No move
    pkm->move4_id = 0; // No move

    // Set PP for moves (Splash has 40 PP base)
    // Lower 6 bits for current PP, upper 2 bits for PP ups (0 ups = 0b00xxxxxx)
    pkm->pp_move1 = 40; 
    pkm->pp_move2 = 0;
    pkm->pp_move3 = 0;
    pkm->pp_move4 = 0;

    // Set Original Trainer ID (RP2040 is little-endian, direct assignment is fine)
    pkm->ot_id = ot_id_param;

    // Set Experience Points (L1 Magikarp = 0 for Medium Slow group)
    set_experience(pkm->exp_points, 0);

    // Set Stat Experience (EVs) - all to 0
    pkm->hp_ev = 0;
    pkm->attack_ev = 0;
    pkm->defense_ev = 0;
    pkm->speed_ev = 0;
    pkm->special_ev = 0;

    // Set Individual Values (IVs) - all to 0 for simplicity
    // IVs are stored as: Attack (bits 12-15), Defense (8-11), Speed (4-7), Special (0-3)
    // HP IV is derived from these (highest bit of each). All 0s = HP IV 0.
    pkm->iv_data = 0; 

    // Set stats for Level 1 Magikarp with 0 IVs and 0 EVs
    // Base Stats: HP:20, Atk:10, Def:55, Spd:80, Spc:20
    pkm->max_hp = 11; 
    pkm->attack = 5;   
    pkm->defense = 6;  
    pkm->speed = 6;    
    pkm->special = 5;  

    // Set Current HP to Max HP
    pkm->current_hp = pkm->max_hp;

    // Set Types (Magikarp is Water type)
    pkm->type1 = TYPE_WATER;
    pkm->type2 = TYPE_WATER; // Single type Pokémon have both type fields set to the same type

    // Set Catch Rate / Held Item
    // For trades, this field is used for Held Item. 0 means no held item.
    pkm->catch_rate_held_item = 0; 

    // Set Status Condition (0 for no status ailment)
    pkm->status_condition = 0;

    // Set Nickname: "MAGIKARP" padded with PKMN_TERMINATOR
    // Ensure the buffer is filled with terminators first.
    memset(pkm->nickname, PKMN_TERMINATOR, GEN1_POKEMON_NICKNAME_LENGTH);
    // Copy the name, ensuring not to write past the buffer (excluding terminator space if name is max len)
    // strncpy will not null-terminate if src is >= count. Our memset handles padding.
    const char* magikarp_name = "MAGIKARP";
    size_t magikarp_name_len = strlen(magikarp_name);
    if (magikarp_name_len < GEN1_POKEMON_NICKNAME_LENGTH) {
        memcpy(pkm->nickname, magikarp_name, magikarp_name_len);
    } else {
        memcpy(pkm->nickname, magikarp_name, GEN1_POKEMON_NICKNAME_LENGTH -1);
        pkm->nickname[GEN1_POKEMON_NICKNAME_LENGTH -1] = PKMN_TERMINATOR; // ensure last is terminator
    }


    // Set Original Trainer Name, padded with PKMN_TERMINATOR
    memset(pkm->ot_name, PKMN_TERMINATOR, GEN1_POKEMON_OT_NAME_LENGTH);
    size_t ot_name_len = strlen(ot_name_param);
    if (ot_name_len < GEN1_POKEMON_OT_NAME_LENGTH) {
        memcpy(pkm->ot_name, ot_name_param, ot_name_len);
    } else {
        memcpy(pkm->ot_name, ot_name_param, GEN1_POKEMON_OT_NAME_LENGTH -1);
        pkm->ot_name[GEN1_POKEMON_OT_NAME_LENGTH-1] = PKMN_TERMINATOR; // ensure last is terminator
    }
}
