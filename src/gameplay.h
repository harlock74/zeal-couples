#pragma once

#include <stdint.h>

#define SOLITAIRE_SUIT_COUNT 4
#define SOLITAIRE_RANK_COUNT 9

/*
 * Rank IDs used across the project:
 * A,2,3,4,5,6,J,Q,K
 * (7/8/9/10 are intentionally excluded from this game).
 */
typedef enum {
    SOL_RANK_ACE = 0,
    SOL_RANK_2,
    SOL_RANK_3,
    SOL_RANK_4,
    SOL_RANK_5,
    SOL_RANK_6,
    SOL_RANK_JACK,
    SOL_RANK_QUEEN,
    SOL_RANK_KING
} SolitaireRank;

/* Shuffle the compact deck [0..35] and reset deal cursor. */
void shuffle_deck(void);

/* Draw one card from the deck, reshuffling automatically when exhausted. */
uint8_t pop_deck(void);

/* Extract rank id from compact card id (used by rank-only matching rules). */
uint8_t card_rank_for_match(uint8_t card);
