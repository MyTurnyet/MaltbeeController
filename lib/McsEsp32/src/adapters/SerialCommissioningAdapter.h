#pragma once

#include <cstddef>
#include <string>

#include "../application/CommissioningSession.h"
#include "../domain/CommandLineParser.h"
#include "../ports/UartPort.h"

template <int N>
class BasicSerialCommissioningAdapter
{
public:
    static constexpr size_t kMaxLineLength = 128;

    BasicSerialCommissioningAdapter(UartPort& uart, BasicCommissioningSession<N>& session)
        : uart_(uart), session_(session)
    {
    }

    void poll()
    {
        while (uart_.available())
        {
            const char c = uart_.read();
            if (c == '\n')
            {
                if (!lineBuffer_.empty() && lineBuffer_.back() == '\r')
                {
                    lineBuffer_.pop_back();
                }
                const ParsedCommand command = CommandLineParser::parse(lineBuffer_);
                uart_.write(session_.apply(command));
                lineBuffer_.clear();
            }
            else if (lineBuffer_.size() >= kMaxLineLength)
            {
                lineBuffer_.clear();
            }
            else
            {
                lineBuffer_ += c;
            }
        }
    }

    [[nodiscard]] bool rebootRequested() const
    {
        return session_.rebootRequested();
    }

private:
    UartPort& uart_;
    BasicCommissioningSession<N>& session_;
    std::string lineBuffer_;
};

using SerialCommissioningAdapter = BasicSerialCommissioningAdapter<12>;
using SerialCommissioningAdapter4 = BasicSerialCommissioningAdapter<4>;
