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
#include "list.h"
#include "operators.h"

token ListOpenToken_ = {ConstantTokenType}; Token ListOpenToken = &ListOpenToken_;
token ListCloseToken_ = {ConstantTokenType}; Token ListCloseToken = &ListCloseToken_;
token ParenOpenToken_ = {ConstantTokenType}; Token ParenOpenToken = &ParenOpenToken_;
token ParenCloseToken_ = {ConstantTokenType}; Token ParenCloseToken = &ParenCloseToken_;
token CurlyOpenToken_ = {ConstantTokenType}; Token CurlyOpenToken = &CurlyOpenToken_;
token CurlyCloseToken_ = {ConstantTokenType}; Token CurlyCloseToken = &CurlyCloseToken_;
token BarToken_ = {ConstantTokenType}; Token BarToken = &BarToken_;
token CommaToken_ = {ConstantTokenType}; Token CommaToken = &CommaToken_;
token PeriodToken_ = {ConstantTokenType}; Token PeriodToken = &PeriodToken_;

word parse_infix(Stream s, word lhs, int precedence, map_t vars);
word parse_postfix(Stream s, word lhs, int precedence, map_t vars);


word syntax_error = 0; // FIXME: Not sure about what to do here


void freeToken(Token t)
{
   switch(t->type)
   {
      case ConstantTokenType:
         return;
      case AtomTokenType:
         free(t->data.atom_data->data);
         free(t->data.atom_data);
         free(t);
         break;
      case SyntaxErrorTokenType:
         free(t->data.syntax_error_data);
         free(t);
         break;
      case StringTokenType:
         free(t->data.atom_data->data);
         free(t->data.atom_data);
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

char* CharConversionTable = NULL; // FIXME

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
    if (get_prolog_flag("char_conversion") == falseAtom)
    {
       return get_raw_char(s);
    }
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
    if (get_prolog_flag("char_conversion") == falseAtom)
    {
        return peek_raw_char(s);
    }
    int t = peek_raw_char(s);
    int tt = CharConversionTable[t];
    if (tt == -1)
        return t;
    else
        return tt;
}

CharBuffer charBuffer()
{
   CharBuffer cb = malloc(sizeof(charbuffer));
   cb->head = NULL;
   cb->tail = NULL;
   cb->length = 0;
   return cb;
}

void pushChar(CharBuffer cb, int c)
{
   cb->length++;
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
      cb->tail = cb->tail->next;
      cb->tail->data = c;
      cb->tail->next = NULL;
   }
}

int bufferLength(CharBuffer buffer)
{
   return buffer->length;
}

char* finalize(CharBuffer buffer)
{
   CharCell* c = buffer->head;
   char* data = malloc(buffer->length + 1);
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
   data[i++] = '\0';
   free(buffer);
   return data;
}

Token SyntaxErrorToken(char* message)
{
   Token t = malloc(sizeof(token));
   t->type = SyntaxErrorTokenType;
   t->data.syntax_error_data = message;
   return t;

}

Token AtomToken(char* data, int length)
{
   Token t = malloc(sizeof(token));
   t->type = AtomTokenType;
   t->data.atom_data = malloc(sizeof(atom_data_t));
   t->data.atom_data->data = data;
   t->data.atom_data->length = length;
   return t;
}

Token VariableToken(char* name)
{
   Token t = malloc(sizeof(token));
   t->type = VariableTokenType;
   t->data.variable_data = name;
   return t;
}

Token BigIntegerToken(char* data)
{
   Token t = malloc(sizeof(token));
   t->type = BigIntegerTokenType;
   t->data.biginteger_data = data;
   return t;
}


Token StringToken(char* data, int length)
{
   Token t = malloc(sizeof(token));
   t->type = StringTokenType;
   t->data.atom_data = malloc(sizeof(atom_data_t));
   t->data.atom_data->data = data;
   t->data.atom_data->length = length;
   return t;
}

Token IntegerToken(long data)
{
   Token t = malloc(sizeof(token));
   t->type = IntegerTokenType;
   t->data.integer_data = data;
   return t;
}

Token FloatToken(double data)
{
   Token t = malloc(sizeof(token));
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
         return AtomToken(strdup("[]"), 2);
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
   else if (c == ',')
      return CommaToken;
   if ((c >= 'A' && c < 'Z') || c == '_')
   {
      CharBuffer sb = charBuffer();
      pushChar(sb, c);
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
      // FIXME: Should also handle 0x and 0b here
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
         int length = bufferLength(sb);
         char *data = finalize(sb);
         if (matcher == '"')
            return StringToken(data, length);
         else
            return AtomToken(data, length);
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
            {
               pushChar(sb, get_raw_char_with_conversion(s));
            }
            else if (punctuation_atom && is_graphic_char(c))
            {
               pushChar(sb, get_raw_char_with_conversion(s));
            }
            else
               break;
         }
         int length = bufferLength(sb);
         char* data = finalize(sb);
         return AtomToken(data, length);
      }
   }
   // This should be unreachable
   return NULL;
}

Token token_lookahead[3];
int token_lookahead_index = 0;

Token read_token(Stream s)
{
   if (token_lookahead_index > 0)
      return token_lookahead[--token_lookahead_index];
   return lex(s);
}

Token peek_token(Stream s)
{
   if (token_lookahead_index > 0)
      return token_lookahead[token_lookahead_index-1];
   Token t = lex(s);
   token_lookahead[token_lookahead_index++] = t;
   return t;
}

void unread_token(Stream s, Token t)
{
   if (token_lookahead_index == 3)
      printf("Horrible failure: Too many unread tokens\n");
   token_lookahead[token_lookahead_index++] = t;
}


int token_operator(Token t, Operator* op, OperatorPosition position)
{
   if (t->type != AtomTokenType)
      return 0;
   return find_operator(t->data.atom_data->data, op, position);
}

void cons(word cell, void* result)
{
   word* r = (word*)result;
   *r = MAKE_VCOMPOUND(listFunctor, cell, *r);
}

void curly_cons(word cell, void* result)
{
   word* r = (word*)result;
   *r = MAKE_VCOMPOUND(conjunctionFunctor, cell, *r);
}


word make_list(List* list, word tail)
{
   word result = tail;
   list_apply_reverse(list, &result, cons);
   return result;
}

word make_curly(List* list, word tail)
{
   word result = tail;
   list_apply_reverse(list, &result, curly_cons);
   return MAKE_VCOMPOUND(curlyFunctor, result);
}


word read_expression(Stream s, int precedence, int isArg, int isList, map_t vars)
{
   Token t = read_token(s);
   if (t == NULL)
      return endOfFileAtom;
   word lhs = 0;
   Operator prefixOperator;
   Operator infixOperator;
   int hasOp = token_operator(t, &prefixOperator, Prefix);
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
      if (t == ListOpenToken || t == CurlyOpenToken)
      {
         List list;
         init_list(&list);
         while (1)
         {
            if (peek_token(s) == ListCloseToken && t == ListOpenToken)
            {
               read_token(s);
               lhs = emptyListAtom;
               break;
            }
            if (peek_token(s) == CurlyCloseToken && t == CurlyOpenToken)
            {
               read_token(s);
               lhs = curlyAtom;
               break;
            }
            word arg = read_expression(s, 1201, 1, 1, vars);
            Token next = read_token(s);
            if (next == CommaToken)
            {
               list_append(&list, arg);
               continue;
            }
            else if (next == ListCloseToken && t == ListOpenToken)
            {
               list_append(&list, arg);
               lhs = make_list(&list, emptyListAtom);
               break;
            }
            else if (next == CurlyCloseToken && t == CurlyOpenToken)
            {
               lhs = make_curly(&list, arg);
               break;
            }
            else if (next == BarToken && t == ListOpenToken)
            {
               list_append(&list, arg);
               word tail = read_expression(s, 1201, 1, 1, vars);
               lhs = make_list(&list, tail);
               next = read_token(s);
               if (next == ListCloseToken)
                  break;
               return syntax_error; // Missing ]
            }
            else
               return syntax_error; // Mismatched 'token' at 'next'
         }
         free_list(&list);
      }
      else if (t == ParenOpenToken)
      {
         printf("()\n");
         lhs = read_expression(s, 12001, 0, 0, vars);
         Token next = read_token(s);
         if (next != ParenCloseToken)
            return syntax_error; // Mismatched ( at <next>
      }
      else if (t->type == VariableTokenType)
      {
         if (t->data.variable_data[0] == '_')
            lhs = MAKE_VAR();
         else
         {
            if (hashmap_get(vars, t->data.variable_data, (any_t*)&lhs) == MAP_MISSING)
            {
               lhs = MAKE_VAR();
               hashmap_put(vars, strdup(t->data.variable_data), (any_t)lhs);
            }
         }
      }
      else if (t == CurlyCloseToken)
         lhs = MAKE_ATOM("}");
      else
      {
         switch(t->type)
         {
            case AtomTokenType:
               lhs = MAKE_NATOM(t->data.atom_data->data, t->data.atom_data->length);
               break;
            case IntegerTokenType:
               lhs = MAKE_INTEGER(t->data.integer_data);
               break;
            case StringTokenType:
            {
               if (get_prolog_flag("double_quotes") == codesAtom)
               {
                  List list;
                  init_list(&list);
                  for (int i = 0; i < t->data.atom_data->length; i++)
                     list_append(&list, MAKE_INTEGER(t->data.atom_data->data[i]));
                  lhs = make_list(&list, emptyListAtom);
                  free_list(&list);
                  break;
               }
               else if (get_prolog_flag("double_quotes") == charsAtom)
               {
                  List list;
                  init_list(&list);
                  for (int i = 0; i < t->data.atom_data->length; i++)
                     list_append(&list, MAKE_NATOM(&t->data.atom_data->data[i], 1));
                  lhs = make_list(&list, emptyListAtom);
                  free_list(&list);
                  break;
               }
               else if (get_prolog_flag("double_quotes") == atomAtom)
               {
                  lhs = MAKE_NATOM(t->data.atom_data->data, t->data.atom_data->length);
                  break;
               }
            }
            default:

               printf("Not implemented yet\n");
               lhs = 0;
         }
      }
   }
   else if (prefixOperator->fixity == FX)
   {
      lhs = MAKE_VCOMPOUND(prefixOperator->functor, read_expression(s, prefixOperator->precedence, isArg, isList, vars));
   }
   else if (prefixOperator->fixity == FY)
   {
      lhs = MAKE_VCOMPOUND(prefixOperator->functor, read_expression(s, prefixOperator->precedence+1, isArg, isList, vars));
   }
   else
      return syntax_error; // parse error
   // At this point, lhs has been set
   while (1)
   {
      Token t1 = peek_token(s);
      if (t1->type == IntegerTokenType && t1->data.integer_data <= 0)
      {
         read_token(s);
         t1->data.integer_data = -t1->data.integer_data;
         unread_token(s, t1);
         unread_token(s, AtomToken(strdup("-"), 1));
      }
      if (t1 == ParenOpenToken)
      {
         // Reading a term. lhs is the name of the functor
         // First consume the (
         read_token(s);
         // Then read each arg into a list
         List list;
         init_list(&list);
         while (1)
         {
            list_append(&list, read_expression(s, 1201, 1, 0, vars));
            Token next = read_token(s);
            if (next == ParenCloseToken)
               break;
            else if (next == CommaToken)
               continue;
            else
            {
               printf("Syntax error\n");
               if (next == NULL)
                  return syntax_error; // end of file in term
               else
                  return syntax_error; // Unexpected token 'next'
            }
         }
         lhs = MAKE_LCOMPOUND(MAKE_FUNCTOR(lhs, list_length(&list)), &list);
         free_list(&list);
         // Now, where were we?
         t1 = peek_token(s);
      }
      if (t1 == PeriodToken)
         return lhs;
      if (t1 == CommaToken && isArg)
         return lhs;
      if (t1 == BarToken && isList)
         return lhs;
      if (t1 == NULL)
         return lhs;
      if (token_operator(t1, &infixOperator, Infix))
      {
         if (infixOperator->fixity == XFX && precedence > infixOperator->precedence)
            lhs = parse_infix(s, lhs, infixOperator->precedence, vars);
         else if (infixOperator->fixity == XFY && precedence > infixOperator->precedence)
            lhs = parse_infix(s, lhs, infixOperator->precedence+1, vars);
         else if (infixOperator->fixity == YFX && precedence >= infixOperator->precedence)
            lhs = parse_infix(s, lhs, infixOperator->precedence, vars);
         else if (infixOperator->fixity == XF && precedence > infixOperator->precedence)
            lhs = parse_postfix(s, lhs, infixOperator->precedence, vars);
         else if (infixOperator->fixity == YF && precedence >= infixOperator->precedence)
            lhs = parse_postfix(s, lhs, infixOperator->precedence, vars);
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
   assert(t->type == AtomTokenType);
   word rhs = read_expression(s, precedence, 0, 0, vars);
   return MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_NATOM(t->data.atom_data->data, t->data.atom_data->length), 2), lhs, rhs);
}

word parse_postfix(Stream s, word lhs, int precedence, map_t vars)
{
   Token t = read_token(s);
   assert(t->type == AtomTokenType);
   return MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_NATOM(t->data.atom_data->data, t->data.atom_data->length), 1), lhs);
}


int free_key(any_t ignored, char* key, any_t value)
{
   free(key);
   return MAP_OK;
}

word readTerm(Stream stream, void* options)
{
   token_lookahead_index = 0;
   map_t vars = hashmap_new();
   word t = read_expression(stream, 12001, 0, 0, vars);
   any_t tmp;
   hashmap_iterate(vars, free_key, &tmp);
   hashmap_free(vars);
   return t;
}
