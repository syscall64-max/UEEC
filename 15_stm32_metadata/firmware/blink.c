#include <stdint.h>
#define REG32(address) (*(volatile uint32_t *)(address))
static volatile uint32_t milliseconds;

void SysTick_Handler(void)
{
    ++milliseconds;
}

static void wait_ms(uint32_t period)
{
    uint32_t start=milliseconds;
    while ((uint32_t)(milliseconds-start)<period) __asm__ volatile("wfi");
}

int main(void)
{
#if GPIO_F1
    REG32(0x40021018u)|=1u<<(GPIO_PORT_INDEX+2u);
    /* F1 CRL/CRH: push-pull output, 2 MHz. */
    uint32_t shift=(GPIO_PIN%8u)*4u;
    uint32_t address=GPIO_BASE+(GPIO_PIN<8u ? 0u : 4u);
    REG32(address)=(REG32(address)&~(15u<<shift))|(2u<<shift);
#else
    REG32(0x40023830u)|=1u<<GPIO_PORT_INDEX;
    REG32(GPIO_BASE)=(REG32(GPIO_BASE)&~(3u<<(GPIO_PIN*2u)))|(1u<<(GPIO_PIN*2u));
#endif
    REG32(0xe000e014u)=CPU_CLOCK_HZ/1000u-1u;
    REG32(0xe000e018u)=0;
    REG32(0xe000e010u)=7; /* CPU clock, interrupt, enable. */
    uint32_t set=1u<<GPIO_PIN;
    for (;;) {
#if GPIO_F1
        REG32(GPIO_BASE+0x10u)=set;
#else
        REG32(GPIO_BASE+0x18u)=set;
#endif
        wait_ms(BLINK_HALF_PERIOD_MS);
#if GPIO_F1
        REG32(GPIO_BASE+0x10u)=set<<16;
#else
        REG32(GPIO_BASE+0x18u)=set<<16;
#endif
        wait_ms(BLINK_HALF_PERIOD_MS);
    }
}
