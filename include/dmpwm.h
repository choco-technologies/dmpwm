#ifndef DMPWM_H
#define DMPWM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmpwm_defs.h"
#include "dmpwm_types.h"

/**
 * dmpwm: a PWM output as a dmdrvi device (/dev/dmpwm<N>), one per section of the ini:
 *
 *   [backlight_pwm]
 *   driver_name=dmpwm
 *   pin=2
 *   frequency=5000
 *   resolution=10
 *   duty=100         ; percent after the start
 *   inverted=off
 *   enabled=on
 *
 * The device is driven with Dmod_Ioctl(): dmpwm_ioctl_cmd_set_duty and the other commands of dmpwm_types.h.
 */

/** Converts percent (0 .. 100, more is 100) into the duty of the ioctl */
dmod_dmpwm_api(1.0, uint16_t, _percent_to_duty, ( uint8_t percent ));

/** Converts the duty of the ioctl into percent (rounded) */
dmod_dmpwm_api(1.0, uint8_t, _duty_to_percent, ( uint16_t duty ));

/** True when the configuration can be run by a port */
dmod_dmpwm_api(1.0, bool, _validate_config, ( const dmpwm_config_t *config ));

#endif // DMPWM_H
