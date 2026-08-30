#pragma once

#include <string>

#include "../application/CommissioningSession.h"
#include "../domain/WebFormSubmission.h"

class WebFormCommissioningAdapter
{
public:
    explicit WebFormCommissioningAdapter(CommissioningSession& session);

    std::string submit(const WebFormSubmission& form);
    [[nodiscard]] bool rebootRequested() const;
    [[nodiscard]] WebFormSubmission currentValues() const;

private:
    CommissioningSession& session_;
};
