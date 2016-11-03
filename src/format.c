#include "global.h"
#include "format.h"

int format(word sink, word fmt, word args)
{
   if (!must_be_atom(fmt))
      return ERROR;
   int a = 0;
   Atom input = getConstant(fmt, NULL).atom_data;
   StringBuilder output = stringBuilder();
   word arg;
   for (int i = 0; i < input->length; i++)
   {
      if (input->data[i] == '~')
      {
         if (i+1 >= input->length)
         {
            freeStringBuilder(output);
            return format_error(MAKE_ATOM("End of string in format specifier"));
         }
         if (input->data[i+1] == '~')
            append_string_no_copy(output, "~", 1);
         else
         {
            i++;
            int radix = -1;
            while (1)
            {
               switch(input->data[i])
               {
                  case 'a': // atom
                  {
                     NEXT_FORMAT_ARG;
                     if (!must_be_atom(arg)) BAD_FORMAT;
                     append_atom(output, getConstant(arg, NULL).atom_data);
                     break;
                  }
                  case 'c': // character code
                  {
                     NEXT_FORMAT_ARG;
                     if (!must_be_character_code(arg)) BAD_FORMAT;
                     char* c = malloc(1);
                     c[0] = (char)getConstant(arg, NULL).integer_data;
                     append_string(output, c, 1);
                     break;
                  }
                  case 'd': // decimal
                  {
                     NEXT_FORMAT_ARG;
                     if (TAGOF(arg) == CONSTANT_TAG)
                     {
                        int type;
                        cdata c = getConstant(arg, &type);
                        if (type == INTEGER_TYPE)
                        {
                           char str[64];
                           sprintf(str, "%ld\n", c.integer_data);
                           append_string(output, strdup(str), strlen(str));
                           break;
                        }
                        else if (type == BIGINTEGER_TYPE)
                        {
                           char* str = mpz_get_str(NULL, 10, c.biginteger_data->data);
                           append_string(output, str, strlen(str));
                        }
                     }
                     type_error(integerAtom, arg);
                     BAD_FORMAT;
                  }
                  case 'D': // decimal with separators
                  {
                     NEXT_FORMAT_ARG;
                     if (TAGOF(arg) == CONSTANT_TAG)
                     {
                        int type;
                        cdata c = getConstant(arg, &type);
                        char* str;
                        if (type == INTEGER_TYPE)
                        {
                           str = malloc(64);
                           sprintf(str, "%ld", c.integer_data);
                        }
                        else if (type == BIGINTEGER_TYPE)
                        {
                           str = mpz_get_str(NULL, 10, c.biginteger_data->data);
                        }
                        else
                        {
                           type_error(integerAtom, arg);
                           BAD_FORMAT;
                        }
                        int k = strlen(str) % 3;
                        for (int j = 0; j < strlen(str);)
                        {
                           append_string(output, strndup(&str[j], k), k);
                           if (k+3 < strlen(str))
                              append_string_no_copy(output, ",", 1);
                           j += k;
                           k = 3;
                        }
                        free(str);
                        break;
                     }
                     type_error(integerAtom, arg);
                     BAD_FORMAT;
                  }
                  case 'e': // floating point as exponential
                  case 'E': // floating point as exponential in upper-case
                  case 'f': // floating point as non-exponential
                  case 'g': // shorter of e or f
                  case 'G': // shorter of E or f
                  {
                     // We can use stdio to do much of this for us, happily.
                     NEXT_FORMAT_ARG;
                     char str[128];
                     number n;
                     if (!evaluate(arg, &n))
                        BAD_FORMAT;
                     // Some special cases for ~f and bignums
                     if (input->data[i] == 'f' && n.type == BigIntegerType)
                     {
                        // ~f must still print .000 depending on the radix
                        // I think it is easiest just to do it ourselves
                        char* str = mpz_get_str(NULL, 10, n.ii);
                        append_string(output, str, strlen(str));
                        toFloatAndFree(n);
                        if (radix == -1)
                           append_string(output, strdup(".000000"), 7);
                        else if (radix > 0)
                        {
                           char str[32];
                           for (int i = 0; i < radix; i++)
                              str[i] = '0';
                           str[i] = 0;
                           append_string(output, strdup(str), strlen(str));
                        }
                     }
                     else if (input->data[i] == 'f' && n.type == RationalType)
                     {
                        char fmt[32]; // This will fit any radix up to LONG_MAX plus an identifier and null terminator (at least!)
                        mpf_t result;
                        // Note that 512 bits of precision here will only get you ~160 significant figures. Hopefully that is sufficient!
                        mpf_init2(result, 512);
                        mpf_set_q(result, n.r);
                        // This is quite tricky.
                        sprintf(fmt, "%%.%dFf", radix == -1?6:radix);
                        int nn = gmp_snprintf(str, 128, fmt, result);
                        if (nn >= 128)
                        {
                           char* big_str = malloc(nn+1);
                           gmp_snprintf(big_str, nn+1, fmt, result);
                           append_string_no_copy(output, big_str, nn);
                        }
                        else
                           append_string(output, strdup(str), strlen(str));
                        mpf_clear(result);
                        toFloatAndFree(n);
                     }
                     else
                     {
                        double d = toFloatAndFree(n);
                        char fmt[32]; // This will fit any radix up to LONG_MAX plus an identifier and null terminator (at least!)
                        sprintf(fmt, "%%.%d%c", radix == -1?6:radix, input->data[i]);
                        sprintf(str, fmt, d);
                        append_string(output, strdup(str), strlen(str));
                     }
                     break;
                  }
                  case 'i': // ignore
                  {
                     NEXT_FORMAT_ARG;
                     break;
                  }
                  case 'I':
                  {
                     NEXT_FORMAT_ARG;
                     if (TAGOF(arg) == CONSTANT_TAG)
                     {
                        int type;
                        cdata c = getConstant(arg, &type);
                        char* str;
                        if (type == INTEGER_TYPE)
                        {
                           str = malloc(64);
                           sprintf(str, "%ld", c.integer_data);
                        }
                        else if (type == BIGINTEGER_TYPE)
                        {
                           str = mpz_get_str(NULL, 10, c.biginteger_data->data);
                        }
                        else
                        {
                           type_error(integerAtom, arg);
                           BAD_FORMAT;
                        }
                        int k = strlen(str) % 3;
                        for (int j = 0; j < strlen(str);)
                        {
                           append_string(output, strndup(&str[j], k), k);
                           if (k+3 < strlen(str))
                              append_string_no_copy(output, "_", 1);
                           j += k;
                           k = 3;
                        }
                        free(str);
                        break;
                     }
                     type_error(integerAtom, arg);
                     BAD_FORMAT;
                  }
                  case 'n': // Newline
                  {
                     append_string_no_copy(output, "\n", 1);
                     break;
                  }
                  case 'N': // soft Newline
                  {
                     if (lastChar(output) != '\n')
                        append_string_no_copy(output, "\n", 1);
                     break;
                  }
                  case 'p': // print
                  {
                     NEXT_FORMAT_ARG;
                     Options options;
                     init_options(&options);
                     set_option(&options, numbervarsAtom, trueAtom);
                     int rc = format_term(output, &options, 1200, arg);
                     free_options(&options);
                     if (!rc) BAD_FORMAT;
                     break;
                  }
                  case 'q': // writeq
                  {
                     NEXT_FORMAT_ARG;
                     Options options;
                     init_options(&options);
                     set_option(&options, numbervarsAtom, trueAtom);
                     set_option(&options, quotedAtom, trueAtom);
                     int rc = format_term(output, &options, 1200, arg);
                     free_options(&options);
                     if (!rc) BAD_FORMAT;
                     break;
                  }
                  case 'r': // radix
                  case 'R': // radix upper case
                  {
                     NEXT_FORMAT_ARG;
                     if (TAGOF(arg) == CONSTANT_TAG)
                     {
                        int type;
                        cdata c = getConstant(arg, &type);
                        if (type == INTEGER_TYPE)
                        {
                           char str[128];
                           str[127] = 0;
                           int k = 126;
                           long source = c.integer_data;
                           while (k > 0 && source != 0)
                           {
                              int j = source % radix;
                              if (j < 10)
                                 str[k] = j + '0';
                              else if (input->data[i] == 'R')
                                 str[k] = j - 10 + 'A';
                              else if (input->data[i] == 'r')
                                 str[k] = j - 10 + 'a';
                              k--;
                              source = source / radix;
                           }
                           append_string(output, strdup(&str[k]), strlen(&str[k]));
                           break;
                        }
                        else if (type == BIGINTEGER_TYPE)
                        {
                           char* str = mpz_get_str(NULL, radix, c.biginteger_data->data);
                           append_string(output, str, strlen(str));
                           break;
                        }
                     }
                     type_error(integerAtom, arg);
                     BAD_FORMAT;
                  }
                  case 's': // string
                  case '@': // execute
                  case 't': // tab
                  case '|': // tab-stop
                  case '+': // tab-stop
                     assert(0 && "Not implemented");
                  case 'w': // write
                  {
                     NEXT_FORMAT_ARG;
                     Options options;
                     init_options(&options);
                     set_option(&options, numbervarsAtom, trueAtom);
		     int rc = format_term(output, &options, 1200, arg);
                     free_options(&options);
                     if (!rc) BAD_FORMAT;
                     break;
                  }
                  case '*':
                  {
                     NEXT_FORMAT_ARG;
                     if (!must_be_integer(arg)) BAD_FORMAT;
                     radix = getConstant(arg, NULL).integer_data;
                     i++;
                     continue;
                  }
                  case '0':
                  case '1':
                  case '2':
                  case '3':
                  case '4':
                  case '5':
                  case '6':
                  case '7':
                  case '8':
                  case '9':
                  {
                     char* end;
                     radix = strtol(&input->data[i], &end, 10);
                     i = (end - input->data);
                     continue;
                  }
                  default:
                  {
                     format_error(MAKE_ATOM("No such format character"));
                     BAD_FORMAT;
                  }
               }
               break;
            }
         }
      }
      else
         append_string_no_copy(output, &input->data[i], 1);
   }
   char* result;
   int len;
   int rc;
   finalize_buffer(output, &result, &len);
   if (TAGOF(sink) == COMPOUND_TAG && FUNCTOROF(sink) == atomFunctor)
   {
      rc = unify(ARGOF(sink, 0), MAKE_NATOM(result, len));
      free(result);
      return rc;
   }
   Stream s = get_stream(sink);
   if (s == NULL)
   {
      free(result);
      return ERROR;
   }
   rc = s->write(s, len, (unsigned char*)result) == len;
   free(result);
   return rc;
}
