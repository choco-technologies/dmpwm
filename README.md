# dmpwm

PWM output as a [dmdrvi](https://github.com/choco-technologies/dmdrvi) device (`/dev/dmpwm<N>`), one per section of the driver ini.
The module is split into the core (`dmpwm`: configuration, the device, the ioctl) and the port (`dmpwm_port`: the peripheral).

| Port | Peripheral |
|------|------------|
| `esp32s3` | LEDC (low speed group: 4 timers, 8 channels, 80 MHz APB clock; outputs on the pin through the GPIO matrix) |

## Configuration

```ini
[backlight_pwm]
driver_name=dmpwm
pin=2             ; GPIO of the output
frequency=5000    ; Hz
resolution=10     ; bits of the duty the port tries to keep (it lowers them when the frequency needs it)
duty=100          ; percent after the start
inverted=off      ; the signal is low for the duty
enabled=on        ; off: the pin stays at its idle level
```

A board's ini ships in the package (`configs/board/<board>/`) and is used with `dmpwm driver=board/<board>/<file>.ini` in a `.dmd`.
`configs/board/elecrow-dis08070h/backlight.ini` is the backlight of the Elecrow DIS08070H (IO2).

## Use

The duty is a 16 bit fraction of the period whatever the hardware resolution (`0` low, `65535` high); `dmpwm_percent_to_duty()` and
`dmpwm_duty_to_percent()` convert from and to percent. The commands of `dmpwm_types.h` go through `Dmod_Ioctl()`:
`dmpwm_ioctl_cmd_set_duty`, `_get_duty`, `_set_frequency`, `_get_frequency`, `_set_enabled`, `_get_enabled`.

`tools/pwmtest` is a manual test tool: `pwmtest info`, `pwmtest duty 30`, `pwmtest freq 1000`, `pwmtest on|off`, `pwmtest sweep`.

## Status

Verified on the Elecrow DIS08070H: the device appears, the ioctl set and read back the duty and the frequency. The ESP32-S3 port is not covered
by CI (no model of the LEDC in the emulator); the host tests cover the conversions and the validation only.
