#include <avr/io.h>
#include <util/delay.h>

#ifndef LED_BIT
#error Define LED_BIT for the chosen model
#endif
#ifndef BLINK_HALF_PERIOD_MS
#define BLINK_HALF_PERIOD_MS 100
#endif

int main(void)
{
    DDRB |= (uint8_t)_BV(LED_BIT);
    PORTB &= (uint8_t)~_BV(LED_BIT);
    for (;;) {
        PORTB ^= (uint8_t)_BV(LED_BIT);
        _delay_ms(BLINK_HALF_PERIOD_MS);
    }
}
