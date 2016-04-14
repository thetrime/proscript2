var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');

module.exports = {emptyListAtom: AtomTerm.get("[]"),
		  clauseFunctor: Functor.get(AtomTerm.get(":-"), 2),
		  listFunctor: Functor.get(AtomTerm.get("."), 2)};
