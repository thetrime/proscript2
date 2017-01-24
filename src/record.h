#include "types.h"
word recorda(word key, word term);
word recordz(word key, word term);
int _erase(word ref);
void initialize_database();
int recorded(word ref, word* key, word* value);
List* find_records(word);
