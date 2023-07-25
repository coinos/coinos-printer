#ifndef DOLLARS_TO_WORDS_H
#define DOLLARS_TO_WORDS_H

#include <Arduino.h>

class DollarsToWords {
public:
  static String convertToWords(float amount);
};

#endif
