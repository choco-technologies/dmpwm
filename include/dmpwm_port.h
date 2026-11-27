#ifndef DMPWM_PORT_H
#define DMPWM_PORT_H

#include "dmod_types.h"
#include "dmpwm_port_defs.h"
#include "dmpwm_types.h"

/*
 * The port only drives the peripheral (ESP32-S3: LEDC). The configuration, the ini, the dmdrvi device and the
 * ioctl live in the core module (src/dmpwm.c). A channel is what _start() returns: a number >= 0.
 */

/** Starts an output, returns its channel or -errno (-ENOMEM: no free channel or timer, -EINVAL: no such pin or frequency) */
dmod_dmpwm_port_api(1.0, int, _start, ( const dmpwm_config_t *config ) );

/** Stops the output and releases the channel (the pin is left low) */
dmod_dmpwm_port_api(1.0, void, _stop, ( int channel ) );

dmod_dmpwm_port_api(1.0, int, _set_duty, ( int channel, uint16_t duty ) );

/** Changes the frequency; the duty stays the same fraction of the period */
dmod_dmpwm_port_api(1.0, int, _set_frequency, ( int channel, uint32_t frequency_hz ) );

/** Disabled: the pin stays at the idle level (low, high when inverted) */
dmod_dmpwm_port_api(1.0, int, _set_enabled, ( int channel, bool enabled ) );

#endif // DMPWM_PORT_H
