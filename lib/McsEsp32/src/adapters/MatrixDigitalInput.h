#pragma once

#include "../domain/MatrixScanner.h"
#include "ports/DigitalInput.h"

class MatrixDigitalInput final : public DigitalInput
{
public:
    MatrixDigitalInput(MatrixScanner& scanner, int row, int col);

    [[nodiscard]] bool isActive() const override;

private:
    MatrixScanner& scanner_;
    int row_;
    int col_;
};
