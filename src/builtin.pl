% setof/3, bagof/3, findall/3 and findall/4 as implemented by Richard O'Keefe and David Warren.
% http://www.j-paine.org/prolog/tools/files/setof.pl


findall(Template, Generator, List) :-
	save_instances(-Template, Generator),
	list_instances([], List).

findall(Template, Generator, SoFar, List) :-
	save_instances(-Template, Generator),
	list_instances(SoFar, List).

set_of(Template, Filter, Set) :-
	bag_of(Template, Filter, Bag),
	sort(Bag, Set).

bag_of(Template, Generator, Bag) :-
	free_variables(Generator, Template, [], Vars),
	Vars \== [],
	!,
	Key =.. [.|Vars],
	functor(Key, ., N),
	save_instances(Key-Template, Generator),
	list_instances(Key, N, [], OmniumGatherum),
	keysort(OmniumGatherum, Gamut), !,
	concordant_subset(Gamut, Key, Answer),
	Bag = Answer.
bag_of(Template, Generator, Bag) :-
	save_instances(-Template, Generator),
	list_instances([], Bag),
	Bag \== [].

save_instances(Template, Generator) :-
	recorda(., -, _),
	call(Generator),
	recorda(., Template, _),
	fail.
save_instances(_, _).


list_instances(SoFar, Total) :-
        recorded(., Term, Ref),
	erase(Ref), !,		%   must not backtrack
	list_instances(Term, SoFar, Total).

list_instances(-, SoFar, Total) :- !,
	Total = SoFar.		%   = delayed in case Total was bound
list_instances(-Template, SoFar, Total) :-
	list_instances([Template|SoFar], Total).

list_instances(Key, NVars, OldBag, NewBag) :-
        recorded(., Term, Ref),
	erase(Ref), !,		%  must not backtrack!
	list_instances(Term, Key, NVars, OldBag, NewBag).

list_instances(-, _, _, AnsBag, AnsBag) :- !.
list_instances(NewKey-Term, Key, NVars, OldBag, NewBag) :-
        replace_key_variables(NVars, Key, NewKey), !,
        list_instances(Key, NVars, [NewKey-Term|OldBag], NewBag).

replace_key_variables(0, _, _) :- !.
replace_key_variables(N, OldKey, NewKey) :-
	arg(N, NewKey, Arg),
	nonvar(Arg), !,
	M is N-1,
	replace_key_variables(M, OldKey, NewKey).
replace_key_variables(N, OldKey, NewKey) :-
	arg(N, OldKey, OldVar),
	arg(N, NewKey, OldVar),
	M is N-1,
	replace_key_variables(M, OldKey, NewKey).


concordant_subset([Key-Val|Rest], Clavis, Answer) :-
	concordant_subset(Rest, Key, List, More),
	concordant_subset(More, Key, [Val|List], Clavis, Answer).

concordant_subset([Key-Val|Rest], Clavis, [Val|List], More) :-
	Key == Clavis,
	!,
	concordant_subset(Rest, Clavis, List, More).
concordant_subset(More, _, [], More).

concordant_subset([],   Key, Subset, Key, Subset) :- !.
concordant_subset(_,    Key, Subset, Key, Subset).
concordant_subset(More, _,   _,   Clavis, Answer) :-
        concordant_subset(More, Clavis, Answer).

recorda(Key, Term):-
        recorda(Key, Term, _).
recordz(Key, Term):-
        recordz(Key, Term, _).
recorded(Key, Term):-
        recorded(Key, Term, _).

call(X):- X.
once(X):- X, !.

assert(X):- assertz(X).

nl:- put_code(10).

writeln(X):-
        write(X), nl.

catch(_Goal,_Unifier,_Handler):-
        '$catch'.

setup_call_cleanup(A,B,C):-
        setup_call_catcher_cleanup(A,B,_,C).

setup_call_catcher_cleanup(Setup, Call, Catcher, Cleanup):-
        Setup,
        catch('$call_catcher_cleanup'(Call, Catcher, Cleanup),
                Catcher,
                ( Cleanup, throw(Catcher))).

'$call_catcher_cleanup'(Call, Catcher, Cleanup):-
        '$choicepoint_depth'(P),
        Call,
        '$choicepoint_depth'(Q),
        ( P == Q->
            Catcher = exit,
            !,
            Cleanup
        ; '$cleanup_choicepoint'(Catcher, Cleanup)
        ).

'$call_catcher_cleanup'(_, fail, Cleanup):-
        Cleanup,
        !,
        fail.