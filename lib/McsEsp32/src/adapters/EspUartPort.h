#pragma once

#include "../ports/UartPort.h"

class EspUartPort final : public UartPort
{
public:
    explicit EspUartPort(unsigned long baudRate);

    void begin();

    [[nodiscard]] bool available() const override;
    char read() override;
    void write(const std::string& text) override;

private:
    unsigned long baudRate_;
};
