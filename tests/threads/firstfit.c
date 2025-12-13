#include "tests/threads/tests.h"
#include "threads/palloc.h"
#include <stdio.h>

void test_bestfit (void) 
{
  palloc_set_mode (PAL_FIRST_FIT);
}
