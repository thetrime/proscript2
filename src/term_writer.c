#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include "options.h"
#include "constants.h"
#include "ctable.h"
#include "stream.h"
#include "kernel.h"
#include "parser.h"
#include "operators.h"

struct string_cell_t
{
   struct string_cell_t* next;
   char* data;
   int length;
   int must_free;
};

typedef struct string_cell_t string_cell_t;

typedef struct
{
   struct string_cell_t* head;
   struct string_cell_t* tail;
   int length;
} string_builder;

typedef string_builder* StringBuilder;

StringBuilder stringBuilder()
{
   StringBuilder b = malloc(sizeof(string_builder));
   b->head = NULL;
   b->tail = NULL;
   b->length = 0;
   return b;
}

// Do not free things passed to append_string - they will be freed for you
void append_string_no_copy(StringBuilder b, char* data, int length)
{
   if (b->head == NULL)
   {
      b->head = malloc(sizeof(string_cell_t));
      b->tail = b->head;
   }
   else
   {
      b->tail->next = malloc(sizeof(string_cell_t));
      b->tail = b->tail->next;
   }
   b->tail->must_free = 0;
   b->tail->next = NULL;
   b->tail->data = data;
   b->tail->length = length;
   b->length += length;
}

void append_string(StringBuilder b, char* data, int length)
{
   if (b->head == NULL)
   {
      b->head = malloc(sizeof(string_cell_t));
      b->tail = b->head;
   }
   else
   {
      b->tail->next = malloc(sizeof(string_cell_t));
      b->tail = b->tail->next;
   }
   b->tail->must_free = 1;
   b->tail->next = NULL;
   b->tail->data = data;
   b->tail->length = length;
   b->length += length;
}


void finalize_buffer(StringBuilder b, char** text, int* length)
{
   *length = b->length;
   *text = malloc(*length);
   string_cell_t* cell = b->head;
   int i = 0;
   while (cell != NULL)
   {
      memcpy(*text + i, cell->data, cell->length);
      i+= cell->length;
      string_cell_t* tmp = cell->next;
      if (cell->must_free)
         free(cell->data);
      free(cell);
      cell = tmp;
   }
   free(b);
}

void quote_string(StringBuilder sb, Atom a)
{
   append_string_no_copy(sb, "'", 1);
   for (int i = 0; i < a->length; i++)
   {
      switch(a->data[i])
      {
         case '\'':
            append_string_no_copy(sb, "\\\'", 2); break;
         case '\\':
            append_string_no_copy(sb, "\\\\", 2); break;
         case 7:
            append_string_no_copy(sb, "\\a", 2); break;
         case 8:
            append_string_no_copy(sb, "\\b", 2); break;
         case 9:
            append_string_no_copy(sb, "\\t", 2); break;
         case 10:
            append_string_no_copy(sb, "\\n", 2); break;
         case 11:
            append_string_no_copy(sb, "\\v", 2); break;
         case 12:
            append_string_no_copy(sb, "\\f", 2); break;
         case 13:
            append_string_no_copy(sb, "\\r", 2); break;
         default:
            append_string_no_copy(sb, &a->data[i], 1); break;
      }
   }
   append_string_no_copy(sb, "'", 1);
}

int is_atom_start(char ch)
{
    // FIXME: unicode?
    return ch >= 'a' && ch <= 'z';
}

int is_atom_continuation(char ch)
{
    // FIXME: And also anything that unicode considers to be not punctuation? I have no idea.
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9')
        || (ch == '_');
}

int needs_quote(Atom a)
{
   if (a->length == 0)
      return 1;
   char ch = a->data[0];
   if (ch == '%')
      return 1;
   if (ch == ';' || ch == '!' || ch == '(' || ch == ')' || ch == ',' || ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == '%')
      return a->length != 1;
   if (is_graphic_char(ch))
   {
      // Things that look like comments need to be quoted
      if (a->length >= 2 && strncmp(a->data, "/*", 2) == 0)
         return 1;
      // If all characters are graphic, then we are OK. Otherwise we must quote
      for (int i = 1; i < a->length; i++)
      {
         if (!is_graphic_char(a->data[i]))
            return 1;
      }
      return 0;
   }
   else if (is_atom_start(ch))
   {
      // If there are only atom_char characters then we are OK. Otherwise we must quote
      for (int i = 1; i < a->length; i++)
      {
         if (!is_atom_continuation(a->data[i]))
            return 1;
      }
      return 0;
   }
   return 1;
}

int format_atom(StringBuilder sb, Options* options, word term)
{
   Atom a = getConstant(term).data.atom_data;
   if (get_option(options, quotedAtom, falseAtom) == trueAtom)
   {
      if (term == emptyListAtom)
      {
         append_string_no_copy(sb, "[]", 2);
         return 1;
      }
      if (needs_quote(a))
         quote_string(sb, a);
      else
         append_string_no_copy(sb, a->data, a->length);
   }
   else
      append_string_no_copy(sb, a->data, a->length);
   return 1;
}

Operator get_op(word functor, Options* options)
{
//   if (get_option(options, operatorsAtom, hmm
   Functor f = getConstant(functor).data.functor_data;
   if (f->arity < 1 || f->arity > 2)
      return NULL;
   Operator op;
   if (f->arity == 1 && find_operator(getConstant(f->name).data.atom_data->data, &op, Prefix))
      return op;
   else if (f->arity == 2 && find_operator(getConstant(f->name).data.atom_data->data, &op, Infix))
      return op;
   else
      return NULL;
}

void glue_atoms(StringBuilder target, StringBuilder left, StringBuilder right)
{
   char* lc, * rc;
   int ll, rl;
   finalize_buffer(left, &lc, &ll);
   finalize_buffer(right, &rc, &rl);
   if (is_graphic_char(lc[ll-1]) && !is_graphic_char(rc[0]))
   {
      append_string(target, lc, ll);
      append_string(target, rc, rl);
   }
   else if (!is_graphic_char(lc[ll-1]) && is_graphic_char(rc[0]))
   {
      append_string(target, lc, ll);
      append_string(target, rc, rl);
   }
   else
   {
      append_string(target, lc, ll);
      append_string_no_copy(target, " ", 1);
      append_string(target, rc, rl);
   }
}


int format_term(StringBuilder sb, Options* options, int precedence, word term)
{
   term = DEREF(term);
   if (TAGOF(term) == VARIABLE_TAG)
   {
      char buffer[64];
      sprintf((char*)buffer, "_G%" PRIuPTR, (term - (word)HEAP));
      append_string(sb, strdup(buffer), strlen(buffer));
      return 1;
   }
   else if (TAGOF(term) == POINTER_TAG)
   {
      char buffer[64];
      sprintf((char*)buffer, "#%p", GET_POINTER(term));
      append_string(sb, strdup(buffer), strlen(buffer));
      return 1;
   }
   else if (TAGOF(term) == CONSTANT_TAG)
   {
      constant c = getConstant(term);
      // FIXME: terms specifying their own formatter
      if (c.type == INTEGER_TYPE)
      {
         char buffer[64];
         sprintf((char*)buffer, "%ld", c.data.integer_data->data);
         append_string(sb, strdup(buffer), strlen(buffer));
         return 1;
      }
      if (c.type == BLOB_TYPE)
      {
         Blob b = c.data.blob_data;
         append_string_no_copy(sb, b->type, strlen(b->type));
         char buffer[64];
         sprintf((char*)buffer, "<%p>", b->ptr);
         append_string(sb, strdup(buffer), strlen(buffer));
         return 1;
      }
      if (c.type == FLOAT_TYPE)
      {
         char buffer[64];
         sprintf((char*)buffer, "%f", c.data.float_data->data);
         append_string(sb, strdup(buffer), strlen(buffer));
         return 1;
      }
      if (c.type == ATOM_TYPE)
      {
         return format_atom(sb, options, term);
      }
      if (c.type == BIGINTEGER_TYPE)
      {
         char* str = mpz_get_str(NULL, 10, c.data.biginteger_data->data);
         // str will be freed later by finalize_buffer()
         append_string(sb, str, strlen(str));
         return 1;
      }
      if (c.type == RATIONAL_TYPE)
      {
         mpz_t num;
         mpz_t den;
         mpz_init(num);
         mpz_init(den);
         mpq_get_num(num, c.data.rational_data->data);
         mpq_get_den(den, c.data.rational_data->data);
         char* str;
         if (mpz_cmp_ui(den, 1) == 0)
         {
            str = mpz_get_str(NULL, 10, num);
            append_string(sb, str, strlen(str));
         }
         else
         {
            str = mpz_get_str(NULL, 10, num);
            append_string(sb, str, strlen(str));
            append_string_no_copy(sb, " rdiv ", 6);
            str = mpz_get_str(NULL, 10, den);
            append_string(sb, str, strlen(str));
         }
         mpz_clear(den);
         mpz_clear(num);
         return 1;
      }
      printf("Type: %d from %08lx\n", c.type, term);
      assert(0 && "Unhandled constant type");
   }
   else if (get_option(options, numbervarsAtom, falseAtom) == trueAtom && TAGOF(term) == COMPOUND_TAG && FUNCTOROF(term) == numberedVarFunctor && TAGOF(ARGOF(term,0)) == CONSTANT_TAG && getConstant(ARGOF(term,0)).type == INTEGER_TYPE)
   {
      constant c = getConstant(ARGOF(term, 0));
      int i = c.data.integer_data->data % 26;
      int j = c.data.integer_data->data / 26;
      char buffer[64];
      if (j == 0)
      {
         buffer[0] = 65+i;
         buffer[1] = 0;
      }
      else
      {
         buffer[0] = 65+i;
         sprintf(&buffer[1], "%d", j);
      }
      append_string(sb, strdup(buffer), strlen(buffer));
      return 1;
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      word functor = FUNCTOROF(term);
      Operator op = get_op(functor, options);
      if (functor == listFunctor && get_option(options, ignoreOpsAtom, falseAtom) == falseAtom)
      {
         append_string_no_copy(sb, "[", 1);
         word head = ARGOF(term, 0);
         word tail = ARGOF(term, 1);
         while (1)
         {
            format_term(sb, options, 1200, head);
            if (TAGOF(tail) == COMPOUND_TAG && FUNCTOROF(tail) == listFunctor)
            {
               append_string_no_copy(sb, ",", 1);
               head = ARGOF(tail, 0);
               tail = ARGOF(tail, 1);
               continue;
            }
            else if (tail == emptyListAtom)
            {
               append_string_no_copy(sb, "]", 1);
               return 1;
            }
            else
            {
               append_string_no_copy(sb, "|", 1);
               format_term(sb, options, 1200, tail);
               append_string_no_copy(sb, "]", 1);
               return 1;
            }
         }
      }
      if (functor == curlyFunctor && get_option(options, ignoreOpsAtom, falseAtom) == falseAtom)
      {
         append_string_no_copy(sb, "{", 1);
         word head = ARGOF(term, 0);
         while (TAGOF(head) == COMPOUND_TAG && FUNCTOROF(head) == conjunctionFunctor)
         {
            format_term(sb, options, 1200, ARGOF(head,0));
            append_string_no_copy(sb, ",", 1);
            head = ARGOF(head, 1);
         }
         format_term(sb, options, 1200, head);
         append_string_no_copy(sb, "}", 1);
         return 1;
      }
      else if (op == NULL || get_option(options, ignoreOpsAtom, falseAtom) == trueAtom)
      {
         Functor f = getConstant(functor).data.functor_data;
         format_atom(sb, options, f->name);
         append_string_no_copy(sb, "(", 1);
         for (int i = 0; i < f->arity; i++)
         {
            format_term(sb, options, 1200, ARGOF(term, i));
            if (i+1 < f->arity)
               append_string_no_copy(sb, ",", 1);
         }
         append_string_no_copy(sb, ")", 1);
         return 1;
      }
      else if (op != NULL && get_option(options, ignoreOpsAtom, falseAtom) == falseAtom)
      {
         Functor f = getConstant(functor).data.functor_data;
         StringBuilder sb1 = stringBuilder();
         if (op->precedence > precedence)
            append_string_no_copy(sb1, "(", 1);
         switch(op->fixity)
         {
            case FX:
            {
               StringBuilder sb2 = stringBuilder();
               format_atom(sb1, options, f->name);
               format_term(sb2, options, precedence-1, ARGOF(term, 0));
               glue_atoms(sb, sb1, sb2);
               break;
            }
            case FY:
            {
               StringBuilder sb2 = stringBuilder();
               format_atom(sb1, options, f->name);
               format_term(sb2, options, precedence, ARGOF(term, 0));
               glue_atoms(sb, sb1, sb2);
               break;
            }
            case XFX:
            {
               StringBuilder sb2 = stringBuilder();
               StringBuilder sb3 = stringBuilder();
               StringBuilder sb4 = stringBuilder();
               format_term(sb1, options, precedence-1, ARGOF(term, 0));
               format_atom(sb2, options, f->name);
               format_term(sb3, options, precedence-1, ARGOF(term, 1));
               glue_atoms(sb4, sb1, sb2);
               glue_atoms(sb, sb4, sb3);
               break;
            }
            case XFY:
            {
               StringBuilder sb2 = stringBuilder();
               StringBuilder sb3 = stringBuilder();
               StringBuilder sb4 = stringBuilder();
               format_term(sb1, options, precedence-1, ARGOF(term, 0));
               format_atom(sb2, options, f->name);
               format_term(sb3, options, precedence, ARGOF(term, 1));
               glue_atoms(sb4, sb1, sb2);
               glue_atoms(sb, sb4, sb3);
               break;
            }
            case YFX:
            {
               StringBuilder sb2 = stringBuilder();
               StringBuilder sb3 = stringBuilder();
               StringBuilder sb4 = stringBuilder();
               format_term(sb1, options, precedence, ARGOF(term, 0));
               format_atom(sb2, options, f->name);
               format_term(sb3, options, precedence-1, ARGOF(term, 1));
               glue_atoms(sb4, sb1, sb2);
               glue_atoms(sb, sb4, sb3);
               break;
            }
            case XF:
            {
               StringBuilder sb2 = stringBuilder();
               format_term(sb1, options, precedence-1, ARGOF(term, 0));
               format_atom(sb2, options, f->name);
               glue_atoms(sb, sb1, sb2);
               break;
            }
            case YF:
            {
               StringBuilder sb2 = stringBuilder();
               format_term(sb1, options, precedence, ARGOF(term, 0));
               format_atom(sb2, options, f->name);
               glue_atoms(sb, sb1, sb2);
               break;
            }
         }
         if (op->precedence > precedence)
            append_string_no_copy(sb, ")", 1);
         return 1;
      }
   }
   assert(0 && "Unknown term type");
}

int write_term(Stream stream, word term, Options* options)
{
   StringBuilder formatted = stringBuilder();
   format_term(formatted, options, 1200, term);
   char* text;
   int length;
   finalize_buffer(formatted, &text, &length);
   int rc = stream->write(stream, length, (unsigned char*)text) >= 0;
   free(text);
   return rc;
}

