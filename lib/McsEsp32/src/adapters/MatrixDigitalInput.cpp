#include "MatrixDigitalInput.h"

MatrixDigitalInput::MatrixDigitalInput(MatrixScanner& scanner, const int row, const int col)
    : scanner_(scanner), row_(row), col_(col)
{
}

bool MatrixDigitalInput::isActive() const
{
    return scanner_.isActive(row_, col_);
}
