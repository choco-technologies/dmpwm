#include "dmod.h"
#include "dmosi.h"
#include "dmpwm.h"
#include <string.h>

/* Manual dmpwm test tool: works on an already configured /dev/dmpwm<N> node, through ioctl only. */

#define DEFAULT_DEVICE  "/dev/dmpwm0"

static void print_usage(const char *name)
{
    Dmod_Printf("Usage: %s [-d DEVICE] COMMAND [ARGS]\n", name);
    Dmod_Printf("  info               print duty, frequency and state\n");
    Dmod_Printf("  duty PERCENT       set the duty (0 .. 100)\n");
    Dmod_Printf("  freq HZ            set the frequency\n");
    Dmod_Printf("  on | off           enable or disable the output\n");
    Dmod_Printf("  sweep              duty 0 -> 100 -> 0 in steps, to watch a backlight or a LED\n");
}

/* The module has no libc: a decimal number from a string */
static uint32_t parse_number(const char *s)
{
    uint32_t value = 0;
    for (; *s >= '0' && *s <= '9'; s++)
        value = value * 10U + (uint32_t)(*s - '0');
    return value;
}

static int cmd_info(void *fp)
{
    uint16_t duty = 0;
    uint32_t frequency = 0;
    bool enabled = false;
    if (Dmod_Ioctl(fp, dmpwm_ioctl_cmd_get_duty, &duty) != 0 || Dmod_Ioctl(fp, dmpwm_ioctl_cmd_get_frequency, &frequency) != 0 ||
        Dmod_Ioctl(fp, dmpwm_ioctl_cmd_get_enabled, &enabled) != 0)
    {
        Dmod_Printf("pwmtest: cannot read the state\n");
        return 1;
    }
    Dmod_Printf("duty %u%% (%u), %u Hz, %s\n", (unsigned)dmpwm_duty_to_percent(duty), (unsigned)duty, (unsigned)frequency,
                enabled ? "enabled" : "disabled");
    return 0;
}

static int set_duty(void *fp, uint8_t percent)
{
    uint16_t duty = dmpwm_percent_to_duty(percent);
    return Dmod_Ioctl(fp, dmpwm_ioctl_cmd_set_duty, &duty);
}

static int cmd_sweep(void *fp)
{
    for (int i = 0; i <= 40; i++)
    {
        int percent = i <= 20 ? i * 5 : (40 - i) * 5;
        if (set_duty(fp, (uint8_t)percent) != 0)
            return 1;
        Dmod_Printf("%d%%\n", percent);
        dmosi_thread_sleep(150);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *device = DEFAULT_DEVICE;
    int arg = 1;
    if (argc > 2 && strcmp(argv[1], "-d") == 0)
    {
        device = argv[2];
        arg = 3;
    }
    if (arg >= argc)
    {
        print_usage(argv[0]);
        return 1;
    }
    void *fp = Dmod_FileOpen(device, "r+");
    if (fp == NULL)
    {
        Dmod_Printf("pwmtest: cannot open %s\n", device);
        return 1;
    }
    const char *cmd = argv[arg];
    int ret = 0;
    bool flag;
    uint32_t frequency;

    if (strcmp(cmd, "info") == 0)
        ret = cmd_info(fp);
    else if (strcmp(cmd, "duty") == 0 && arg + 1 < argc)
        ret = set_duty(fp, (uint8_t)parse_number(argv[arg + 1]));
    else if (strcmp(cmd, "freq") == 0 && arg + 1 < argc)
    {
        frequency = parse_number(argv[arg + 1]);
        ret = Dmod_Ioctl(fp, dmpwm_ioctl_cmd_set_frequency, &frequency);
    }
    else if (strcmp(cmd, "on") == 0 || strcmp(cmd, "off") == 0)
    {
        flag = cmd[1] == 'n';
        ret = Dmod_Ioctl(fp, dmpwm_ioctl_cmd_set_enabled, &flag);
    }
    else if (strcmp(cmd, "sweep") == 0)
        ret = cmd_sweep(fp);
    else
    {
        print_usage(argv[0]);
        ret = 1;
    }
    if (ret != 0)
        Dmod_Printf("pwmtest: %s failed (%d)\n", cmd, ret);
    Dmod_FileClose(fp);
    return ret != 0 ? 1 : 0;
}
