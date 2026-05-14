#include <stdint.h>

#include <zgdk.h>

#include "main.h"
#include "app_state.h"
#include "gameplay.h"

/* Returns an unbiased random value in [0, max_inclusive]. */
static uint8_t rand_bounded_u8(uint8_t max_inclusive)
{
    uint8_t range = (uint8_t)(max_inclusive + 1U);
    uint8_t limit;
    uint8_t x;

    if (range == 0U) {
        return 0U;
    }

    limit = (uint8_t)(255U - (255U % range));
    do {
        x = rand8();
    } while (x > limit);

    return (uint8_t)(x % range);
}

/*
 * Fisher-Yates shuffle over the compact [0..35] card-id deck.
 * The deck is rebuilt each shuffle so re-seeding + shuffle always starts clean.
 */
void shuffle_deck(void)
{
    for (uint8_t i = 0; i < DECK_SIZE; i++) {
        deck[i] = i;
    }

    for (uint8_t i = DECK_SIZE - 1; i > 0; i--) {
        uint8_t j = rand_bounded_u8(i);
        uint8_t tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }

    deck_pos = 0;
}

/* Deal next card; if exhausted, reshuffle and continue dealing. */
uint8_t pop_deck(void)
{
    if (deck_pos >= DECK_SIZE) {
        shuffle_deck();
    }

    return deck[deck_pos++];
}

uint8_t card_rank_for_match(uint8_t card)
{
    /*
     * Rank IDs are 0..8 => A..6,J,Q,K.
     * Suit is intentionally ignored for match logic.
     */
    return (uint8_t)(card % SOLITAIRE_RANK_COUNT);
}
