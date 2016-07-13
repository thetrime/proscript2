% setof/3, bagof/3, findall/3 and findall/4 as implemented by Richard O'Keefe and David Warren.
% http://www.j-paine.org/prolog/tools/files/setof.pl

:-meta_predicate(findall(?, 0, -)).
:-meta_predicate(setof(?, ^, -)).
:-meta_predicate(bagof(?, ^, -)).

findall(Template, Generator, List) :-
	save_instances(-Template, Generator),
	list_instances([], List).

findall(Template, Generator, SoFar, List) :-
	save_instances(-Template, Generator),
	list_instances(SoFar, List).

setof(Template, Filter, Set) :-
        bagof(Template, Filter, Bag),
	sort(Bag, Set).

bagof(Template, Generator, Bag) :-
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
bagof(Template, Generator, Bag) :-
	save_instances(-Template, Generator),
	list_instances([], Bag),
	Bag \== [].

save_instances(_Template, Generator) :-
        var(Generator),
        throw(error(instantiation_error, _)).
save_instances(Template, _^Generator) :-
        !,
        save_instances(Template, Generator).
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

% free_variables/4 is written by Richard O'Keefe, and Jan Wielemaker, and is licensed in the public domain
free_variables(Term, Bound, VarList, [Term|VarList]) :-
	var(Term),
	term_is_free_of(Bound, Term),
	list_is_free_of(VarList, Term), !.
free_variables(Term, _Bound, VarList, VarList) :-
	var(Term), !.
free_variables(Term, Bound, OldList, NewList) :-
	explicit_binding(Term, Bound, NewTerm, NewBound), !,
	free_variables(NewTerm, NewBound, OldList, NewList).
free_variables(Term, Bound, OldList, NewList) :-
	functor(Term, _, N),
	free_variables(N, Term, Bound, OldList, NewList).

free_variables(0, _, _, VarList, VarList) :- !.
free_variables(N, Term, Bound, OldList, NewList) :-
	arg(N, Term, Argument),
	free_variables(Argument, Bound, OldList, MidList),
	M is N-1, !,
	free_variables(M, Term, Bound, MidList, NewList).


explicit_binding(\+ _Goal,	       Bound, fail,	Bound      ) :- !.
explicit_binding(not(_Goal),	       Bound, fail,	Bound	   ) :- !.
explicit_binding(Var^Goal,	       Bound, Goal,	Bound+Var) :- !.
explicit_binding(setof(Var,Goal,Set),  Bound, Goal-Set, Bound+Var) :- !.
explicit_binding(bagof(Var,Goal,Bag),  Bound, Goal-Bag, Bound+Var) :- !.

term_is_free_of(Term, Var) :-
	\+ var_in_term(Term, Var).

var_in_term(Term, Var) :-
	Var == Term, !.
var_in_term(Term, Var) :-
	compound(Term),
	arg(_, Term, Arg),
	var_in_term(Arg, Var), !.

list_is_free_of([Head|Tail], Var) :-
	Head \== Var, !,
	list_is_free_of(Tail, Var).
list_is_free_of([], _).









recorda(Key, Term):-
        recorda(Key, Term, _).
recordz(Key, Term):-
        recordz(Key, Term, _).
recorded(Key, Term):-
        recorded(Key, Term, _).

:-meta_predicate(call(0)).
call(X):- var(X), throw(error(instantiation_error, _)).
call(X):- X.

:-meta_predicate(once(0)).
once(X):- call(X), !.

:-meta_predicate(assert(:)).
:-meta_predicate(asserta(:)).
:-meta_predicate(assertz(:)).
assert(X):- assertz(X).

nl:- put_code(10).

writeln(X):-
        write(X), nl.

:-meta_predicate(catch(0,?,0)).
catch(_Goal,_Unifier,_Handler):-
        '$catch'.

:-meta_predicate(setup_call_cleanup(0,0,0)).
setup_call_cleanup(A,B,C):-
        setup_call_catcher_cleanup(A,B,_,C).

:-meta_predicate(setup_call_catcher_cleanup(0,0,0)).
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


% Directives
include(_).        % FIXME: implement
ensure_loaded(_).  % FIXME: implement

% These are not ISO, but everyone seems to expect them

append([], []).
append([A|C], B) :-
	append(A, D, B),
        append(C, D).

append([], A, A).
append([A|B], C, [A|D]) :-
	append(B, C, D).


length(List, Length) :-
        ( var(Length) ->
          length(List, 0, Length)
        ; Length >= 0,
          length1(List, Length)
        ).

length([], Length, Length).
length([_|L], N, Length) :-
        N1 is N+1,
        length(L, N1, Length).

length1([], 0) :- !.
length1([_|L], Length) :-
        N1 is Length-1,
        length1(L, N1).

member(X,[X|_]).
member(X,[Y|T]) :- member(X,T).