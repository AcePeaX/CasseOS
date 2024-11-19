#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>


void init_timer(uint32_t freq);

uint64_t timer_get_ticks();

void sleep_ticks(uint64_t ticks_num);
void sleep_ms(uint64_t milliseconds);


#endif
