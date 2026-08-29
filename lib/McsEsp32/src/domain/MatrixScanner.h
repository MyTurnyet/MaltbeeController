#pragma once

#include <array>

#include "ports/DigitalInput.h"
#include "ports/DigitalOutput.h"

class MatrixScanner
{
public:
    static constexpr int kRowCount = 3;
    static constexpr int kColumnCount = 4;

    MatrixScanner(std::array<DigitalOutput*, kRowCount> rows,
                  std::array<DigitalInput*, kColumnCount> columns);

    void update();

    [[nodiscard]] bool isActive(int row, int col) const;

private:
    std::array<DigitalOutput*, kRowCount> rows_;
    std::array<DigitalInput*, kColumnCount> columns_;
    std::array<std::array<bool, kColumnCount>, kRowCount> cache_{};
    int currentRow_ = -1;
};
