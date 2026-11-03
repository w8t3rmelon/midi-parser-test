#ifndef _MF_UTIL_H
#define _MF_UTIL_H

#include <stdint.h>

uint16_t swap_16(uint16_t i) {
    uint16_t val = i;
    val = ((val & 0xff00) >> 8) | ((val & 0x00ff) << 8);
    return val;
}

uint32_t swap_32(uint32_t i) {
    uint32_t val = i;

    val = ((val & 0xff) << 24) | ((val & 0xff00) << 8) |
          ((val & 0xff0000) >> 8) | ((val & 0xff000000) >> 24);

    return val;
}

uint64_t swap_64(uint64_t i) {
    uint64_t val = i;

    val = ((val & 0x00000000000000ffULL) << 56) |
          ((val & 0xff00000000000000ULL) >> 56) |

          ((val & 0x000000000000ff00ULL) << 40) |
          ((val & 0x00ff000000000000ULL) >> 40) |

          ((val & 0x0000000000ff0000ULL) << 24) |
          ((val & 0x0000ff0000000000ULL) >> 24) |

          ((val & 0x00000000ff000000ULL) << 8) |
          ((val & 0x000000ff00000000ULL) >> 8);

    return val;
}

#endif /* _MF_UTIL_H */
