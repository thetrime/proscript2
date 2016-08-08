#include "errors.h"

#define must_be_bound(a) ((TAGOF(a) != VARIABLE_TAG) || instantiation_error())
#define must_be_atom(a) (((TAGOF(a) != VARIABLE_TAG) || instantiation_error()) && ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == ATOM_TYPE) || type_error(atomAtom, a)))
#define must_be_atomic(a) ((TAGOF(a) == CONSTANT_TAG) || type_error(atomicAtom, a))
#define must_be_compound(a) ((TAGOF(a) == COMPOUND_TAG) || type_error(compoundAtom, a))
#define must_be_positive_integer(a) (must_be_integer(a) && ((getConstant(a).data.integer_data->data >= 0) || domain_error(notLessThanZeroAtom, a)))
#define must_be_character_code(a) (must_be_integer(a) && ((getConstant(a).data.integer_data->data >= 0 && getConstant(a).data.integer_data->data <= 65535) || representation_error(characterCodeAtom, a)))
#define must_be_integer(a) (((TAGOF(a) != VARIABLE_TAG) || instantiation_error()) && ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == INTEGER_TYPE) || type_error(integerAtom, a)))
#define must_be_character(a) ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == ATOM_TYPE && getConstant(a).data.atom_data->length == 1) || type_error(characterAtom, a))
#define must_be_predicate_indicator(a) (((TAGOF(a) == COMPOUND_TAG && FUNCTOROF(a) == predicateIndicatorFunctor) || type_error(predicateIndicatorAtom, a)) && (must_be_atom(ARGOF(a, 0)) && must_be_positive_integer(ARGOF(a, 1))))
