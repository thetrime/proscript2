var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');

module.exports = {emptyListAtom: new AtomTerm("[]"),
		  cutAtom: new AtomTerm("!"),
		  failAtom: new AtomTerm("fail"),
                  trueAtom: new AtomTerm("true"),
                  catchAtom: new AtomTerm("$catch"),
                  modifyAtom: new AtomTerm("modify"),
                  boundedAtom: new AtomTerm("bounded"),
                  unboundedAtom: new AtomTerm("unbounded"),
                  flagAtom: new AtomTerm("flag"),
                  flagValueAtom: new AtomTerm("flag_value"),
                  maxIntegerAtom: new AtomTerm("max_integer"),
                  minIntegerAtom: new AtomTerm("min_integer"),
                  integerRoundingFunctionAtom: new AtomTerm("integer_rounding_function"),
                  towardZeroAtom: new AtomTerm("toward_zero"),
                  onAtom: new AtomTerm("on"),
                  offAtom: new AtomTerm("off"),
                  maxArityAtom: new AtomTerm("max_arity"),


                  catchFunctor: new Functor(new AtomTerm("catch"), 3),
                  throwFunctor: new Functor(new AtomTerm("throw"), 1),
                  unifyFunctor: new Functor(new AtomTerm("="), 2),
                  notFunctor: new Functor(new AtomTerm("\\+"), 1),
                  notUnifiableFunctor: new Functor(new AtomTerm("\\="), 2),
                  unifyFunctor: new Functor(new AtomTerm("="), 2),
                  clauseFunctor: new Functor(new AtomTerm(":-"), 2),
		  conjunctionFunctor: new Functor(new AtomTerm(","), 2),
		  disjunctionFunctor: new Functor(new AtomTerm(";"), 2),
		  localCutFunctor: new Functor(new AtomTerm("->"), 2),
		  listFunctor: new Functor(new AtomTerm("."), 2)};
