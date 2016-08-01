fox(a,b).
fox(c, X/Y):-
        functor(boing(cat, dog), X, Y).
fox(c, x).
fox(c, A):-
        between(1, 3, A).