var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');

module.exports = {emptyListAtom: new AtomTerm("[]"),
		  cutAtom: new AtomTerm("!"),
		  failAtom: new AtomTerm("fail"),
                  trueAtom: new AtomTerm("true"),
                  catchAtom: new AtomTerm("$catch"),
                  emptyListAtom: new AtomTerm("[]"),

                  catchFunctor: new Functor(new AtomTerm("catch"), 3),
                  throwFunctor: new Functor(new AtomTerm("throw"), 1),
                  unifyFunctor: new Functor(new AtomTerm("="), 2),
                  notFunctor: new Functor(new AtomTerm("\\+"), 1),
                  unifyFunctor: new Functor(new AtomTerm("="), 2),
                  clauseFunctor: new Functor(new AtomTerm(":-"), 2),
		  conjunctionFunctor: new Functor(new AtomTerm(","), 2),
		  disjunctionFunctor: new Functor(new AtomTerm(";"), 2),
		  localCutFunctor: new Functor(new AtomTerm("->"), 2),
		  listFunctor: new Functor(new AtomTerm("."), 2)};
