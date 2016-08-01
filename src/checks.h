#define must_be_bound(a) ((TAGOF(a) != VARIABLE_TAG) || SET_EXCEPTION(instantiation_error()))
#define must_be_atom(a) ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == ATOM_TYPE) || SET_EXCEPTION(type_error(atomAtom, a)))
#define must_be_positive_integer(a) ((TAGOF(a) == CONSTANT_TAG && getConstant(a).type == INTEGER_TYPE && getConstant(a).data.integer_data->data >= 0) || SET_EXCEPTION(domain_error(notLessThanZeroAtom, a)))
#define must_be_integer(a) (TAGOF(a) == CONSTANT_TAG && getConstant(a).type == INTEGER_TYPE || SET_EXCEPTION(type_error(integerAtom, a)))
