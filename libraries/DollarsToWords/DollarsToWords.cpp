#include "DollarsToWords.h"

const char *ones[] = {
  "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"
};

const char *teens[] = {
  "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
};

const char *tens[] = {
  "ten", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"
};

String DollarsToWords::convertToWords(float amount) {
  // Split the amount into dollars and cents
  int dollars = static_cast<int>(amount);
  int cents = static_cast<int>((amount - dollars) * 100);

  // Convert dollars to words
  String result = "";

  if (dollars > 0) {
    if (dollars >= 1000000) {
      result += String(ones[dollars / 1000000]) + " million ";
      dollars %= 1000000;
    }

    if (dollars >= 1000) {
      result += DollarsToWords::convertToWords(dollars / 1000) + " thousand ";
      dollars %= 1000;
    }

    if (dollars >= 100) {
      result += String(ones[dollars / 100]) + " hundred ";
      dollars %= 100;
    }

    if (dollars >= 20) {
      result += String(tens[dollars / 10 - 1]) + " ";
      dollars %= 10;
    } else if (dollars >= 11) {
      result += String(teens[dollars - 11]) + " ";
      dollars = 0;
    }

    if (dollars > 0) {
      result += String(ones[dollars]) + " ";
    }

    result += (dollars == 1) ? "dollar " : "dollars ";
  } else {
    result += "zero dollars ";
  }

  // Convert cents to words
  if (cents > 0) {
    result += "and ";

    if (cents >= 20) {
      result += String(tens[cents / 10 - 1]) + " ";
      cents %= 10;
    } else if (cents >= 11) {
      result += String(teens[cents - 11]) + " ";
      cents = 0;
    }

    if (cents > 0) {
      result += String(ones[cents]) + "-";
    }

    result += (cents == 1) ? "cent" : "cents";
  } else {
    result += "and zero cents";
  }

  return result;
}
