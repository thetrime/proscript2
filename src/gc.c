// Note: This file is included at the end of kernel.c if GC is enabled
//       this saves having to expose a lot of details about the VM

/* This GC algorithm is based on Appleby et al, 1988.

   2 bits in each word are reserved for GC. Since we reserve 2 additional bits for type, this implies that in a 32-bit system the largest
   integer we can represent as a native type is 268435455, and on a 64-bit system, 1152921504606846975.

   The following invariants are expected:
   * There are no pointers from the heap to the stack
   * There are no pointers from the heap to the trail
   * There are no pointers from the stack to the trail

   Terms can be copied off the heap using make_local. This is of paramount concern when doing AGC, but is not a problem for heap compaction because
   terms which are made local have NO references to the heap, trail or stack anymore (the localisation is recursive).

   So this means that a very broad overview of compaction goes like this:
   * Sweep the stack, heap and trail looking for pointers. Mark things which are reachable
   * Remove any objects which are not reachable
   * Compact data
   * Adjust pointers in the stack, heap and trail so that they refer to the new structures on the heap

   There are a few key differences between Proscript and the design in Appleby:
   * We allow a variable to point to a STR cell
   * We have no LST cell type. The other kind of type is PTR, which behaves essentially like a CON
   * The stack is maintained in a completely different format to implement the stack-based (rather than register-based) calling semantics
      * But we still have ARGS as a global register

*/

#define GC_MASK (F_MASK | M_MASK)
#define IS_MARKED(w) (((*w) & M_MASK) != 0)
#define IS_FIRST(w) (((*w) & F_MASK) != 0)
#define SET_FIRST(w) (*w |= F_MASK)
#define UNSET_FIRST(w) (*w &= ~F_MASK)
#define SET_MARKED(w) (*w |= M_MASK)
int total_marked = 0;

#define VALPTR(w) (TAGOF(*w) == VARIABLE_TAG?((word*)*w):w)
#define swap3(a,b,c) {word tmp = *a; *a = *b; *b = *c; *c = tmp;}
#define REVERSE(current, next) swap3(VALPTR(next), current, next)
#define UNDO(current, next) swap3(VALPTR(current), next, current)
#define ADVANCE(current, next) swap3(VALPTR(current+1), next, VALPTR(current))
#define LAST(t) ((Word)(((Word*)(t & ~TAG_MASK))+1 + getConstant(FUNCTOROF(t), NULL).functor_data->arity))

void mark_variable(word* start)
{
   printf("Marking as reachable: %ld\n", *start);
   word* current = start;
   // If current is a variable, then next is points to the location that the variable points to (for unbound variables, next == current)
   // If current points to a structure, then next = current. In Appleby, structures are pointers to the first cell (functor), but we optimise that out
   // If current points to a constant, then next is undefined.
   word* next = VALPTR(start);
   SET_FIRST(current);
   total_marked--;
  forward:
   printf("Forward: Current = %ld, next = %ld\n", (*current & ~GC_MASK) , (*next & ~GC_MASK));
   if (IS_MARKED(current))
      goto backward;
   SET_MARKED(current);
   total_marked++;
   switch(TAGOF(*current))
   {
      case VARIABLE_TAG:
         if (IS_FIRST(next))
            goto backward;
         printf("Reversing...\n");
         REVERSE(current, next);
         goto forward;
      case COMPOUND_TAG:
         if (IS_FIRST(ARGPOF(*current)))
            goto backward;
         // for every arg in next, except the first, set f. NOT DONE!
         next = LAST(*current);
         REVERSE(current, next);
         goto forward;
      default: // Constants and pointers
         goto backward;
   }
  backward:
   printf("Backward: Current = %ld, next = %ld\n", (*current & ~GC_MASK) , (*next & ~GC_MASK));
   if (!IS_FIRST(current))
   {
      UNDO(current, next);
      goto backward;
   }
   UNSET_FIRST(current);
   if (current == start)
      return;
   current--;
   ADVANCE(current, next);
   goto forward;
}


int IS_HEAP(word w)
{
   return ((word*)(w & ~GC_MASK)) < HTOP;
}

void mark_arguments()
{
   int arity = getConstant(FR->functor, NULL).functor_data->arity;
   for (int i = arity-1; i > 0; i--)
   {
      if (IS_HEAP(ARGS[i]))
      {
         word tmp = ARGS[i];
         mark_variable(&tmp);
      }
   }
}

void mark_frames(Frame f)
{
   while (f != NULL)
   {
      for (int i = f->clause->slot_count-1; i > 0; i--)
      {
         if (IS_MARKED(&f->slots[i]))
            return;
         else if (IS_HEAP(f->slots[i]))
            mark_variable(&f->slots[i]);
      }
      f = f->parent;
   }
}

void mark_choicepoints()
{
   Choicepoint c = CP;
   while (c != NULL)
   {
      mark_frames(c->FR);
      for (int i = 0; i < c->argc; i++)
      {
         if (IS_HEAP(c->args[i]))
            mark_variable(&c->args[i]);
      }
      c = c->CP;
   }
}

void mark()
{
   total_marked = 0;
   mark_arguments();
   mark_frames(FR);
   mark_choicepoints();
}

void gc()
{
   mark();

}

