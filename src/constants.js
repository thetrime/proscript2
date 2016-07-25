"use strict";
exports=module.exports;

var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');

module.exports = {emptyListAtom: AtomTerm.get("[]"),
                  curlyAtom: AtomTerm.get("{}"),
		  cutAtom: AtomTerm.get("!"),
		  failAtom: AtomTerm.get("fail"),
                  trueAtom: AtomTerm.get("true"),
                  atomAtom: AtomTerm.get("atom"),
                  atomicAtom: AtomTerm.get("atomic"),
                  catchAtom: AtomTerm.get("$catch"),
                  modifyAtom: AtomTerm.get("modify"),
                  characterAtom: AtomTerm.get("character"),
                  characterCodeAtom: AtomTerm.get("character_code"),
                  boundedAtom: AtomTerm.get("bounded"),
                  nonEmptyListAtom: AtomTerm.get("non_empty_list"),
                  compoundAtom: AtomTerm.get("compound"),
                  callableAtom: AtomTerm.get("callable"),
                  unboundedAtom: AtomTerm.get("unbounded"),
                  flagAtom: AtomTerm.get("flag"),
                  flagValueAtom: AtomTerm.get("flag_value"),
                  maxIntegerAtom: AtomTerm.get("max_integer"),
                  minIntegerAtom: AtomTerm.get("min_integer"),
                  integerRoundingFunctionAtom: AtomTerm.get("integer_rounding_function"),
                  towardZeroAtom: AtomTerm.get("toward_zero"),
                  onAtom: AtomTerm.get("on"),
                  offAtom: AtomTerm.get("off"),
                  maxArityAtom: AtomTerm.get("max_arity"),
                  streamOptionAtom: AtomTerm.get("stream_option"),
                  ioModeAtom: AtomTerm.get("io_mode"),
                  dbReferenceAtom: AtomTerm.get("db_reference"),
                  endOfFileAtom: AtomTerm.get("end_of_file"),
                  evaluableAtom: AtomTerm.get("evaluable"),
                  floatAtom: AtomTerm.get("float"),
                  numberAtom: AtomTerm.get("number"),
                  numericAtom: AtomTerm.get("numeric"), // CHECKME: Suspicious
                  integerAtom: AtomTerm.get("integer"),
                  keyAtom: AtomTerm.get("key"),
                  listAtom: AtomTerm.get("list"),
                  operatorAtom: AtomTerm.get("operator"),
                  positionAtom: AtomTerm.get("position"),
                  privateProcedureAtom: AtomTerm.get("private_procedure"),
                  operatorPriorityAtom: AtomTerm.get("operator_priority"),
                  operatorSpecifierAtom: AtomTerm.get("operator_specifier"),
                  notLessThanZeroAtom: AtomTerm.get("not_less_than_zero"),
                  predicateIndicatorAtom: AtomTerm.get("predicate_indicator"),
                  prologFlagAtom: AtomTerm.get("prolog_flag"),
                  staticProcedureAtom: AtomTerm.get("static_procedure"),
                  procedureAtom: AtomTerm.get("procedure"),
                  streamAtom: AtomTerm.get("stream"),
                  streamOptionAtom: AtomTerm.get("stream_option"),
                  writeAtom: AtomTerm.get("write"),
                  writeOptionAtom: AtomTerm.get("write_option"),
                  readOptionAtom: AtomTerm.get("read_option"),
                  illegalNumberAtom: AtomTerm.get("illegal_number"),
                  streamOrAliasAtom: AtomTerm.get("stream_or_alias"),
                  accessAtom: AtomTerm.get("access"),
                  metaArgumentSpecifierAtom: AtomTerm.get("meta_argument_specifier"),

                  atomFunctor: Functor.get(AtomTerm.get("atom"), 1),
                  undefinedPredicateFunctor: Functor.get(AtomTerm.get("$undefined_predicate"), 0),
                  metaPredicateFunctor: Functor.get(AtomTerm.get("meta_predicate"), 1),
                  multiFileFunctor: Functor.get(AtomTerm.get("multi_file"), 1),
                  dynamicFunctor: Functor.get(AtomTerm.get("dynamic"), 1),
                  discontiguousFunctor: Functor.get(AtomTerm.get("discontiguous"), 1),
                  initializationFunctor: Functor.get(AtomTerm.get("initialization"), 1),
                  systemErrorFunctor: Functor.get(AtomTerm.get("system_error"), 1),
                  pairFunctor: Functor.get(AtomTerm.get("-"), 2),
                  failFunctor: Functor.get(AtomTerm.get("fail"), 0),
                  crossModuleCallFunctor: Functor.get(AtomTerm.get(":"), 2),
                  addFunctor: Functor.get(AtomTerm.get("+"), 2),
                  rdivFunctor: Functor.get(AtomTerm.get("rdiv"), 2),
                  subtractFunctor: Functor.get(AtomTerm.get("-"), 2),
                  multiplyFunctor: Functor.get(AtomTerm.get("*"), 2),
                  exponentiationFunctor: Functor.get(AtomTerm.get("^"), 2),
                  intDivFunctor: Functor.get(AtomTerm.get("//"), 2),
                  divisionFunctor: Functor.get(AtomTerm.get("/"), 2),
                  remainderFunctor: Functor.get(AtomTerm.get("rem"), 2),
                  moduloFunctor: Functor.get(AtomTerm.get("mod"), 2),
                  negateFunctor: Functor.get(AtomTerm.get("-"), 1),
                  absFunctor: Functor.get(AtomTerm.get("abs"), 1),
                  signFunctor: Functor.get(AtomTerm.get("sign"), 1),
                  floatIntegerPartFunctor: Functor.get(AtomTerm.get("float_integer_part"), 1),
                  floatFractionPartFunctor: Functor.get(AtomTerm.get("float_fraction_part"), 1),
                  floatFunctor: Functor.get(AtomTerm.get("float"), 1),
                  floorFunctor: Functor.get(AtomTerm.get("floor"), 1),
                  truncateFunctor: Functor.get(AtomTerm.get("truncate"), 1),
                  roundFunctor: Functor.get(AtomTerm.get("round"), 1),
                  ceilingFunctor: Functor.get(AtomTerm.get("ceiling"), 1),
                  moduleFunctor: Functor.get(AtomTerm.get("module"), 2),
                  predicateIndicatorFunctor: Functor.get(AtomTerm.get("/"), 2),
                  numberedVarFunctor: Functor.get(AtomTerm.get("$VAR"), 1),
                  cleanupChoicepointFunctor: Functor.get(AtomTerm.get("$cleanup_choicepoint"), 2),
                  curlyFunctor: Functor.get(AtomTerm.get("{}"), 1),
                  catchFunctor: Functor.get(AtomTerm.get("catch"), 3),
                  caughtFunctor: Functor.get(AtomTerm.get("caught"), 0), // A frame which has already caught an exception. See b_throw_foreign
                  throwFunctor: Functor.get(AtomTerm.get("throw"), 1),
                  unifyFunctor: Functor.get(AtomTerm.get("="), 2),
                  notFunctor: Functor.get(AtomTerm.get("\\+"), 1),
                  notUnifiableFunctor: Functor.get(AtomTerm.get("\\="), 2),
                  directiveFunctor: Functor.get(AtomTerm.get(":-"), 1),
                  clauseFunctor: Functor.get(AtomTerm.get(":-"), 2),
		  conjunctionFunctor: Functor.get(AtomTerm.get(","), 2),
		  disjunctionFunctor: Functor.get(AtomTerm.get(";"), 2),
		  localCutFunctor: Functor.get(AtomTerm.get("->"), 2),
		  listFunctor: Functor.get(AtomTerm.get("."), 2)};
