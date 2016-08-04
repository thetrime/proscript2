#include "errors.h"

#define must_be_bound(a) ((TAGOF(a) != VARIABLE_TAG) || instantiation_error())
#define must_be_atom(a) ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == ATOM_TYPE) || type_error(atomAtom, a))
#define must_be_positive_integer(a) ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == INTEGER_TYPE && getConstant(a).data.integer_data->data >= 0) || domain_error(notLessThanZeroAtom, a))
#define must_be_integer(a) (TAGOF(a) == CONSTANT_TAG && getConstant(a).type == INTEGER_TYPE || type_error(integerAtom, a))
#define must_be_character(a) ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == ATOM_TYPE && getConstant(a).data.atom_data->length == 1) || type_error(characterAtom, a))
#define must_be_predicate_indicator(a) ((TAGOF(a) == COMPOUND_TAG && FUNCTOROF(a) == predicateIndicatorFunctor && TAGOF(ARGOF(a, 0)) == CONSTANT_TAG && getConstant(TAGOF(ARGOF(a, 0))).type == ATOM_TYPE && TAGOF(ARGOF(a, 1)) == CONSTANT_TAG && getConstant(TAGOF(ARGOF(a, 1))).type == INTEGER_TYPE) || type_error(predicateIndicatorAtom, a))
