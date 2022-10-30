#include "rvcc.h"

// Encode a given character in UTF-8.
int encodeUTF8(char *Buf, uint32_t C) {
  if (C <= 0x7F) {
    Buf[0] = C;
    return 1;
  }

  if (C <= 0x7FF) {
    Buf[0] = 0b11000000 | (C >> 6);
    Buf[1] = 0b10000000 | (C & 0b00111111);
    return 2;
  }

  if (C <= 0xFFFF) {
    Buf[0] = 0b11100000 | (C >> 12);
    Buf[1] = 0b10000000 | ((C >> 6) & 0b00111111);
    Buf[2] = 0b10000000 | (C & 0b00111111);
    return 3;
  }

  Buf[0] = 0b11110000 | (C >> 18);
  Buf[1] = 0b10000000 | ((C >> 12) & 0b00111111);
  Buf[2] = 0b10000000 | ((C >> 6) & 0b00111111);
  Buf[3] = 0b10000000 | (C & 0b00111111);
  return 4;
}
