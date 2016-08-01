char CharConversionTable[256];

void initialize_char_conversion()
{
   for (int i = 0; i < 256; i++)
      CharConversionTable[i] = i;
}

