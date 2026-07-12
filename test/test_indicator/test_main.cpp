#include <unity.h>

#include "domain/Indicator.h"
#include "ports/DigitalOutput.h"

class FakeDigitalOutput final : public DigitalOutput
{
public:
    void set(const bool active) override
    {
        active_ = active;
        setCallCount_++;
    }

    [[nodiscard]] bool isSet() const override
    {
        return active_;
    }

    [[nodiscard]] int setCallCount() const
    {
        return setCallCount_;
    }

private:
    bool active_ = false;
    int setCallCount_ = 0;
};

void setUp()
{
    // Called before each test.
}

void tearDown()
{
    // Called after each test.
}

void indicator_turns_output_on()
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.on();

    TEST_ASSERT_TRUE(output.isSet());
    TEST_ASSERT_TRUE(indicator.isOn());
}

void indicator_turns_output_off()
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.on();
    indicator.off();

    TEST_ASSERT_FALSE(output.isSet());
    TEST_ASSERT_FALSE(indicator.isOn());
}

void indicator_sets_requested_state()
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.set(true);
    TEST_ASSERT_TRUE(indicator.isOn());

    indicator.set(false);
    TEST_ASSERT_FALSE(indicator.isOn());
}

void indicator_delegates_each_request_to_output()
{
    FakeDigitalOutput output;
    Indicator indicator(output);

    indicator.on();
    indicator.off();

    TEST_ASSERT_EQUAL_INT(2, output.setCallCount());
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(indicator_turns_output_on);
    RUN_TEST(indicator_turns_output_off);
    RUN_TEST(indicator_sets_requested_state);
    RUN_TEST(indicator_delegates_each_request_to_output);

    return UNITY_END();
}