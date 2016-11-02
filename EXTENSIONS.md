Extensions
==========

While Proscript aims to implement ISO Prolog as far as possible, there are a number of notable exceptions:

Arithmetic Type Tolerance
-------------------------
By default, proscript will ignore many common arithmetic type errors where the type seems like it should be well-defined. These include:
   * The following float functions will accept an integer or biginteger, even though the ISO standard requires an error to be thrown:
      * float_integer_part/1
      * float_fractional_part/1
      * floor/1
      * truncate/1
      * round/1
      * ceil/1
   * A new function rdiv/2 is added. This will return a rational expression.
   * A new function **/2 is added which returns first argument raised to the power of the second

These can all be disabled by compiling with STRICT_ISO=yes


Multiprecision Arithmetic
-------------------------
By default proscript will support arbitrary precision arithmetic via either the system libgmp (for native builds) or gmpjs (for the Javascript library).
You can disable multiprecison arithmetic by editing src/arithmetic.c and changing the variable is_unbounded to be 0. In the future this should be made into a C define, but bear in mind that even if you change the value, gmpjs is still a dependency. (This should also be fixed, but for now it is quite tightly coupled).

Builtin Predicates
------------------
Several additional predicates are made available beyond what is required by ISO/IEC 13211-1:
   * member/2
   * memberchk/2
   * length/2
   * is_list/1
   * forall/2
   * setup_call_catcher_cleanup/4
   * setup_call_cleanup/3
   * writeln/1
   * upcase_atom/2
   * recorda/3
   * recordz/3
   * erase/1
   * recorded/3
   * between/3
   * sort/2
   * keysort/2
   * format/2
   * format/3 (including using atom(X) as the first argument instead of a stream)
   * atomic_list_concat/3
   * code_type/2
   * append/2
   * reverse/2

There are also several non-ISO exceptions which can be raised:
   * io_error/2
   * syntax_error/1
   * format_error/1


Modules
-------
Modules are implemented in a similar style to SWI modules, but with a few critical differences. The directive :-module(+Name, +Exports). begins a new module. Critically:
   * You can have this directive many times in a single file
   * You can (but probably shouldn't) redefine a module by using the declaration again with the same name

By default, code is located in module user. Once you have started a module, you cannot get 'back' to user without consulting a new file.

You cannot (currently) declare predicates in other modules using the module as a predicate to the predicate as in `other_module:predicate(a, b):- goal(c).`

During execution, predicates are resolved as follows:
   * If the predicate is defined in the current module, use that
   * Otherwise, if the current module is not user, look for the predicate in module(user).
   * Otherwise, resolution failure.

When compiling a module, the exports are used to create a number of 'shims' in module(user). Each of these shims is just the predicate head with a body that involves a special opcode to switch to the module being consulted and re-try resolution.

### Meta-Predicates
Meta-predicates cause a lot of problems with modules. Suppose you are in module(foo), where there are 3 clauses to some fact my_fact/1. If you were to execute `findall(X, call(my_fact(X)), Xs)`, we would resolve `call/1` in `module(user)` (where it is defined), and call/1 would then try to resolve `my_fact/1` in user, and fail to do so. Note that this would have worked fine if we had instead called `findall(X, call(foo:my_fact(X)), Xs)`.

To work around this, you can declare a predicate which takes an argument that will eventually be executed to be a 'meta-predicate' using the directive :-meta_predicate/1. This generates special instructions that automatically prefixes the argument with the calling context's module. SWI-Prolog supports using ^, 0-9, +, -, ? and : for meta-predicate arguments. Proscript accepts all of those as well, but:
   * 0-9 and : all mean the same thing: This is a meta-argument
   * ^ has the same meaning as SWI-Prolog: The argument may be ^/2 term, of which only the second argument is going to be executed
   * +, - and ? all mean the same thing: This is not a meta-argument.
