var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');

module.exports = {emptyListAtom: new AtomTerm("[]"),
		  cutAtom: new AtomTerm("!"),

		  unifyFunctor: new Functor(new AtomTerm("="), 2),
		  clauseFunctor: new Functor(new AtomTerm(":-"), 2),
		  conjunctionFunctor: new Functor(new AtomTerm(","), 2),
		  disjunctionFunctor: new Functor(new AtomTerm(";"), 2),
		  localCutFunctor: new Functor(new AtomTerm("->"), 2),
		  listFunctor: new Functor(new AtomTerm("."), 2)};
