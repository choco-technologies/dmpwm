#ifndef DMPWM_TYPES_H
#define DMPWM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "dmdrvi_ioctl.h"

/** The duty is a 16 bit fraction of the period whatever the resolution of the hardware: 0 is always low, 65535 always high. */
#define DMPWM_DUTY_MAX      65535U

/** Configuration of one PWM output (one section of the ini = one /dev/dmpwm<N>) */
typedef struct
{
    int         pin;                /**< GPIO number of the output */
    uint32_t    frequency_hz;       /**< Frequency of the signal */
    uint8_t     resolution_bits;    /**< Wanted resolution of the duty (the port lowers it when the frequency needs it) */
    uint16_t    duty;               /**< Duty after the start (0 .. DMPWM_DUTY_MAX) */
    bool        inverted;           /**< The signal is low for the duty (and the idle level of a disabled output is high) */
    bool        enabled;            /**< The output runs after the start; otherwise it stays at its idle level */
} dmpwm_config_t;

/** ioctl of /dev/dmpwm<N> (private commands start at DMDRVI_IOCTL_CUSTOM_BASE) */
typedef enum
{
    dmpwm_ioctl_cmd_set_duty = DMDRVI_IOCTL_CUSTOM_BASE,    /**< arg: const uint16_t* (0 .. DMPWM_DUTY_MAX) */
    dmpwm_ioctl_cmd_get_duty,                               /**< arg: uint16_t* */
    dmpwm_ioctl_cmd_set_frequency,                          /**< arg: const uint32_t* (Hz) */
    dmpwm_ioctl_cmd_get_frequency,                          /**< arg: uint32_t* (Hz) */
    dmpwm_ioctl_cmd_set_enabled,                            /**< arg: const bool* */
    dmpwm_ioctl_cmd_get_enabled,                            /**< arg: bool* */

    dmpwm_ioctl_cmd_max
} dmpwm_ioctl_cmd_t;

struct dmdrvi_context;
typedef struct dmdrvi_context *dmdrvi_context_t;

#endif /* DMPWM_TYPES_H */
