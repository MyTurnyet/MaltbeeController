#pragma once

#include <string>

#include "ports/UartPort.h"

class FakeUartPort final : public UartPort
{
public:
    std::string written;

    void queueInput(const std::string& text)
    {
        pending_ += text;
    }

    [[nodiscard]] bool available() const override
    {
        return position_ < pending_.size();
    }

    char read() override
    {
        return pending_[position_++];
    }

    void write(const std::string& text) override
    {
        written += text;
    }

private:
    std::string pending_;
    size_t position_ = 0;
};
