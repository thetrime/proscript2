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
#include "options.h"
#include "operators.h"
#include "errors.h"

token ListOpenToken_ = {ConstantTokenType, .data = {.constant_data = "["}}; Token ListOpenToken = &ListOpenToken_;
token ListCloseToken_ = {ConstantTokenType, .data = {.constant_data = "]"}}; Token ListCloseToken = &ListCloseToken_;
token ParenOpenToken_ = {ConstantTokenType, .data = {.constant_data = "("}}; Token ParenOpenToken = &ParenOpenToken_;
token ParenCloseToken_ = {ConstantTokenType, .data = {.constant_data = ")"}}; Token ParenCloseToken = &ParenCloseToken_;
token CurlyOpenToken_ = {ConstantTokenType, .data = {.constant_data = "{"}}; Token CurlyOpenToken = &CurlyOpenToken_;
token CurlyCloseToken_ = {ConstantTokenType, .data = {.constant_data = "}"}}; Token CurlyCloseToken = &CurlyCloseToken_;
token BarToken_ = {ConstantTokenType, .data = {.constant_data = "|"}}; Token BarToken = &BarToken_;
token CommaToken_ = {ConstantTokenType, .data = {.constant_data = ","}}; Token CommaToken = &CommaToken_;
token PeriodToken_ = {ConstantTokenType, .data = {.constant_data = "."}}; Token PeriodToken = &PeriodToken_;

int parse_infix(Stream s, word lhs, int precedence, map_t vars, word*);
int parse_postfix(Stream s, word lhs, int precedence, map_t vars, word*);



void print_token(Token t)
{
   switch(t->type)
   {
      case ConstantTokenType:
         printf("%s", t->data.constant_data);
         return;
      case AtomTokenType:
         printf("%.*s", (int)t->data.atom_data->length, t->data.atom_data->data);
         return;
      case SyntaxErrorTokenType:
         printf("%s", t->data.syntax_error_data);
         break;
      case StringTokenType:
         printf("%.*s", (int)t->data.atom_data->length, t->data.atom_data->data);
         break;
      case IntegerTokenType:
         printf("%ld", t->data.integer_data);
         break;
      case FloatTokenType:
         printf("%f", t->data.float_data);
         break;
      case VariableTokenType:
         printf("%s", t->data.variable_data);
         break;
      case BigIntegerTokenType:
         printf("<bigint>");
         break;
   }
}

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

int is_graphic_char(char c)
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
    if (get_prolog_flag("char_conversion") == offAtom)
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
    if (get_prolog_flag("char_conversion") == offAtom)
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

void free_buffer(CharBuffer buffer)
{
   free(buffer);
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
   syntax_error(MAKE_ATOM(message));
   return NULL;

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
   else if (c == '.')
      return PeriodToken;
   if ((c >= 'A' && c <= 'Z') || c == '_')
   {
      CharBuffer sb = charBuffer();
      pushChar(sb, c);
      while (1)
      {
         c = peek_raw_char_with_conversion(s);
         if (is_char(c))
            pushChar(sb, get_raw_char_with_conversion(s));
         else
            return VariableToken(finalize(sb));
      }
   }
   else if ((c >= '0' && c <= '9') || (c == '-' && peek_raw_char_with_conversion(s) >= '0' && peek_raw_char_with_conversion(s) <= '9'))
   {
      int base = 10;
      if (c == '0' && peek_raw_char_with_conversion(s) == '\'')
      {
         // Char code 0'x
         get_raw_char_with_conversion(s);
         return IntegerToken(get_raw_char_with_conversion(s));
      }
      // Parse a number
      CharBuffer sb = charBuffer();
      pushChar(sb, c);
      if (c == '0')
      {
         char b = peek_raw_char_with_conversion(s);
         if (b == 'x')
         {
            base = 16;
            pushChar(sb, b);
            get_raw_char_with_conversion(s);
         }
         else if (b == 'b')
         {
            base = 2;
            pushChar(sb, b);
            get_raw_char_with_conversion(s);
         }
         if (b == 'o')
         {
            base = 8;
            pushChar(sb, b);
            get_raw_char_with_conversion(s);
         }
      }
      int seen_decimal = 0;
      int seen_exp = 0;
      while(1)
      {
         c = peek_raw_char_with_conversion(s);
         if (c >= '0' && ((base == 10 && c <= '9') || (base == 8 && c <= '8') || (base == 2 && c <= '1') || (base == 16 && c <= '9')))
         {
               pushChar(sb, c);
               get_raw_char_with_conversion(s);
         }
         else if (base == 16 && ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
         {
            pushChar(sb, c);
            get_raw_char_with_conversion(s);
         }
         else if (c == '.' && !seen_decimal && base == 10)
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
         else if ((c == 'E' || c == 'e') && !seen_exp  && base == 10)
         {
            seen_exp = 1;
            pushChar(sb, c);
            get_raw_char_with_conversion(s);
         }
         else if ((c == '+' || c == '-') && seen_exp && base == 10)
         {
            pushChar(sb, c);
            get_raw_char_with_conversion(s);
         }
         else if (is_char(c))
         {
            printf("Bad char %d\n", c);
            return SyntaxErrorToken("illegal number");
         }
         else
            break;
      }
      if (!seen_decimal && !seen_exp)
      {
         char* str = finalize(sb);
         char* sp;
         long l = strtol(str, &sp, base);
         if (errno == ERANGE)
         {
            return BigIntegerToken(str);
         }
         else
         {
            free(str);
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
      int is_escape = 0;
      if (c == '\'' || c == '"')
      {
         CharBuffer sb = charBuffer();
         int matcher = c;
         while (1)
         {
            c = get_raw_char_with_conversion(s);
            if (c == -1)
            {
               free_buffer(sb);
               return SyntaxErrorToken("end of file in atom");
            }
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
   assert(0 && "This should not be reachable");
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
   if (t->type == ConstantTokenType)
   {
      if (t == CommaToken)
         return find_operator(",", op, position);
   }
   if (t->type != AtomTokenType)
      return 0;
   return find_operator(t->data.atom_data->data, op, position);
}
void curly_cons(word cell, void* result)
{
   word* r = (word*)result;
   *r = MAKE_VCOMPOUND(conjunctionFunctor, cell, *r);
}

word make_curly(List* list, word tail)
{
   word result = tail;
   list_apply_reverse(list, &result, curly_cons);
   return MAKE_VCOMPOUND(curlyFunctor, result);
}


int read_expression(Stream s, int precedence, int isArg, int isList, map_t vars, word* result)
{
   Token t0 = read_token(s);
   if (t0 == NULL)
   {
      *result = endOfFileAtom;
      return 1;
   }
   word lhs = 0;
   Operator prefixOperator;
   Operator infixOperator;
   int hasOp = token_operator(t0, &prefixOperator, Prefix);
   Token peeked_token = peek_token(s);
   if (peeked_token == ListCloseToken || peeked_token == ParenCloseToken || peeked_token == CommaToken)
      hasOp = 0;
   if (peeked_token == ParenOpenToken)
   {
      read_token(s); // Dont need to free this - guaranteed to be constant
      Token pp = peek_token(s);
      unread_token(s, ParenOpenToken);
      if (pp != ParenOpenToken)
         hasOp = 0;
   }
   if (!hasOp)
   {
      if (t0 == ListOpenToken || t0 == CurlyOpenToken)
      {
         List list;
         init_list(&list);
         while (1)
         {
            if (peek_token(s) == ListCloseToken && t0 == ListOpenToken)
            {
               read_token(s);  // Dont need to free this - guaranteed to be constant
               lhs = emptyListAtom;
               break;
            }
            if (peek_token(s) == CurlyCloseToken && t0 == CurlyOpenToken)
            {
               read_token(s);  // Dont need to free this - guaranteed to be constant
               lhs = curlyAtom;
               break;
            }
            word arg;
            if (!read_expression(s, 1201, 1, 1, vars, &arg))
            {
               // FIXME
            }
            Token next = read_token(s);
            if (next == CommaToken)
            {
               list_append(&list, arg);
               continue;
            }
            else if (next == ListCloseToken && t0 == ListOpenToken)
            {
               list_append(&list, arg);
               lhs = term_from_list(&list, emptyListAtom);
               break;
            }
            else if (next == CurlyCloseToken && t0 == CurlyOpenToken)
            {
               lhs = make_curly(&list, arg);
               break;
            }
            else if (next == BarToken && t0 == ListOpenToken)
            {
               list_append(&list, arg);
               word tail;
               if (!read_expression(s, 1201, 1, 1, vars, &tail))
               {
                  // FIXME
               }
               lhs = term_from_list(&list, tail);
               next = read_token(s);
               if (next == ListCloseToken)
                  break;
               return syntax_error(MAKE_ATOM("Missing ]"));
            }
            else
            {
               freeToken(next);
               return syntax_error(MAKE_ATOM("Mismatched <token> at <next>"));
            }
         }
         free_list(&list);
      }
      else if (t0 == ParenOpenToken)
      {
         if (!read_expression(s, 12001, 0, 0, vars, &lhs))
         {
            // FIXME:
         }
         Token next = read_token(s);
         if (next != ParenCloseToken)
            return syntax_error(MAKE_ATOM("Mismatched ( at <next>"));
      }
      else if (t0->type == VariableTokenType)
      {
         if (t0->data.variable_data[0] == '_')
            lhs = MAKE_VAR();
         else
         {
            if (hashmap_get(vars, t0->data.variable_data, (any_t*)&lhs) == MAP_MISSING)
            {
               lhs = MAKE_VAR();
               hashmap_put(vars, strdup(t0->data.variable_data), (any_t)lhs);
            }
         }
      }
      else if (t0 == CurlyCloseToken)
         lhs = MAKE_ATOM("}");
      else
      {
         switch(t0->type)
         {
            case AtomTokenType:
               lhs = MAKE_NATOM(t0->data.atom_data->data, t0->data.atom_data->length);
               break;
            case ConstantTokenType:
               lhs = MAKE_ATOM(t0->data.constant_data);
               break;
            case IntegerTokenType:
               lhs = MAKE_INTEGER(t0->data.integer_data);
               break;
            case FloatTokenType:
               lhs = MAKE_FLOAT(t0->data.float_data);
               break;
            case StringTokenType:
            {
               if (get_prolog_flag("double_quotes") == codesAtom)
               {
                  List list;
                  init_list(&list);
                  for (int i = 0; i < t0->data.atom_data->length; i++)
                     list_append(&list, MAKE_INTEGER(t0->data.atom_data->data[i]));
                  lhs = term_from_list(&list, emptyListAtom);
                  free_list(&list);
                  break;
               }
               else if (get_prolog_flag("double_quotes") == charsAtom)
               {
                  List list;
                  init_list(&list);
                  for (int i = 0; i < t0->data.atom_data->length; i++)
                     list_append(&list, MAKE_NATOM(&t0->data.atom_data->data[i], 1));
                  lhs = term_from_list(&list, emptyListAtom);
                  free_list(&list);
                  break;
               }
               else if (get_prolog_flag("double_quotes") == atomAtom)
               {
                  lhs = MAKE_NATOM(t0->data.atom_data->data, t0->data.atom_data->length);
                  break;
               }
            }
            case BigIntegerTokenType:
            {
               mpz_t m;
               mpz_init_set_str(m, t0->data.biginteger_data, 10);
               lhs = MAKE_BIGINTEGER(m);
               break;
            }
            case SyntaxErrorTokenType:
            {
               // FIXME: Not good
               lhs = MAKE_ATOM("syntax_error");
               break;
            }
            default:
               printf("Token type %d not implemented yet\n", t0->type);
               assert(0);
         }
      }
   }
   else if (prefixOperator->fixity == FX)
   {
      word w;
      if (!read_expression(s, prefixOperator->precedence, isArg, isList, vars, &w))
      {
         // FIXME:
      }
      lhs = MAKE_VCOMPOUND(prefixOperator->functor, w);
   }
   else if (prefixOperator->fixity == FY)
   {
      word w;
      if (!read_expression(s, prefixOperator->precedence+1, isArg, isList, vars, &w))
      {
         // FIXME:
      }
      lhs = MAKE_VCOMPOUND(prefixOperator->functor, w);
   }
   else
      return syntax_error(MAKE_ATOM("Parse error"));
   // At this point, lhs has been set. We no longer need t0
   freeToken(t0);
   while (1)
   {
      Token t1 = peek_token(s);
      if (t1 == NULL)
      {
         *result = lhs;
         return 1;
      }
      if (t1->type == IntegerTokenType && t1->data.integer_data <= 0)
      {
         Token t2 = read_token(s);
         t2->data.integer_data = -t2->data.integer_data;
         unread_token(s, t2);
         // Must not free t2 since we will read it later
         unread_token(s, AtomToken(strdup("-"), 1));
      }
      else if (t1 == ParenOpenToken)
      {
         // Reading a term. lhs is the name of the functor
         // First consume the (
         read_token(s); // No need to free as guaranteed to be constant
         // Then read each arg into a list
         List list;
         init_list(&list);
         while (1)
         {
            word w;
            if (!read_expression(s, 1201, 1, 0, vars, &w))
            {
               // FIXME:
            }
            list_append(&list, w);
            Token next = read_token(s);
            if (next == ParenCloseToken)
               break;
            else if (next == CommaToken)
               continue;
            else
            {
               if (next == NULL)
               {
                  printf("Syntax error: end of file in term\n");
                  return syntax_error(MAKE_ATOM("end of file in term"));
               }
               else
               {
                  printf("Syntax error: Unexpected token ");
                  print_token(next);
                  printf("\n");
                  freeToken(next);
                  return syntax_error(MAKE_ATOM("unexpected token <next>"));
               }
            }
         }
         lhs = MAKE_LCOMPOUND(MAKE_FUNCTOR(lhs, list_length(&list)), &list);
         free_list(&list);
         // Now, where were we?
         t1 = peek_token(s);
      }
      else if (t1 == PeriodToken)
      {
         *result = lhs;
         return 1;
      }
      else if (t1 == CommaToken && isArg)
      {
         *result = lhs;
         return 1;
      }
      else if (t1 == BarToken && isList)
      {
         *result = lhs;
         return 1;
      }
      else if (t1 == NULL)
      {
         *result = lhs;
         return 1;
      }
      else if (token_operator(t1, &infixOperator, Infix))
      {
         if (infixOperator->fixity == XFX && precedence > infixOperator->precedence)
         {
            if (!parse_infix(s, lhs, infixOperator->precedence, vars, &lhs))
               return 0;
         }
         else if (infixOperator->fixity == XFY && precedence > infixOperator->precedence)
         {
            if (!parse_infix(s, lhs, infixOperator->precedence+1, vars, &lhs))
               return 0;
         }
         else if (infixOperator->fixity == YFX && precedence >= infixOperator->precedence)
         {
            if (!parse_infix(s, lhs, infixOperator->precedence, vars, &lhs))
               return 0;
         }
         else if (infixOperator->fixity == XF && precedence > infixOperator->precedence)
         {
            if (!parse_postfix(s, lhs, infixOperator->precedence, vars, &lhs))
               return 0;
         }
         else if (infixOperator->fixity == YF && precedence >= infixOperator->precedence)
         {
            if (!parse_postfix(s, lhs, infixOperator->precedence, vars, &lhs))
               return 0;
         }
         else
         {
            *result = lhs;
            return 1;
         }
      }
      else
      {
         *result = lhs;
         return 1;
      }
   }
}

int parse_infix(Stream s, word lhs, int precedence, map_t vars, word* result)
{
   Token t = read_token(s);
   assert(t->type == AtomTokenType || t == CommaToken);
   char* data;
   int len;
   if (t->type == AtomTokenType)
   {
      data = t->data.atom_data->data;
      len = t->data.atom_data->length;
   }
   else if (t == CommaToken)
   {
      data = ",";
      len = 1;
   }
   word rhs;
   if (!read_expression(s, precedence, 0, 0, vars, &rhs))
   {
      freeToken(t);
      return 0;
   }
   *result = MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_NATOM(data, len), 2), lhs, rhs);
   freeToken(t);
   return 1;
}

int parse_postfix(Stream s, word lhs, int precedence, map_t vars, word* result)
{
   Token t = read_token(s);
   assert(t->type == AtomTokenType || t == CommaToken);
   *result = MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_NATOM(t->data.atom_data->data, t->data.atom_data->length), 1), lhs);
   freeToken(t);
   return 1;
}


int free_key(any_t ignored, char* key, any_t value)
{
   free(key);
   return MAP_OK;
}

int read_term(Stream stream, Options* options, word* t)
{
   token_lookahead_index = 0;
   map_t vars = hashmap_new();
   int rc = read_expression(stream, 12001, 0, 0, vars, t);
   any_t tmp;
   hashmap_iterate(vars, free_key, &tmp);
   hashmap_free(vars);
   //printf("Read term: "); PORTRAY(t); printf("\n");
   return rc;
}
