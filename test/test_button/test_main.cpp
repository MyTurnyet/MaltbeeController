#include <unity.h>

#include "domain/Button.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;
}

void setUp()
{
    // Called before each test.
}

void tearDown()
{
    // Called after each test.
}

void button_begins_released()
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    TEST_ASSERT_FALSE(button.isPressed());
}

void button_ignores_raw_input_shorter_than_debounce_period()
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();

    clock.advanceBy(DEBOUNCE_MS - 1);
    button.update();

    TEST_ASSERT_FALSE(button.isPressed());
}

void button_reports_pressed_after_stable_active_input()
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();

    clock.advanceBy(DEBOUNCE_MS);
    button.update();

    TEST_ASSERT_TRUE(button.isPressed());
}

void button_was_pressed_is_true_once()
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();

    clock.advanceBy(DEBOUNCE_MS);
    button.update();
    TEST_ASSERT_TRUE(button.wasPressed());

    button.update();
    TEST_ASSERT_FALSE(button.wasPressed());
}

void button_holding_does_not_repeat_pressed_event()
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();

    for (int i = 0; i < 5; i++)
    {
        clock.advanceBy(DEBOUNCE_MS);
        button.update();
        TEST_ASSERT_FALSE(button.wasPressed());
    }
}

void button_release_follows_same_debounce_rules()
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();
    TEST_ASSERT_TRUE(button.isPressed());

    input.active = false;
    button.update();
    TEST_ASSERT_TRUE(button.isPressed());

    clock.advanceBy(DEBOUNCE_MS - 1);
    button.update();
    TEST_ASSERT_TRUE(button.isPressed());

    clock.advanceBy(1);
    button.update();
    TEST_ASSERT_FALSE(button.isPressed());
}

void button_was_released_is_true_once()
{
    FakeDigitalInput input;
    FakeClock clock;
    Button button(input, clock, DEBOUNCE_MS);

    input.active = true;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();

    input.active = false;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();
    TEST_ASSERT_TRUE(button.wasReleased());

    button.update();
    TEST_ASSERT_FALSE(button.wasReleased());
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(button_begins_released);
    RUN_TEST(button_ignores_raw_input_shorter_than_debounce_period);
    RUN_TEST(button_reports_pressed_after_stable_active_input);
    RUN_TEST(button_was_pressed_is_true_once);
    RUN_TEST(button_holding_does_not_repeat_pressed_event);
    RUN_TEST(button_release_follows_same_debounce_rules);
    RUN_TEST(button_was_released_is_true_once);

    return UNITY_END();
}
