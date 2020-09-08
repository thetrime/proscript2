#ifndef _LOCAL_H
#define _LOCAL_H

#include <stdlib.h>
#include <assert.h>
#include "kernel.h"
#include "ctable.h"

void free_local(word t);
word copy_local_with_extra_space(word t, word** local, int extra, int mark_constants);
word copy_local(word t, word** local);

#endif
