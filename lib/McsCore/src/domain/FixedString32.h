#pragma once

class FixedString32
{
public:
    static constexpr int CAPACITY = 32;

    FixedString32() = default;

    explicit FixedString32(const char* value)
    {
        int i = 0;
        for (; i < CAPACITY - 1 && value[i] != '\0'; ++i)
        {
            buffer_[i] = value[i];
        }
        buffer_[i] = '\0';
    }

    bool operator==(const char* other) const
    {
        int i = 0;
        for (; buffer_[i] != '\0' && other[i] != '\0'; ++i)
        {
            if (buffer_[i] != other[i])
            {
                return false;
            }
        }
        return buffer_[i] == '\0' && other[i] == '\0';
    }

private:
    char buffer_[CAPACITY] = {};
};
