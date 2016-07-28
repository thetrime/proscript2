#include "types.h"
#include "stream.h"

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
   TokenType type;
   union
   {
      char* atom_data;
      char* string_data;
      long integer_data;
      double float_data;
      char* syntax_error_data;
      char* variable_data;
      char* biginteger_data;
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
   CharCell* head;
   CharCell* tail;
} charbuffer;

typedef charbuffer* CharBuffer;

typedef enum
{
   FX, FY, XFX, XFY, YFX, XF, YF
} Fixity;

typedef struct
{
   int precedence;
   Fixity fixity;
   word functor;
} Operator;

 
typedef struct
{
} HashMap;

typedef enum
{
   Prefix, Infix, Postfix
} OperatorPosition;

word readTerm(Stream s, void* options);
