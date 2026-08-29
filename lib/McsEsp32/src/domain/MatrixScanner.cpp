#include "MatrixScanner.h"

MatrixScanner::MatrixScanner(std::array<DigitalOutput*, kRowCount> rows,
                              std::array<DigitalInput*, kColumnCount> columns)
    : rows_(rows), columns_(columns)
{
}

void MatrixScanner::update()
{
    if (currentRow_ >= 0)
    {
        rows_[currentRow_]->set(false);
    }

    currentRow_ = (currentRow_ + 1) % kRowCount;
    rows_[currentRow_]->set(true);

    for (int col = 0; col < kColumnCount; ++col)
    {
        cache_[currentRow_][col] = columns_[col]->isActive();
    }
}

bool MatrixScanner::isActive(const int row, const int col) const
{
    return cache_[row][col];
}
