#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmpwm.h"
#include "dmpwm_port.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <errno.h>
#include <string.h>

#define DMPWM_CONTEXT_MAGIC     0x44504D57U     /* "DPMW" */
#define DMPWM_DEFAULT_FREQUENCY 5000U
#define DMPWM_DEFAULT_RESOLUTION 10U

struct dmdrvi_context
{
    uint32_t        magic;
    dmpwm_config_t  config;
    int             channel;        /* what the port returned */
};

/* ---- helpers ---- */

dmod_dmpwm_api_declaration(1.0, uint16_t, _percent_to_duty, ( uint8_t percent ))
{
    if (percent >= 100U)
        return DMPWM_DUTY_MAX;
    return (uint16_t)(((uint32_t)percent * DMPWM_DUTY_MAX + 50U) / 100U);
}

dmod_dmpwm_api_declaration(1.0, uint8_t, _duty_to_percent, ( uint16_t duty ))
{
    return (uint8_t)(((uint32_t)duty * 100U + DMPWM_DUTY_MAX / 2U) / DMPWM_DUTY_MAX);
}

dmod_dmpwm_api_declaration(1.0, bool, _validate_config, ( const dmpwm_config_t *config ))
{
    return config != NULL && config->pin >= 0 && config->frequency_hz != 0U &&
           config->resolution_bits >= 1U && config->resolution_bits <= 14U;
}

static bool is_valid_context(dmdrvi_context_t context)
{
    return context != NULL && context->magic == DMPWM_CONTEXT_MAGIC;
}

static bool ini_switch(dmini_context_t ini, const char *key, bool fallback)
{
    const char *s = dmini_get_string(ini, NULL, key, NULL);
    if (s == NULL)
        return fallback;
    if (strcmp(s, "on") == 0 || strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0)
        return true;
    if (strcmp(s, "off") == 0 || strcmp(s, "false") == 0 || strcmp(s, "0") == 0 || strcmp(s, "no") == 0)
        return false;
    return fallback;
}

static int read_config(dmpwm_config_t *c, dmini_context_t ini)
{
    c->pin             = (int)dmini_get_int(ini, NULL, "pin", -1);
    c->frequency_hz    = (uint32_t)dmini_get_int(ini, NULL, "frequency", DMPWM_DEFAULT_FREQUENCY);
    c->resolution_bits = (uint8_t)dmini_get_int(ini, NULL, "resolution", DMPWM_DEFAULT_RESOLUTION);
    c->duty            = dmpwm_percent_to_duty((uint8_t)dmini_get_int(ini, NULL, "duty", 100));
    c->inverted        = ini_switch(ini, "inverted", false);
    c->enabled         = ini_switch(ini, "enabled", true);
    if (!dmpwm_validate_config(c))
    {
        DMOD_LOG_ERROR("dmpwm: invalid configuration (pin %d, frequency %u Hz, resolution %u)\n",
                       c->pin, (unsigned)c->frequency_hz, (unsigned)c->resolution_bits);
        return -EINVAL;
    }
    return 0;
}

/* ---- DMDRVI interface ---- */

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if (config == NULL || dev_num == NULL)
        return NULL;

    dmdrvi_context_t context = Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (context == NULL)
        return NULL;
    memset(context, 0, sizeof(*context));
    context->magic = DMPWM_CONTEXT_MAGIC;

    int ret = read_config(&context->config, config);
    if (ret == 0)
    {
        ret = dmpwm_port_start(&context->config);
        if (ret >= 0)
            context->channel = ret;
        else
            DMOD_LOG_ERROR("dmpwm: cannot start the output on pin %d (%d)\n", context->config.pin, ret);
    }
    if (ret < 0)
    {
        context->magic = 0;
        Dmod_Free(context);
        return NULL;
    }

    /* One major number per output -> /dev/dmpwm<channel> */
    dev_num->flags = DMDRVI_NUM_MAJOR;
    dev_num->major = (dmdrvi_dev_id_t)context->channel;
    return context;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, void, _free, ( dmdrvi_context_t context ))
{
    if (!is_valid_context(context))
        return;
    dmpwm_port_stop(context->channel);
    context->magic = 0;
    Dmod_Free(context);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, void*, _open, ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t* dev_num ))
{
    return is_valid_context(context) ? context : NULL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    /* No per-handle state - the output keeps running until the driver is freed. */
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, dmdrvi_ssize_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    return 0;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, dmdrvi_ssize_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    return -ENOTSUP;
}

static int ioctl_set(dmdrvi_context_t context, int command, const void *arg)
{
    dmpwm_config_t *c = &context->config;
    int ret;

    switch (command)
    {
        case dmpwm_ioctl_cmd_set_duty:
            ret = dmpwm_port_set_duty(context->channel, *(const uint16_t *)arg);
            if (ret == 0)
                c->duty = *(const uint16_t *)arg;
            return ret;
        case dmpwm_ioctl_cmd_set_frequency:
            if (*(const uint32_t *)arg == 0U)
                return -EINVAL;
            ret = dmpwm_port_set_frequency(context->channel, *(const uint32_t *)arg);
            if (ret == 0)
                c->frequency_hz = *(const uint32_t *)arg;
            return ret;
        case dmpwm_ioctl_cmd_set_enabled:
            ret = dmpwm_port_set_enabled(context->channel, *(const bool *)arg);
            if (ret == 0)
                c->enabled = *(const bool *)arg;
            return ret;
        default:
            return -ENOTTY;
    }
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    if (!is_valid_context(context))
        return -EINVAL;

    /* Unknown commands - including the standard ones dmdevfs probes every node with - are answered quietly with -ENOTTY. */
    bool ours = command >= (int)dmpwm_ioctl_cmd_set_duty && command < (int)dmpwm_ioctl_cmd_max;
    if (!ours)
        return -ENOTTY;
    if (arg == NULL)
        return -EINVAL;

    switch (command)
    {
        case dmpwm_ioctl_cmd_get_duty:
            *(uint16_t *)arg = context->config.duty;
            return 0;
        case dmpwm_ioctl_cmd_get_frequency:
            *(uint32_t *)arg = context->config.frequency_hz;
            return 0;
        case dmpwm_ioctl_cmd_get_enabled:
            *(bool *)arg = context->config.enabled;
            return 0;
        default:
            return ioctl_set(context, command, arg);
    }
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    return is_valid_context(context) ? 0 : -EINVAL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmpwm, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    if (!is_valid_context(context) || stat == NULL)
        return -EINVAL;
    stat->size = 0;
    stat->mode = 0666;
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
