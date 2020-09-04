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

