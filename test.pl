:- op( 20, xfx, <-- ).

run_all_tests:-
        catch(q,
              E,
              writeln(got(E))).

q:-
        X = undef_pred,
        call(X).
