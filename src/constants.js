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
                  streamOptionAtom: new AtomTerm("stream_option"),
                  ioModeAtom: new AtomTerm("io_mode"),

                  addFunctor: new Functor(new AtomTerm("+"), 2),
                  subtractFunctor: new Functor(new AtomTerm("-"), 2),
                  multiplyFunctor: new Functor(new AtomTerm("*"), 2),
                  intDivFunctor: new Functor(new AtomTerm("//"), 2),
                  divisionFunctor: new Functor(new AtomTerm("/"), 2),
                  remainderFunctor: new Functor(new AtomTerm("rem"), 2),
                  moduloFunctor: new Functor(new AtomTerm("mod"), 2),
                  negateFunctor: new Functor(new AtomTerm("-"), 1),
                  absFunctor: new Functor(new AtomTerm("abs"), 1),
                  signFunctor: new Functor(new AtomTerm("sign"), 1),
                  floatIntegerPartFunctor: new Functor(new AtomTerm("float_integer_part"), 1),
                  floatFractionPartFunctor: new Functor(new AtomTerm("float_fraction_part"), 1),
                  floatFunctor: new Functor(new AtomTerm("float"), 1),
                  floorFunctor: new Functor(new AtomTerm("floor"), 1),
                  truncateFunctor: new Functor(new AtomTerm("truncate"), 1),
                  roundFunctor: new Functor(new AtomTerm("round"), 1),
                  ceilingFunctor: new Functor(new AtomTerm("ceiling"), 1),

                  predicateIndicatorFunctor: new Functor(new AtomTerm("/"), 2),
                  numberedVarFunctor: new Functor(new AtomTerm("$VAR"), 1),

                  curlyFunctor: new Functor(new AtomTerm("{}"), 1),
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
