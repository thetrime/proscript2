#ifndef _PROLOG_H
#define _PROLOG_H

typedef uintptr_t word;
struct choicepoint;
typedef struct choicepoint* Choicepoint;
struct options;
typedef struct options* Options;

typedef enum
{
   FAIL = 0,
   SUCCESS = 1,
   SUCCESS_WITH_CHOICES = 2,
   YIELD = 3,
   ERROR = 4,
   HALT,
   AGAIN
} RC;

void init_prolog();
int _exists_predicate(word module, word functor);
word _make_atom(const char* a, int l);
word _make_variable();
word _deref(word w);
Choicepoint _push_state();
void _restore_state(Choicepoint w);
word _string_to_local_term(const char* string, int length);
word _make_functor(word name, int arity);
word _make_blob_from_index(const char* name, int ptr);
word _make_vcompound(word functor, ...);
word _make_vacompound(word functor, va_list argp);
void _consult_string(const char* string);
void _consult_string_of_length(const char* string, int len);
int _consult_file(const char* filename);
void _execute(word goal, void(*callback)(RC));
int _is_variable(word w);
int _is_atom(word w);
int _is_compound(word w);
int _is_compound_with_functor(word w, word f);
word _term_arg(word w, int i);
const char* _atom_data(word a);
int _atom_length(word a);
int _is_blob(word a, const char* type);
int _get_blob(const char* type, word w);
word _make_local(word t);
word _get_exception(); // Returns (word)0 if no exception
void _format_term(Options* options, int priority, word term, char** ptr, int* length);
Options* _create_options();
void _set_option(Options* options, word key, word value);
void _free_options(Options* options);
void _define_foreign_predicate(word moduleName, word functor, int(*func)(), int flags);
int _unify(word a, word b);
int _set_exception(word error);
#endif
