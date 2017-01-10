#include "global.h"
#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "types.h"
#include "kernel.h"
#include "ctable.h"
#include "constants.h"
#include "module.h"
#include "options.h"
#include "string_builder.h"
#include "term_writer.h"
#include "parser.h"

EMSCRIPTEN_KEEPALIVE
int _atom_length(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   int type;
   cdata c = getConstant(a, &type);
   assert(type == ATOM_TYPE);
   return c.atom_data->length;
}

EMSCRIPTEN_KEEPALIVE
const char* _atom_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   int type;
   cdata c = getConstant(a, &type);
   assert(type == ATOM_TYPE);
   return c.atom_data->data;
}

EMSCRIPTEN_KEEPALIVE
long integer_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   int type;
   cdata c = getConstant(a, &type);
   assert(type == INTEGER_TYPE);
   return c.integer_data;
}

EMSCRIPTEN_KEEPALIVE
double float_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   int type;
   cdata c = getConstant(a, &type);
   assert(type == FLOAT_TYPE);
   return c.float_data->data;
}

EMSCRIPTEN_KEEPALIVE
int term_type(word a)
{
   int type;
   if (TAGOF(a) == CONSTANT_TAG)
   {
      getConstant(a, &type);
      return type;
   }
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
   int t;
   cdata c = getConstant(a, &t);
   return (t == BLOB_TYPE && strcmp(type, c.blob_data->type) == 0);
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
   return getConstant(FUNCTOROF(a), NULL).functor_data->name;
}

EMSCRIPTEN_KEEPALIVE
int _term_functor_arity(word a)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return getConstant(FUNCTOROF(a), NULL).functor_data->arity;
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
void _make_choicepoint(word w)
{
   make_foreign_choicepoint(w);
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

word _make_vcompound(word functor, ...)
{
   va_list argp;
   va_start(argp, functor);
   return MAKE_VACOMPOUND(functor, argp);
}

word _make_vacompound(word functor, va_list argp)
{
   return MAKE_VACOMPOUND(functor, argp);
}


EMSCRIPTEN_KEEPALIVE
word _make_variable()
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

int _unify(word a, word b)
{
   return unify(a, b);
}

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

#ifdef EMSCRIPTEN
char* _portray_js_blob(char* type, void* ptr, Options* options, int precedence, int* len)
{
   return (char*)EM_ASM_INT({return portray_js_blob($0, $1, $2, $3, $4, $5)}, type, strlen(type), (int)ptr, options, precedence, len);
}

EMSCRIPTEN_KEEPALIVE
word _make_blob_from_index(char* type, int key)
{
   //printf("Making blob of type %s\n", type);
   word q = intern_blob(type, (void*)key, _portray_js_blob);
   //printf("Allocated blob %08x\n", q);
   return q;
}
#else
word _make_blob_from_index(char* type, int key)
{
   uintptr_t p = (uintptr_t)key;
   word q = intern_blob(type, (void*)p, NULL);
   return q;
}

#endif


EMSCRIPTEN_KEEPALIVE
int _release_blob(char* type, word w)
{
   Blob b = getConstant(w, NULL).blob_data;
   if (strcmp(type, b->type) != 0)
   {
      printf("Blob mismatch: Expecting %s but blob passed in is of type %s\n", type, b->type);
      assert(0);
   }
   int i = (int)b->ptr;
   delete_constant(w, BLOB_TYPE);
   return i;
}


EMSCRIPTEN_KEEPALIVE
int _get_blob(char* type, word w)
{
   Blob b = getConstant(w, NULL).blob_data;
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
void _consult_string_of_length(char* string, int len)
{
   consult_string_of_length(string, len);
}


EMSCRIPTEN_KEEPALIVE
int _exists_predicate(word module, word functor)
{
   Module m = find_module(module);
   if (m != NULL)
      return lookup_predicate(m, functor) != NULL;
   return 0;
}

#ifdef EMSCRIPTEN
void jscall(RC result)
{
   EM_ASM_({_jscallback($0)}, (result == SUCCESS || result == SUCCESS_WITH_CHOICES));
}

EMSCRIPTEN_KEEPALIVE
void executejs(word goal, int callback_ref)
{
   execute_query(goal, jscall);
}
#else

void _execute(word goal, void(*callback)(RC))
{
    PORTRAY(goal); printf("\n");
   execute_query(goal, callback);
}
#endif

EMSCRIPTEN_KEEPALIVE
void _format_term(Options* options, int priority, word term, char** ptr, int* length)
{
   StringBuilder formatted = stringBuilder();
   format_term(formatted, options, priority, term);
    PORTRAY(term); printf("\n");
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
word _string_to_local_term(char* string, int length)
{
   Stream stream = stringBufferStream(string, length);
   word w;
   if (read_term(stream, NULL, &w) == 0)
      return 0;
   if (TAGOF(w) == CONSTANT_TAG)
      return w;
   word* ptr;
   copy_local(w, &ptr);
   freeStream(stream);
   return (word)ptr;
}

EMSCRIPTEN_KEEPALIVE
word __copy_term(word w)
{
   return copy_term(w);
}

EMSCRIPTEN_KEEPALIVE
int is_list(word w)
{
   w = DEREF(w);
   return (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor);
}

EMSCRIPTEN_KEEPALIVE
word list_head(word w)
{
   return ARGOF(w, 0);
}

EMSCRIPTEN_KEEPALIVE
word list_tail(word w)
{
   return ARGOF(w, 1);
}

EMSCRIPTEN_KEEPALIVE
word is_empty_list(word w)
{
   return DEREF(w) == emptyListAtom;
}

EMSCRIPTEN_KEEPALIVE
word _deref(word w)
{
   return DEREF(w);
}

int _consult_file(const char* f)
{
   return consult_file(f);
}

int _is_variable(word a)
{
   a = DEREF(a);
   return (a & TAG_MASK) == VARIABLE_TAG;
}

int _is_atom(word a)
{
   a = DEREF(a);
   return (a & TAG_MASK) == CONSTANT_TAG && getConstantType(a) == ATOM_TYPE;
}

int _is_compound(word a)
{
   a = DEREF(a);
   return (a & TAG_MASK) == COMPOUND_TAG;
}

int _is_compound_with_functor(word a, word f)
{
   a = DEREF(a);
   return ((a & TAG_MASK) == COMPOUND_TAG) && (FUNCTOROF(a) == f);
}

word _make_local(word t)
{
   t = DEREF(t);
   if ((t & TAG_MASK) == CONSTANT_TAG)
      return t;
   word* w;
   copy_local(t, &w);
   return (word)w;
}

int _define_foreign_predicate(word moduleName, word functor, int(*func)(), int flags)
{
   Module module = find_module(moduleName);
   return define_foreign_predicate_c(module, functor, func, flags);
}
