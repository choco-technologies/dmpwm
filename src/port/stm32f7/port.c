#define DMOD_ENABLE_REGISTRATION    ON
#include "dmpwm_port.h"
#include "dmod.h"
#include <errno.h>

/*
 * No PWM on the STM32 yet (a timer output would do it): every call answers -ENOTSUP, so dmpwm loads, a driver ini that asks for an
 * output fails with a message, and a client that checks the result sees "no PWM". The port exists so that the headers of dmpwm are
 * published for the STM32 families too: the display driver (dmlcdtft) builds for them and uses the ioctl numbering of dmpwm_types.h.
 */

dmod_dmpwm_port_api_declaration(1.0, int, _start, ( const dmpwm_config_t *config ))
{
    DMOD_LOG_ERROR("dmpwm: no PWM on this CPU family (not implemented)\n");
    return -ENOTSUP;
}

dmod_dmpwm_port_api_declaration(1.0, void, _stop, ( int channel ))
{
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_duty, ( int channel, uint16_t duty ))
{
    return -ENOTSUP;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_frequency, ( int channel, uint32_t frequency_hz ))
{
    return -ENOTSUP;
}

dmod_dmpwm_port_api_declaration(1.0, int, _set_enabled, ( int channel, bool enabled ))
{
    return -ENOTSUP;
}

int dmod_init(const Dmod_Config_t *Config)
{
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
