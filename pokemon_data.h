#ifndef POKEMON_DATA_H
#define POKEMON_DATA_H

#include <stdint.h>

// Constants
#define MAGIKARP_ID 0x81 // 129 decimal
#define SPLASH_ID   0x96 // 150 decimal

#define TYPE_NORMAL   0x00
#define TYPE_FIGHTING 0x01
#define TYPE_FLYING   0x02
#define TYPE_POISON   0x03
#define TYPE_GROUND   0x04
#define TYPE_ROCK     0x05
#define TYPE_BIRD     0x06 // Unused
#define TYPE_BUG      0x07
#define TYPE_GHOST    0x08
#define TYPE_FIRE     0x14
#define TYPE_WATER    0x15
#define TYPE_GRASS    0x16
#define TYPE_ELECTRIC 0x17
#define TYPE_PSYCHIC  0x18
#define TYPE_ICE      0x19
#define TYPE_DRAGON   0x1A

#define GEN1_POKEMON_NICKNAME_LENGTH 11
#define GEN1_POKEMON_OT_NAME_LENGTH  7
#define PKMN_TERMINATOR              0x50

// Structure for Gen 1 Pokémon Party Data (44 bytes for core stats + 11 for nickname + 7 for OT name = 62 bytes total)
// Based on Bulbapedia's "Pokémon data structure (Generation I)"
// All multi-byte fields are little-endian as on the Game Boy.
typedef struct {
    // Core Pokémon Data (offsets 0x00 - 0x2B in party data, total 44 bytes)
    uint8_t species_id;          // 0x00: Index number of the Species
    uint16_t current_hp;         // 0x01: Current HP
    uint8_t level_in_box;        // 0x03: Level (apparently redundant for party, used in PC)
    uint8_t status_condition;    // 0x04: Status condition (e.g., SLP, PSN, BRN, FRZ, PAR)
    uint8_t type1;               // 0x05: Type 1
    uint8_t type2;               // 0x06: Type 2
    uint8_t catch_rate_held_item;// 0x07: Catch rate / Held item (used as Held Item in trades to Gen 2)
    uint8_t move1_id;            // 0x08: Index number of move 1
    uint8_t move2_id;            // 0x09: Index number of move 2
    uint8_t move3_id;            // 0x0A: Index number of move 3
    uint8_t move4_id;            // 0x0B: Index number of move 4
    uint16_t ot_id;              // 0x0C: Original Trainer ID number
    uint8_t exp_points[3];       // 0x0E: Experience points (3 bytes)
    uint16_t hp_ev;              // 0x11: HP stat experience data
    uint16_t attack_ev;          // 0x13: Attack stat experience data
    uint16_t defense_ev;         // 0x15: Defense stat experience data
    uint16_t speed_ev;           // 0x17: Speed stat experience data
    uint16_t special_ev;         // 0x19: Special stat experience data
    uint16_t iv_data;            // 0x1B: IV data (Attack, Defense, Speed, Special - 4 bits each. HP IV is derived)
    uint8_t pp_move1;            // 0x1D: Move 1's PP values (lower 6 bits current PP, upper 2 bits PP Ups)
    uint8_t pp_move2;            // 0x1E: Move 2's PP values
    uint8_t pp_move3;            // 0x1F: Move 3's PP values
    uint8_t pp_move4;            // 0x20: Move 4's PP values
    
    // Party-specific stats (calculated, follows the 33 bytes above)
    uint8_t level;               // 0x21: Current Level (this is the one used in battles)
    uint16_t max_hp;             // 0x22: Maximum HP
    uint16_t attack;             // 0x24: Attack
    uint16_t defense;            // 0x26: Defense
    uint16_t speed;              // 0x28: Speed
    uint16_t special;            // 0x2A: Special
    // End of 44-byte core data block

    // Nickname and OT Name are part of the trade block structure
    char nickname[GEN1_POKEMON_NICKNAME_LENGTH]; // Pokémon Nickname (10 chars + terminator 0x50)
    char ot_name[GEN1_POKEMON_OT_NAME_LENGTH];   // Original Trainer Name (6 chars + terminator 0x50)

} Gen1PokemonPartyData;

// Function declaration
void create_tradeable_magikarp_lvl1(Gen1PokemonPartyData* pkm, const char* ot_name_param, uint16_t ot_id_param);

#endif // POKEMON_DATA_H
