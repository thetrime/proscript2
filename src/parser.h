#ifndef _PARSER_H
#define _PARSER_H
#include "types.h"
#include "stream.h"
#include "options.h"
#include "prolog_flag.h"
#include "char_conversion.h"

typedef enum
{
   AtomTokenType,
   StringTokenType,
   IntegerTokenType,
   FloatTokenType,
   ConstantTokenType,
   VariableTokenType,
   SyntaxErrorTokenType,
   BigIntegerTokenType
} TokenType;


typedef struct
{
   char* data;
   size_t length;
} atom_data_t;

typedef struct
{
   TokenType type;
   union
   {
      atom_data_t* atom_data;
      long integer_data;
      double float_data;
      char* syntax_error_data;
      char* variable_data;
      char* biginteger_data;
      char* constant_data;
// ...
   } data;
} token;

typedef token* Token;

struct CharCell
{
   struct CharCell* next;
   char data;
};

typedef struct CharCell CharCell;

typedef struct
{
   int length;
   CharCell* head;
   CharCell* tail;
} charbuffer;

typedef charbuffer* CharBuffer;


int is_graphic_char(char);
int read_term(Stream s, Options* options, word* w);
Token lex(Stream s);
void freeToken(Token t);
#endif
