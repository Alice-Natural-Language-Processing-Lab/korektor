// This file is part of UniLib <http://github.com/ufal/unilib/>.
//
// Copyright 2014 by Institute of Formal and Applied Linguistics, Faculty
// of Mathematics and Physics, Charles University in Prague, Czech Republic.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under 3-clause BSD licence.
//
// UniLib version: 2.0
// Unicode version: 7.0.0

#include "utf16.h"

namespace ngramchecker {

bool utf16::valid(const char16_t* str) {
  for (; *str; str++)
    if (*str >= 0xD800 && *str < 0xDC00) {
      str++; if (*str < 0xDC00 || *str >= 0xE000) return false;
    } else if (*str >= 0xDC00 && *str < 0xE000) return false;

  return true;
}

bool utf16::valid(const char16_t* str, size_t len) {
  for (; len; str++, len--)
    if (*str >= 0xD800 && *str < 0xDC00) {
      str++; if (!--len || *str < 0xDC00 || *str >= 0xE000) return false;
    } else if (*str >= 0xDC00 && *str < 0xE000) return false;

  return true;
}

void utf16::decode(const char16_t* str, std::u32string& decoded) {
  decoded.clear();

  for (char32_t chr; (chr = decode(str)); )
    decoded.push_back(chr);
}

void utf16::decode(const char16_t* str, size_t len, std::u32string& decoded) {
  decoded.clear();

  while (len)
    decoded.push_back(decode(str, len));
}

void utf16::encode(const std::u32string& str, std::u16string& encoded) {
  encoded.clear();

  for (auto&& chr : str)
    append(encoded, chr);
}

const char16_t utf16::REPLACEMENT_CHAR;

} // namespace ngramchecker