#define DMOD_ENABLE_REGISTRATION    ON
#include "dmpwm_port.h"
#include "dmod.h"
#include <errno.h>

/*
 * dmpwm port of the ESP32-S3: the LEDC peripheral (low speed group: 4 timers, 8 channels), clocked from the 80 MHz APB. The output goes
 * to the pin through the GPIO matrix. Channels with the same frequency and resolution share a timer. A duty change is written with the
 * "no fade" sequence of the ESP-IDF (the duty, one step of 1 period, the start, then the update of the low speed registers), so it takes
 * effect at the end of the current period - no glitch.
 */

#define SYSTEM_BASE         0x600C0000UL
#define PERIP_CLK_EN0       (SYSTEM_BASE + 0x18UL)
#define PERIP_RST_EN0       (SYSTEM_BASE + 0x20UL)
#define LEDC_BIT            (1UL << 11)
#define LEDC_BASE           0x60019000UL
#define LEDC_CH_CONF0(n)    (LEDC_BASE + 0x14UL * (n) + 0x00UL)
#define LEDC_CH_HPOINT(n)   (LEDC_BASE + 0x14UL * (n) + 0x04UL)
#define LEDC_CH_DUTY(n)     (LEDC_BASE + 0x14UL * (n) + 0x08UL)
#define LEDC_CH_CONF1(n)    (LEDC_BASE + 0x14UL * (n) + 0x0CUL)
#define LEDC_TIMER_CONF(n)  (LEDC_BASE + 0xA0UL + 4UL * (n))
#define LEDC_CONF           (LEDC_BASE + 0xD0UL)
#define LEDC_CLK_EN         (1UL << 31)
#define LEDC_APB_CLK        1UL                 /* APB_CLK_SEL: 80 MHz */
#define APB_HZ              80000000ULL
#define SIG_LEDC_OUT0       73U                 /* LEDC_LS_SIG_OUT0_IDX; channel n is 73 + n */
#define GPIO_BASE           0x60004000UL
#define GPIO_ENABLE_W1TS    (GPIO_BASE + 0x24UL)
#define GPIO_ENABLE1_W1TS   (GPIO_BASE + 0x30UL)
#define GPIO_FUNC_OUT_SEL(n) (GPIO_BASE + 0x554UL + 4UL * (n))
#define IO_MUX_PIN(n)       (0x60009004UL + 4UL * (n))
#define CHANNELS            8
#define TIMERS              4
#define MAX_RESOLUTION      14U
#define MAX_PIN             48

typedef struct
{
    bool        used;
    uint32_t    frequency_hz;
    uint8_t     resolution;
    uint8_t     users;
} timer_t_;

typedef struct
{
    bool        used;
    uint8_t     timer;
    int         pin;
    bool        inverted;
    bool        enabled;
    uint16_t    duty;
} channel_t_;

static timer_t_ g_timers[TIMERS];
static channel_t_ g_channels[CHANNELS];
static bool g_clock_on;

static inline volatile uint32_t *reg(uintptr_t a) { return (volatile uint32_t *)a; }

static void ledc_clock_on(void)
{
    if (g_clock_on)
        return;
    *reg(PERIP_CLK_EN0) |= LEDC_BIT;
    *reg(PERIP_RST_EN0) |= LEDC_BIT;
    *reg(PERIP_RST_EN0) &= ~LEDC_BIT;
    *reg(LEDC_CONF) = LEDC_APB_CLK | LEDC_CLK_EN;
    g_clock_on = true;
    DMOD_LOG_INFO("dmpwm: LEDC clocked from the APB (80 MHz)\n");
}

static void route_pin(int pin, unsigned channel)
{
    volatile uint32_t *mux = reg(IO_MUX_PIN((uint32_t)pin));
    *mux = (*mux & ~(7UL << 12)) | (1UL << 12) | (2UL << 10);       /* GPIO function, drive strength 2 */
    *reg(GPIO_FUNC_OUT_SEL((uint32_t)pin)) = (SIG_LEDC_OUT0 + channel) | (1UL << 10);   /* OEN_SEL: the output enable is the register's */
    if (pin < 32)
        *reg(GPIO_ENABLE_W1TS) = 1UL << pin;
    else
        *reg(GPIO_ENABLE1_W1TS) = 1UL << (pin - 32);
}

/* The divider (10 bit integer part, 8 bit fraction) for the frequency, the resolution lowered until the divider is at least 1 */
static int timer_params(uint32_t frequency_hz, uint8_t *resolution, uint32_t *divider)
{
    if (frequency_hz == 0U)
        return -EINVAL;
    uint8_t res = *resolution;
    if (res < 1U)
        res = 1U;
    if (res > MAX_RESOLUTION)
        res = MAX_RESOLUTION;
    for (; res >= 1U; res--)
    {
        uint64_t div = (APB_HZ << 8) / ((uint64_t)frequency_hz << res);
        if (div >= 256ULL)
        {
            if (div >= (1ULL << 18))
                return -EINVAL;                 /* too slow for the divider */
            *resolution = res;
            *divider = (uint32_t)div;
            return 0;
        }
    }
    return -EINVAL;                             /* too fast */
}

static void timer_write(unsigned timer, uint8_t resolution, uint32_t divider)
{
    uint32_t conf = (uint32_t)resolution | (divider << 4);
    *reg(LEDC_TIMER_CONF(timer)) = conf | (1UL << 23);                   /* held in reset while the divider is written */
    *reg(LEDC_TIMER_CONF(timer)) = conf | (1UL << 25);                   /* PARA_UP: the new divider and resolution */
}

/* A timer of the frequency (the shared one, or a free one configured now) */
static int timer_acquire(uint32_t frequency_hz, uint8_t resolution)
{
    uint32_t divider;
    int ret = timer_params(frequency_hz, &resolution, &divider);
    if (ret != 0)
        return ret;
    int free_timer = -1;
    for (int t = 0; t < TIMERS; t++)
    {
        if (g_timers[t].used && g_timers[t].frequency_hz == frequency_hz && g_timers[t].resolution == resolution)
        {
            g_timers[t].users++;
            return t;
        }
        if (!g_timers[t].used && free_timer < 0)
            free_timer = t;
    }
    if (free_timer < 0)
        return -ENOMEM;
    g_timers[free_timer] = (timer_t_){ true, frequency_hz, resolution, 1U };
    timer_write((unsigned)free_timer, resolution, divider);
    return free_timer;
}

static void timer_release(unsigned timer)
{
    if (g_timers[timer].users > 0U && --g_timers[timer].users == 0U)
    {
        g_timers[timer].used = false;
        *reg(LEDC_TIMER_CONF(timer)) = 1UL << 22;                        /* paused */
    }
}

static uint32_t duty_raw(const channel_t_ *ch)
{
    uint32_t full = 1UL << g_timers[ch->timer].resolution;
    uint32_t raw = ((uint32_t)ch->duty * full + DMPWM_DUTY_MAX / 2U) / DMPWM_DUTY_MAX;
    return ch->inverted ? full - raw : raw;
}

/* All the registers of a channel from its state (the output is held at the idle level when it is disabled) */
static void channel_apply(unsigned n)
{
    const channel_t_ *ch = &g_channels[n];
    uint32_t idle = ch->inverted ? 1UL : 0UL;
    *reg(LEDC_CH_HPOINT(n)) = 0;
    *reg(LEDC_CH_DUTY(n)) = duty_raw(ch) << 4;
    *reg(LEDC_CH_CONF1(n)) = (0UL << 0) | (1UL << 10) | (1UL << 20) | (1UL << 30) | (1UL << 31);     /* scale 0, cycle 1, num 1, inc, start */
    *reg(LEDC_CH_CONF0(n)) = ch->timer | ((ch->enabled ? 1UL : 0UL) << 2) | (idle << 3) | (1UL << 4);   /* ... and PARA_UP */
}

dmod_dmpwm_port_api_declaration(1.0, int, _start, ( const dmpwm_config_t *config ))
{
    if (config == NULL || config->pin < 0 || config->pin > MAX_PIN || config->pin == 19 || config->pin == 20)
        return -EINVAL;                          /* (19 and 20 are the USB pads) */
    int n = -1;
    for (int i = 0; i < CHANNELS && n < 0; i++)
        n = g_channels[i].used ? n : i;
    if (n < 0)
        return -ENOMEM;
    ledc_clock_on();
    int timer = timer_acquire(config->frequency_hz, config->resolution_bits);
    if (timer < 0)
        return timer;
    g_channels[n] = (channel_t_){ true, (uint8_t)timer, config->pin, config->inverted, config->enabled, config->duty };
    channel_apply((unsigned)n);
    route_pin(config->pin, (unsigned)n);
    return n;
}

dmod_dmpwm_port_api_declaration(1.0, void, _stop, ( int channel ))
{
    if (channel < 0 || channel >= CHANNELS || !g_channels[channel].used)
        return;
    g_channels[channel].enabled = false;
    channel_apply((unsigned)channel);
    timer_release(g_channels[channel].timer);
    g_channels[channel].used = false;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_duty, ( int channel, uint16_t duty ))
{
    if (channel < 0 || channel >= CHANNELS || !g_channels[channel].used)
        return -EINVAL;
    g_channels[channel].duty = duty;
    channel_apply((unsigned)channel);
    return 0;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_frequency, ( int channel, uint32_t frequency_hz ))
{
    if (channel < 0 || channel >= CHANNELS || !g_channels[channel].used)
        return -EINVAL;
    channel_t_ *ch = &g_channels[channel];
    uint8_t wanted = g_timers[ch->timer].resolution;
    uint32_t divider;
    uint8_t resolution = wanted;
    int ret = timer_params(frequency_hz, &resolution, &divider);
    if (ret != 0)
        return ret;
    if (g_timers[ch->timer].users == 1U)
    {
        /* the timer is the channel's own: it is reconfigured in place */
        g_timers[ch->timer].frequency_hz = frequency_hz;
        g_timers[ch->timer].resolution = resolution;
        timer_write(ch->timer, resolution, divider);
    }
    else
    {
        int timer = timer_acquire(frequency_hz, wanted);
        if (timer < 0)
            return timer;
        timer_release(ch->timer);
        ch->timer = (uint8_t)timer;
    }
    channel_apply((unsigned)channel);
    return 0;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_enabled, ( int channel, bool enabled ))
{
    if (channel < 0 || channel >= CHANNELS || !g_channels[channel].used)
        return -EINVAL;
    g_channels[channel].enabled = enabled;
    channel_apply((unsigned)channel);
    return 0;
}

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
