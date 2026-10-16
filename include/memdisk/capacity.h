#pragma once
#include "protocol.h"

/* Byte offsets exposed by Windows storage APIs are signed 64-bit values. */
static inline int MdValidCapacity(ULONGLONG bytes)
{
    return bytes != 0 && bytes <= 0x7fffffffffffffffull && bytes % MD_MIB == 0;
}

/* floor(available * 80 / 100), without multiplication overflow. */
static inline ULONGLONG MdCapacityBudget(ULONGLONG available)
{
    return (available / 5) * 4 + (available % 5) * 4 / 5;
}
