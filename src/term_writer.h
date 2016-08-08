#include "string_builder.h"

int write_term(Stream stream, word term, Options* options);
int format_term(StringBuilder sb, Options* options, int precedence, word term);
