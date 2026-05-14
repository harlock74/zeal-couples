#include <stdint.h>

#include "rewards.h"

/* Reward pipeline is intentionally stubbed until reward UI/economy is enabled. */
void rewards_reset(void)
{
}

void rewards_register_hand_result(uint16_t win_amount)
{
    (void)win_amount;
}

uint8_t rewards_has_pending(void)
{
    /* No reward states generated in current solitaire scope. */
    return 0;
}

uint8_t rewards_pending_stage(void)
{
    /* No active stage while subsystem is disabled. */
    return 0;
}

uint8_t rewards_is_final_stage_pending(void)
{
    /* Always false for stub implementation. */
    return 0;
}

uint8_t rewards_consume_pending_stage(void)
{
    /* Nothing to consume yet. */
    return 0;
}

void rewards_clear_pending(void)
{
}
