run_agc_test:-
        foo(x),
        T is random(100),
        Low is random(1000000),
        High is Low + T,
        forall(between(Low, High, X),
               writeln(X)).

foo(X):-
        bar(x(X)).

bar(X):-
        baz(x(X)).

baz(X):-
        true.




% The idea is that we can 'lock' some atoms if we know they are used in compilation or local terms
% The problem is deciding when to unlock them - or rather, if we unlock them whether we can *also* free them.

% If, when we restore the state, we find a locked atom, we can mark it as not-on-the-heap. Then when we unlock, we can delete it.
% But what happens if we unlock it first? Is that even possible? That is, what if we compile a term with a constant (eg via assert)
% then retract it. The constant is now not needed, but it MAY still be referred to on the heap.

% Maybe.... If we create all atoms with count=1, then increment it further if it is used in a clause or local copy, and then
% decrement it when we restore a state, free a clause or local copy? If it gets to 0, we can free it?

% Not if it can appear *first* in a compiled or local term.... ? But is that even possible?


/*

kernel.c is the only place we intern anything. We therefore create *atoms* in the following places:
* From constants.c at boot. These should *never* be released.
* From fli.c in foreign code
* In foreign bultins, from foreign.c and foreign_impl.c
* In format.c when using format(atom(...)). (And 2 other places that really should use universal constants)
* In operators.c when defining operators (and when iterating through them, from foreign.c). We *may* not actually need to define op->functor.
* in stream.c when raising an error about a filename that doesnt exist. We could change fileStream to pass in a word, in fact - it is only called from foreign code where the data is already available as a word.
* in prolog_flag.c. We could probably refactor this in the same way - passing in a word instead of that char* it represents.
* Finally, in parser.c. This is really where all the terms actually get *made*.

In summary, there are really only 4 sources of constants:
1) universal constants from constants.c
2) Constants created in foreign code (and format(atom(...) - this can be fixed by having format() return the chars and foreign code write them to sink and free them ))
3) Constants created by the parser
4) Edge cases that could probably be cleaned up.

Next up: How can we get references to these constants?

A) Local terms come from make_local (only in foreign code) and copy_local (foreign, SET_EXCEPTION, very briefly in list_delete_first, record(recorda/erase) and module(assert/retract/add_clause)
B) constants go into clause->constants when compiled
C) Terms go into the global heap (and argument heap) and these may contain constants. They are created mainly in B_ATOM


So the pattern is (approximately):

When we execute B_ATOM, we acquire the constant
When we compile a clause, we acquire the constants in clause->constants
When we make a local term, we aqcuire all the constants in the term.

When we move H down, we can release all constants directly on the heap between H and the new value.
We can release all constants in a clause when the clause is deleted
We can release all constants in a local term when it is freed

*/