function evaluate_expression(a)
{
    throw new Error("arithmetic not implemented");
}

var type_names = ["int", "bigint", "float", "rational"];

function commonType(a, b)
{
    var type = 0;
    if (a instanceof BigIntegerTerm)
        type = 1;
    else if (a instanceof FloatTerm)
        type = 2;
    else if (a instanceof RationalTerm)
        type = 3;
    else if (!(a instanceof IntegerTerm))
        Errors.typeError(Constants.numericAtom, a);

    if (b instanceof BigIntegerTerm)
    {
        if (type < 1)
            type = 1;
    }
    else if (b instanceof FloatTerm)
    {
        if (type < 2)
            type = 2;
    }
    else if (b instanceof RationalTerm)
    {
        if (type < 3)
            type = 3;
    }
    else if (!(b instanceof IntegerTerm))
        Errors.typeError(Constants.numericAtom, b);
    return type_names[type];
}


// 8.6.1
module.exports.is = function(result, expr)
{
    return this.unify(result, evaluate_expression(expr));
}

// 8.7.1 (Not complete)
module.exports["=\="] = function(a, b)
{
    var ae = evaluate_expression(a);
    var be = evaluate_expression(b);
    var targettype = commonType(ae, be);
    switch (targettype)
    {
        case "int":
        {
            return ae.value != be.value;
        }
        case "bigint":
        {
            var bigints = toBigIntegers(ae, be);
            return !bigints[0].equals(bigints[1].value);
        }
        case "float":
        {
            var floats = toFloats(ae, be);
            return floats[0].value != floats[1].value;
        }
        case "rational":
        {
            var rationals = toRationals(ae, be);
            return !rationals[0].equals(rationals[1].value);
        }
    }
}

// FIXME: Also need to implement =:=, >, >=, =< and <
