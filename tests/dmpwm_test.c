#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmpwm.h"

DMOD_TEST_STEP(percent_to_duty_ends)
{
    DMOD_TEST_EXPECT_EQ(dmpwm_percent_to_duty(0), 0);
    DMOD_TEST_EXPECT_EQ(dmpwm_percent_to_duty(100), DMPWM_DUTY_MAX);
    DMOD_TEST_EXPECT_EQ(dmpwm_percent_to_duty(200), DMPWM_DUTY_MAX);
}

DMOD_TEST_STEP(percent_round_trip)
{
    for (int p = 0; p <= 100; p++)
        DMOD_TEST_EXPECT_EQ(dmpwm_duty_to_percent(dmpwm_percent_to_duty((uint8_t)p)), p);
}

DMOD_TEST_STEP(validate_config)
{
    dmpwm_config_t c = { .pin = 2, .frequency_hz = 5000, .resolution_bits = 10, .duty = 0, .inverted = false, .enabled = true };
    DMOD_TEST_EXPECT_TRUE(dmpwm_validate_config(&c));
    c.frequency_hz = 0;
    DMOD_TEST_EXPECT_FALSE(dmpwm_validate_config(&c));
    c.frequency_hz = 5000;
    c.pin = -1;
    DMOD_TEST_EXPECT_FALSE(dmpwm_validate_config(&c));
    c.pin = 2;
    c.resolution_bits = 15;
    DMOD_TEST_EXPECT_FALSE(dmpwm_validate_config(&c));
    DMOD_TEST_EXPECT_FALSE(dmpwm_validate_config(NULL));
}
