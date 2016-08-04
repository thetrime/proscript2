run_all_tests:-
        catch(foo(A),
              B,
              writeln(caught(B))),
        writeln(after(A)).

qcatch(_,_,_).

foo(a).
foo(b):- throw(eggs).
foo(c).