arun_all_tests:-
        ( foo(C) ; foo(C)).


foo(X):-
        writeln(X).


run_all_tests:-
        ( foo(C)
        ; true
        ),
        foo(C).