#define DMOD_ENABLE_REGISTRATION    ON
#include "dmpwm_port.h"
#include "dmod.h"
#include <errno.h>

/*
 * A software stand-in for the PWM peripheral: it keeps what it was told and drives nothing. It exists so that dmpwm can be loaded and
 * tested on the host (dmod_loader), where dmpwm_port has no real peripheral.
 */

#define CHANNELS 8

typedef struct
{
    bool        used;
    uint32_t    frequency_hz;
    uint16_t    duty;
    bool        enabled;
} channel_t_;

static channel_t_ g_channels[CHANNELS];

static bool valid(int channel)
{
    return channel >= 0 && channel < CHANNELS && g_channels[channel].used;
}

dmod_dmpwm_port_api_declaration(1.0, int, _start, ( const dmpwm_config_t *config ))
{
    if (config == NULL)
        return -EINVAL;
    for (int n = 0; n < CHANNELS; n++)
    {
        if (!g_channels[n].used)
        {
            g_channels[n] = (channel_t_){ true, config->frequency_hz, config->duty, config->enabled };
            DMOD_LOG_INFO("dmpwm (host stand-in): output %d on pin %d\n", n, config->pin);
            return n;
        }
    }
    return -ENOMEM;
}

dmod_dmpwm_port_api_declaration(1.0, void, _stop, ( int channel ))
{
    if (valid(channel))
        g_channels[channel].used = false;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_duty, ( int channel, uint16_t duty ))
{
    if (!valid(channel))
        return -EINVAL;
    g_channels[channel].duty = duty;
    return 0;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_frequency, ( int channel, uint32_t frequency_hz ))
{
    if (!valid(channel) || frequency_hz == 0U)
        return -EINVAL;
    g_channels[channel].frequency_hz = frequency_hz;
    return 0;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_enabled, ( int channel, bool enabled ))
{
    if (!valid(channel))
        return -EINVAL;
    g_channels[channel].enabled = enabled;
    return 0;
}

int dmod_init(const Dmod_Config_t *Config)
{
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
