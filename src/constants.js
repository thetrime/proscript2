var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');

module.exports = {emptyListAtom: AtomTerm.get("[]"),
		  cutAtom: AtomTerm.get("!"),

		  clauseFunctor: Functor.get(AtomTerm.get(":-"), 2),
		  conjunctionFunctor: Functor.get(AtomTerm.get(","), 2),
		  disjunctionFunctor: Functor.get(AtomTerm.get(";"), 2),
		  localCutFunctor: Functor.get(AtomTerm.get("->"), 2),
		  listFunctor: Functor.get(AtomTerm.get("."), 2)};
