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

static bool inRange(uint32_t *Range, uint32_t C) {
  for (int I = 0; Range[I] != -1; I += 2)
    if (Range[I] <= C && C <= Range[I + 1])
      return true;
  return false;
}

// [https://www.sigbus.info/n1570#D] C11 allows not only ASCII but
// some multibyte characters in certan Unicode ranges to be used in an
// identifier.
//
// This function returns true if a given character is acceptable as
// the first character of an identifier.
//
// For example, ¾ (U+00BE) is a valid identifier because characters in
// 0x00BE-0x00C0 are allowed, while neither ⟘ (U+27D8) nor '　'
// (U+3000, full-width space) are allowed because they are out of range.
bool isIdent1_1(uint32_t C) {
  static uint32_t Range[] = {
      '_',     '_',     'a',     'z',     'A',     'Z',     0x00A8,  0x00A8,
      0x00AA,  0x00AA,  0x00AD,  0x00AD,  0x00AF,  0x00AF,  0x00B2,  0x00B5,
      0x00B7,  0x00BA,  0x00BC,  0x00BE,  0x00C0,  0x00D6,  0x00D8,  0x00F6,
      0x00F8,  0x00FF,  0x0100,  0x02FF,  0x0370,  0x167F,  0x1681,  0x180D,
      0x180F,  0x1DBF,  0x1E00,  0x1FFF,  0x200B,  0x200D,  0x202A,  0x202E,
      0x203F,  0x2040,  0x2054,  0x2054,  0x2060,  0x206F,  0x2070,  0x20CF,
      0x2100,  0x218F,  0x2460,  0x24FF,  0x2776,  0x2793,  0x2C00,  0x2DFF,
      0x2E80,  0x2FFF,  0x3004,  0x3007,  0x3021,  0x302F,  0x3031,  0x303F,
      0x3040,  0xD7FF,  0xF900,  0xFD3D,  0xFD40,  0xFDCF,  0xFDF0,  0xFE1F,
      0xFE30,  0xFE44,  0xFE47,  0xFFFD,  0x10000, 0x1FFFD, 0x20000, 0x2FFFD,
      0x30000, 0x3FFFD, 0x40000, 0x4FFFD, 0x50000, 0x5FFFD, 0x60000, 0x6FFFD,
      0x70000, 0x7FFFD, 0x80000, 0x8FFFD, 0x90000, 0x9FFFD, 0xA0000, 0xAFFFD,
      0xB0000, 0xBFFFD, 0xC0000, 0xCFFFD, 0xD0000, 0xDFFFD, 0xE0000, 0xEFFFD,
      -1,
  };

  return inRange(Range, C);
}

// Returns true if a given character is acceptable as a non-first
// character of an identifier.
bool isIdent2_1(uint32_t C) {
  static uint32_t Range[] = {
      '0',    '9',    0x0300, 0x036F, 0x1DC0, 0x1DFF,
      0x20D0, 0x20FF, 0xFE20, 0xFE2F, -1,
  };

  return isIdent1_1(C) || inRange(Range, C);
}
