#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <assert.h>
#include <string.h>

#include "types.h"
#include "kernel.h"
#include "ctable.h"
#include "module.h"
#include "options.h"
#include "string_builder.h"
#include "term_writer.h"
#include "parser.h"

EMSCRIPTEN_KEEPALIVE
int atom_length(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == ATOM_TYPE);
   return c.data.atom_data->length;
}

EMSCRIPTEN_KEEPALIVE
char* atom_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == ATOM_TYPE);
   return c.data.atom_data->data;
}

EMSCRIPTEN_KEEPALIVE
long integer_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == INTEGER_TYPE);
   return c.data.integer_data->data;
}

EMSCRIPTEN_KEEPALIVE
double float_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == FLOAT_TYPE);
   return c.data.float_data->data;
}

EMSCRIPTEN_KEEPALIVE
int term_type(word a)
{
   if (TAGOF(a) == CONSTANT_TAG)
      return getConstant(a).type;
   else if (TAGOF(a) == VARIABLE_TAG)
      return 127;
   else if (TAGOF(a) == COMPOUND_TAG)
      return 128;
   else if (TAGOF(a) == POINTER_TAG)
      return 129;
   return -1;
}

EMSCRIPTEN_KEEPALIVE
int _is_blob(word a, char* type)
{
   constant c = getConstant(a);
   return (c.type == BLOB_TYPE && strcmp(type, c.data.blob_data->type) == 0);
}

EMSCRIPTEN_KEEPALIVE
word _term_functor(word a)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return FUNCTOROF(a);
}

EMSCRIPTEN_KEEPALIVE
word _term_functor_name(word a)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return getConstant(FUNCTOROF(a)).data.functor_data->name;
}

EMSCRIPTEN_KEEPALIVE
int _term_functor_arity(word a)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return getConstant(FUNCTOROF(a)).data.functor_data->arity;
}

EMSCRIPTEN_KEEPALIVE
word _term_arg(word a, int i)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return ARGOF(a, i);
}

EMSCRIPTEN_KEEPALIVE
word _make_atom(char* a, int l)
{
   return MAKE_NATOM(a, l);
}

EMSCRIPTEN_KEEPALIVE
word _make_functor(word name, int arity)
{
   return MAKE_FUNCTOR(name, arity);
}

EMSCRIPTEN_KEEPALIVE
word _make_compound(word functor, word* args)
{
   return MAKE_ACOMPOUND(functor, args);
}

EMSCRIPTEN_KEEPALIVE
word make_variable()
{
   return MAKE_VAR();
}

EMSCRIPTEN_KEEPALIVE
int _set_exception(word error)
{
   printf("Setting an exception: "); PORTRAY(error); printf("\n");
   return SET_EXCEPTION(error);
}

EMSCRIPTEN_KEEPALIVE
extern int unify(word, word);

EMSCRIPTEN_KEEPALIVE
word _get_exception()
{
   return getException();
}

EMSCRIPTEN_KEEPALIVE
void _clear_exception()
{
   CLEAR_EXCEPTION();
}

char* _portray_js_blob(char* type, void* ptr, Options* options, int precedence, int* len)
{
   return (char*)EM_ASM_INT({return portray_js_blob($0, $1, $2, $3, $4, $5)}, type, strlen(type), (int)ptr, options, precedence, len);
}

EMSCRIPTEN_KEEPALIVE
word _make_blob(char* type, int key)
{
   //printf("Making blob of type %s\n", type);
   word q = intern_blob(type, (void*)key, _portray_js_blob);
   //printf("Allocated blob %08x\n", q);
   return q;
}

EMSCRIPTEN_KEEPALIVE
int _get_blob(char* type, word w)
{
   Blob b = getConstant(w).data.blob_data;
   if (strcmp(type, b->type) != 0)
   {
      printf("Blob mismatch: Expecting %s but blob passed in is of type %s\n", type, b->type);
      assert(0);
   }
   return (int)b->ptr;
}

EMSCRIPTEN_KEEPALIVE
void _consult_string(char* string)
{
   consult_string(string);
}

EMSCRIPTEN_KEEPALIVE
int _exists_predicate(word module, word functor)
{
   Module m = find_module(module);
   if (m != NULL)
      return lookup_predicate(m, functor) != NULL;
   return 0;
}

void jscall(RC result)
{
   EM_ASM_({_jscallback($0)}, (result == SUCCESS || result == SUCCESS_WITH_CHOICES));
}

EMSCRIPTEN_KEEPALIVE
void executejs(word goal, int callback_ref)
{
   //printf("Executing: "); PORTRAY(goal); printf("\n");
   execute_query(goal, jscall);
}

EMSCRIPTEN_KEEPALIVE
void _format_term(Options* options, int priority, word term, char** ptr, int* length)
{
   StringBuilder formatted = stringBuilder();
   format_term(formatted, options, priority, term);
   finalize_buffer(formatted, ptr, length);
   //printf("-> "); PORTRAY(term);
   //printf(" turned into string at %p (%d): %s\n", *ptr, *length, *ptr);
}

EMSCRIPTEN_KEEPALIVE
Options* _create_options()
{
   Options* options = malloc(sizeof(Options));
   init_options(options);
   return options;
}

EMSCRIPTEN_KEEPALIVE
void _set_option(Options* options, word key, word value)
{
   set_option(options, key, value);
}

EMSCRIPTEN_KEEPALIVE
void _free_options(Options* options)
{
   free_options(options);
}


EMSCRIPTEN_KEEPALIVE
Choicepoint _push_state()
{
   return push_state();
}

EMSCRIPTEN_KEEPALIVE
void _restore_state(Choicepoint w)
{
   restore_state(w);
}

EMSCRIPTEN_KEEPALIVE
word string_to_local_term(char* string, int length)
{
   Stream stream = stringBufferStream(string, length);
   word w;
   if (read_term(stream, NULL, &w) == 0)
      return 0;
   word* ptr;
   copy_local(w, &ptr);
   freeStream(stream);
   return (word)ptr;
}

#endif

