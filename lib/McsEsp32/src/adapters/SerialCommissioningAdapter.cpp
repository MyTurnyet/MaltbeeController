#include "SerialCommissioningAdapter.h"

#include "../domain/CommandLineParser.h"

SerialCommissioningAdapter::SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session)
    : uart_(uart), session_(session)
{
}

void SerialCommissioningAdapter::poll()
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
        else
        {
            lineBuffer_ += c;
        }
    }
}

bool SerialCommissioningAdapter::rebootRequested() const
{
    return session_.rebootRequested();
}
