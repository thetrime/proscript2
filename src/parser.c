#include <errno.h>
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stream.h"
#include "constants.h"
#include "kernel.h"
#include "hashmap.h"
#include <assert.h>

token ListOpenToken_ = {ConstantTokenType}; Token ListOpenToken = &ListOpenToken_;
token ListCloseToken_ = {ConstantTokenType}; Token ListCloseToken = &ListCloseToken_;
token ParenOpenToken_ = {ConstantTokenType}; Token ParenOpenToken = &ParenOpenToken_;
token ParenCloseToken_ = {ConstantTokenType}; Token ParenCloseToken = &ParenCloseToken_;
token CurlyOpenToken_ = {ConstantTokenType}; Token CurlyOpenToken = &CurlyOpenToken_;
token CurlyCloseToken_ = {ConstantTokenType}; Token CurlyCloseToken = &CurlyCloseToken_;
token BarToken_ = {ConstantTokenType}; Token BarToken = &BarToken_;
token CommaToken_ = {ConstantTokenType}; Token CommaToken = &CommaToken_;
token PeriodToken_ = {ConstantTokenType}; Token PeriodToken = &PeriodToken_;
token DoubleQuoteToken_ = {ConstantTokenType}; Token DoubleQuoteToken = &DoubleQuoteToken_;

word parse_infix(Stream s, word lhs, int precedence, map_t vars);
word parse_postfix(Stream s, word lhs, int precedence, map_t vars);


void freeToken(Token t)
{
   switch(t->type)
   {
      case ConstantTokenType:
         return;
      case AtomTokenType:
         free(t->data.atom_data);
         free(t);
         break;
      case SyntaxErrorTokenType:
         free(t->data.syntax_error_data);
         free(t);
         break;
      case StringTokenType:
         free(t->data.string_data);
         free(t);
         break;
      case IntegerTokenType:
         free(t);
         break;
      case FloatTokenType:
         free(t);
         break;
      case VariableTokenType:
         free(t->data.variable_data);
         free(t);
         break;
      case BigIntegerTokenType:
         free(t->data.biginteger_data);
         free(t);
         break;

   }
}

int is_char(c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_');
}

int is_graphic_char(c)
{
   return c == '#' ||
      c == '$' ||
      c == '&' ||
      c == '*' ||
      c == '+' ||
      c == '-' ||
      c == '.' ||
      c == '/' ||
      c == ':' ||
      c == '<' ||
      c == '=' ||
      c == '>' ||
      c == '?' ||
      c == '@' ||
      c == '^' ||
      c == '~' ||
      c == '\\'; // graphic-token-char is either graphic-char or backslash-char
}

char* CharConversionTable = NULL;

char lookahead = -1;
void unget_raw_char(int c)
{
    lookahead = c;
}

char get_raw_char_with_conversion(Stream s)
{
    if (lookahead != -1)
    {
        int c = lookahead;
        lookahead = -1;
        return c;
    }
//    if (!PrologFlag.values['char_conversion'])
        return get_raw_char(s);
    int t = get_raw_char(s);
    int tt = CharConversionTable[t];
    if (tt == -1)
        return t;
    else
        return tt;
}

char peek_raw_char_with_conversion(Stream s)
{
    if (lookahead != -1)
    {
        return lookahead;
    }
    //if (!PrologFlag.values['char_conversion'])
        return peek_raw_char(s);
    int t = peek_raw_char(s);
    int tt = CharConversionTable[t];
    if (tt == -1)
        return t;
    else
        return tt;
}

CharBuffer charBuffer()
{
   CharBuffer cb = malloc(sizeof(CharBuffer));
   cb->head = NULL;
   cb->tail = NULL;
   return cb;
}

void pushChar(CharBuffer cb, int c)
{
   if (cb->tail == NULL)
   {
      cb->head = malloc(sizeof(CharCell));
      cb->tail = cb->head;
      cb->head->next = NULL;
      cb->head->data = c;
   }
   else
   {
      cb->tail->next = malloc(sizeof(CharCell));
      cb->tail->next->next = NULL;
      cb->tail->next->data = c;
   }
}

char* finalize(CharBuffer buffer)
{
   int length = 1; // null-terminator
   CharCell* c = buffer->head;
   while (c != NULL)
   {
      length++;
      c = c->next;
   }
   char* data = malloc(length);
   c = buffer->head;
   int i = 0;
   CharCell* d;
   while (c != NULL)
   {
      data[i++] = c->data;
      d = c->next;
      free(c);
      c = d;
   }
   free(buffer);
   return data;
}

Token SyntaxErrorToken(char* message)
{
   Token t = malloc(sizeof(Token));
   t->type = SyntaxErrorTokenType;
   t->data.syntax_error_data = message;
   return t;

}

Token AtomToken(char* data)
{
   Token t = malloc(sizeof(Token));
   t->type = AtomTokenType;
   t->data.atom_data = data;
   return t;
}

Token VariableToken(char* name)
{
   Token t = malloc(sizeof(Token));
   t->type = VariableTokenType;
   t->data.variable_data = name;
   return t;
}

Token BigIntegerToken(char* data)
{
   Token t = malloc(sizeof(Token));
   t->type = BigIntegerTokenType;
   t->data.biginteger_data = data;
   return t;
}


Token StringToken(char* data)
{
   Token t = malloc(sizeof(Token));
   t->type = StringTokenType;
   t->data.atom_data = data;
   return t;
}

Token IntegerToken(long data)
{
   Token t = malloc(sizeof(Token));
   t->type = IntegerTokenType;
   t->data.integer_data = data;
   return t;
}

Token FloatToken(double data)
{
   Token t = malloc(sizeof(Token));
   t->type = FloatTokenType;
   t->data.float_data = data;
   return t;
}

Token lex(Stream s)
{
   Token t;
   int c, d;
   while (1)
   {
      c = get_raw_char_with_conversion(s);
      if (c == -1)
         return NULL;
      if (c == ' ' || c == '\n' || c == '\t')
         continue;
      else if (c == '%') // Discard single-line comment
      {
         do
         {
            d = get_raw_char_with_conversion(s);
            if (d == -1)
               return NULL;
         } while (d != '\n');
         continue;
      }
      else if (c == '/')
      {
         d = peek_raw_char_with_conversion(s);
         if (d == '*') // Discard multi-line comment
         {
            get_raw_char_with_conversion(s);
            d = get_raw_char_with_conversion(s);
            while (1)
            {
               if (d == -1)
                  return SyntaxErrorToken("end of file in comment");
               if (d == '*' && ((d = get_raw_char_with_conversion(s)) == '/'))
                  break;
               else
                  d = get_raw_char_with_conversion(s);
            }
            continue;
         }
         else
         {
            // My mistake, the term actually begins with / character. c is still set to the right thing
            break;
         }
      }
      break;
   }
   if (c == '[')
   {
      if (peek_raw_char_with_conversion(s) == ']')
      {
         get_raw_char_with_conversion(s);
         return AtomToken("[]");
      }
      return ListOpenToken;
   }
   else if (c == '(')
      return ParenOpenToken;
   else if (c == ')')
      return ParenCloseToken;
   else if (c == '{')
      return CurlyOpenToken;
   else if (c == '}')
      return CurlyCloseToken;
   else if (c == ']')
      return ListCloseToken;
   else if (c == '|')
      return BarToken;
   if ((c >= 'A' && c < 'Z') || c == '_')
   {
      CharBuffer sb = charBuffer();
      while (1)
      {
         c = peek_raw_char_with_conversion(s);
         if (is_char(c))
            pushChar(sb, c);
         else
            return VariableToken(finalize(sb));
      }
   }
   else if ((c >= '0' && c <= '9') || (c == '-' && peek_raw_char_with_conversion(s) >= '0' && peek_raw_char_with_conversion(s) <= '9'))
   {
      if (c == '0' && peek_raw_char_with_conversion(s) == '\'')
      {
         // Char code 0'x
         get_raw_char_with_conversion(s);
         return IntegerToken(get_raw_char_with_conversion(s));
      }
      // FIXME: Should also hadle 0x and 0b here
      // Parse a number
      CharBuffer sb = charBuffer();
      pushChar(sb, c);
      int seen_decimal = 0;
      while(1)
      {
         c = peek_raw_char_with_conversion(s);
         if (c >= '0' && c <= '9')
         {
            pushChar(sb, c);
            get_raw_char_with_conversion(s);
         }
         else if (c == '.' && !seen_decimal)
         {
            pushChar(sb, '.');
            get_raw_char_with_conversion(s);
            int p = peek_raw_char_with_conversion(s);
            if (p >= '0' && p <= '9')
               seen_decimal = 1;
            else
            {
               // For example, "X = 3."
               unget_raw_char('.');
               break;
            }
         }
         else if (is_char(c))
            return SyntaxErrorToken("illegal number");
         else
            break;
      }
      if (!seen_decimal)
      {
         char* s = finalize(sb);
         char* sp;
         long l = strtol(s, &sp, 10);
         if (errno == ERANGE)
         {
            return BigIntegerToken(s);
         }
         else
         {
            free(s);
            return IntegerToken(l);
         }
      }
      else if (seen_decimal)
      {
         // Must be a float
         char* s = finalize(sb);
         char* sp;
         double d = strtod(s, &sp);
         free(s);
         return FloatToken(d);
      }
   }
   else
   {
      CharBuffer sb = charBuffer();
      int is_escape = 0;
      if (c == '\'' || c == '"')
      {
         int matcher = c;
         while (1)
         {
            c = get_raw_char_with_conversion(s);
            if (c == -1)
               return SyntaxErrorToken("end of file in atom");
            if (c == '\\')
            {
               if (is_escape)
                  pushChar(sb, '\\');
               is_escape = ~is_escape;
               continue;
            }
            else if (is_escape)
            {
               // control char like \n
               if (c == 'x')
               {
                  char hex[3];
                  hex[0] = get_raw_char_with_conversion(s);
                  hex[1] = get_raw_char_with_conversion(s);
                  hex[2] = '\0';
                  pushChar(sb, strtol(hex, NULL, 16));
                  if (peek_raw_char_with_conversion(s) == '\\')
                     get_raw_char_with_conversion(s);
               }
               else if (c == 'n')
                  pushChar(sb, '\n');
               else if (c == 't')
                  pushChar(sb, '\t');
               else if (c == '\'')
                  pushChar(sb, '\'');
               else if (c == 'u')
               {
                  char hex[5];
                  hex[0] = get_raw_char_with_conversion(s);
                  hex[1] = get_raw_char_with_conversion(s);
                  hex[2] = get_raw_char_with_conversion(s);
                  hex[3] = get_raw_char_with_conversion(s);
                  hex[4] = '\0';
                  pushChar(sb, strtol(hex, NULL, 16));
                  if (peek_raw_char_with_conversion(s) == '\\')
                     get_raw_char_with_conversion(s);
               }
               else
                  printf("Unexpected escape code %d\n", c);
               is_escape = 0;
               continue;
            }
            if (c == matcher && !is_escape)
            {
               // For example ''''
               if (peek_raw_char_with_conversion(s) == matcher)
                  get_raw_char_with_conversion(s);
               else
                  break;
            }
            pushChar(sb, c);
         }
         if (matcher == '"')
            return StringToken(finalize(sb));
         else
            return AtomToken(finalize(sb));
      }
      else
      {
         CharBuffer sb = charBuffer();
         pushChar(sb, c);
         int char_atom = is_char(c);
         int punctuation_atom = is_graphic_char(c);
         while (1)
         {
            c = peek_raw_char_with_conversion(s);
            if (c == -1)
               break;
            if (char_atom && is_char(c))
               pushChar(sb, get_raw_char_with_conversion(s));
            else if (punctuation_atom && is_graphic_char(c))
               pushChar(sb, get_raw_char_with_conversion(s));
            else
               break;
         }
         return AtomToken(finalize(sb));
      }
   }
   // This should be unreachable
   return NULL;
}

Token peeked_token = NULL;

Token read_token(Stream s)
{
   if (peeked_token != NULL)
   {
      Token t = peeked_token;
      peeked_token = NULL;
      return t;
   }
   return lex(s);
}

Token peek_token(Stream s)
{
   if (peeked_token != NULL)
      return peeked_token;
   peeked_token = lex(s);
   return peeked_token;
}

void unread_token(Stream s, Token t)
{
   if (peeked_token == NULL)
      peeked_token = t;
   printf("Horrible failure: Attempt to unread a token when we already have one unread\n");
}

int find_operator(Token t, Operator* op, OperatorPosition position)
{
   return 0;
}

word syntax_error = 0; // FIXME: Not sure about what to do here




word read_expression(Stream s, int precedence, int isArg, int isList, map_t vars)
{
   Token token = read_token(s);
   if (token == NULL)
      return endOfFileAtom;
//   if (token == ListCloseToken)
//      return token;
   word lhs;
   Operator prefixOperator;
   Operator infixOperator;

   int hasOp = find_operator(token, &prefixOperator, Prefix);
   Token peeked_token = peek_token(s);
   if (peeked_token == ListCloseToken || peeked_token == ParenCloseToken || peeked_token == CommaToken)
      hasOp = 0;
   if (peeked_token == ParenOpenToken)
   {
      read_token(s);
      Token pp = peek_token(s);
      unread_token(s, ParenOpenToken);
      if (pp != ParenOpenToken)
         hasOp = 0;
   }
   if (!hasOp)
   {
      if (token == DoubleQuoteToken)
      {
         // FIXME: implement...?
      }
      else if (token == ListOpenToken || token == CurlyOpenToken)
      {
         // FIXME: implement
      }
      else if (token == ParenOpenToken)
      {
         lhs = read_expression(s, 12001, 0, 0, vars);
         Token next = read_token(s);
         if (next != ParenCloseToken)
            return syntax_error; // Mismatched ( at <next>
         else if (token->type == VariableTokenType)
         {
            if (token->data.variable_data[0] == '_')
               lhs = MAKE_VAR();
            else
            {
               if (hashmap_get(vars, token->data.variable_data, strlen(token->data.variable_data), (any_t*)&lhs) == MAP_MISSING)
               {
                  lhs = MAKE_VAR();
                  hashmap_put(vars, strdup(token->data.variable_data), strlen(token->data.variable_data), (any_t)lhs);
               }
            }
         }
      }
      else if (token == CurlyCloseToken)
         lhs = MAKE_ATOM("}");
      else
      {
         switch(token->type)
         {
            case AtomTokenType:
               lhs = MAKE_ATOM(token->data.atom_data);
               break;
            case IntegerTokenType:
               lhs = MAKE_INTEGER(token->data.integer_data);
               break;
            default:
               printf("Not implemented yet\n");
               lhs = 0;
         }
      }
   }
   else if (prefixOperator.fixity == FX)
   {
      lhs = MAKE_VCOMPOUND(prefixOperator.functor, read_expression(s, prefixOperator.precedence, isArg, isList, vars));
   }
   else if (prefixOperator.fixity == FY)
   {
      lhs = MAKE_VCOMPOUND(prefixOperator.functor, read_expression(s, prefixOperator.precedence+1, isArg, isList, vars));
   }
   else
      return syntax_error; // parse error
   while (1)
   {
      token = peek_token(s);
      if (token->type == IntegerTokenType && token->data.integer_data <= 0)
      {
         read_token(s);
         token->data.integer_data = -token->data.integer_data;
         unread_token(s, token);
         unread_token(s, AtomToken("-"));
      }
      if (token == ParenOpenToken)
      {
         // Reading a term
      }
      if (token == PeriodToken)
         return lhs;
      if (token == CommaToken && isArg)
         return lhs;
      if (token == BarToken && isList)
         return lhs;
      if (token == NULL)
         return lhs;
      if (find_operator(token, &infixOperator, Infix))
      {
         if (infixOperator.fixity == XFX && precedence > infixOperator.precedence)
            lhs = parse_infix(s, lhs, infixOperator.precedence, vars);
         else if (infixOperator.fixity == XFY && precedence > infixOperator.precedence)
            lhs = parse_infix(s, lhs, infixOperator.precedence+1, vars);
         else if (infixOperator.fixity == YFX && precedence >= infixOperator.precedence)
            lhs = parse_infix(s, lhs, infixOperator.precedence, vars);
         else if (infixOperator.fixity == XF && precedence > infixOperator.precedence)
            lhs = parse_postfix(s, lhs, infixOperator.precedence, vars);
         else if (infixOperator.fixity == YF && precedence >= infixOperator.precedence)
            lhs = parse_postfix(s, lhs, infixOperator.precedence, vars);
         else
            return lhs;
      }
      else
         return lhs;
   }
}

word parse_infix(Stream s, word lhs, int precedence, map_t vars)
{
   Token t = read_token(s);
   word rhs = read_expression(s, precedence, 0, 0, vars);
   assert(t->type == AtomTokenType);
   return MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_ATOM(t->data.atom_data), 2), lhs, rhs);
}

word parse_postfix(Stream s, word lhs, int precedence, map_t vars)
{
   Token t = read_token(s);
   assert(t->type == AtomTokenType);
   return MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_ATOM(t->data.atom_data), 1), lhs);
}


int free_key(any_t key, any_t value)
{
   free(key);
   return MAP_OK;
}

word readTerm(Stream stream, void* options)
{
   peeked_token = NULL;
   map_t vars = hashmap_new();
   word t = read_expression(stream, 12001, 0, 0, vars);
   any_t tmp;
   hashmap_iterate(vars, free_key, &tmp);
   hashmap_free(vars);
   return t;
}
