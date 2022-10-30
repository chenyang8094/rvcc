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

// Read a UTF-8-encoded Unicode code point from a source file.
// We assume that source files are always in UTF-8.
//
// UTF-8 is a variable-width encoding in which one code point is
// encoded in one to four bytes. One byte UTF-8 code points are
// identical to ASCII. Non-ASCII characters are encoded using more
// than one byte.
uint32_t decodeUTF8(char **NewPos, char *P) {
  if ((unsigned char)*P < 128) {
    *NewPos = P + 1;
    return *P;
  }

  char *Start = P;
  int Len;
  uint32_t C;

  if ((unsigned char)*P >= 0b11110000) {
    Len = 4;
    C = *P & 0b111;
  } else if ((unsigned char)*P >= 0b11100000) {
    Len = 3;
    C = *P & 0b1111;
  } else if ((unsigned char)*P >= 0b11000000) {
    Len = 2;
    C = *P & 0b11111;
  } else {
    errorAt(Start, "invalid UTF-8 sequence");
  }

  for (int I = 1; I < Len; I++) {
    if ((unsigned char)P[I] >> 6 != 0b10)
      errorAt(Start, "invalid UTF-8 sequence");
    C = (C << 6) | (P[I] & 0b111111);
  }

  *NewPos = P + Len;
  return C;
}
