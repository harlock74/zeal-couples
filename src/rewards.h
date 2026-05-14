#pragma once

#include <stdint.h>

/* Number of staged reward states expected by future reward presentation flow. */
#define REWARD_STAGE_COUNT 6

/* Reward-state API kept modular even while implementation is stubbed. */
void rewards_reset(void);
void rewards_register_hand_result(uint16_t win_amount);
uint8_t rewards_has_pending(void);
uint8_t rewards_pending_stage(void);
uint8_t rewards_is_final_stage_pending(void);
uint8_t rewards_consume_pending_stage(void);
void rewards_clear_pending(void);
