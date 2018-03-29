Proscript
=========

An implementation of a VM and compiler for Prolog. Written with a view for transpilation from C to Javascript.
hashmap comes from https://github.com/petewarden/c_hashmap where it is stated "There are no restrictions on how you reuse this code.". The code is included in modified forms in bihashmap and whashmap.


Building
--------
Proscript can be built either as a native (C) interpreter or a Javascript library. To build as a Javascript library you must have emscripten installed and the appropriate variables set in your envrionment.

   * To build as a native executable (./proscript): make ARCH=c
   * To build as a javascript library (./proscript.js): make ARCH=js
      * You must also obtain and build gmpjs from https://github.com/thetrime/gmpjs. The build process expects to find this in ./node_modules/gmpjs
      * Alternatively, if you have npm installed, you can just run `npm install`. This *should* take care of the dependencies as well. Note that you still need emscripten set up in your shell.

Design
------
The compiler design was originally based on GNU Prolog for Java, since my most recent experience with Prolog had been with that package. However, the opcodes themselves were borrowed from SWI-Prolog, partly because I wanted to better understand the architecture. There are a couple of serious departures however:
1) There are no predicate supervisors. To execute a predicate, you just start executing the code of the first clause
2) There are very few optimisation opcodes. Arithmetic is not compiled, there is no list type, etc.
3) There are a couple of new opcodes introduced to deal with LCO. These are opcodes including the substring UNSAFE, and will be explained below. Logically, B_UNSAFEVAR behaves like B_VAR if LCO is disabled.

General Execution Model
-----------------------
A clause (or query) is made up of (up to) two parts: Head opcodes and body opcodes. Head opcodes attempt to identify whether we can execute a predicate by matching the argument stack to the arguments of the head. Then the body opcodes prepare arguments for calling subgoals. Unlike SWI-Prolog, where these arguments are written to the stack, the arguments in Proscript are written to a block of pseudo-registers called the 'argument stack', pointed to by the symbol ARGS. This design is a hybrid of SWI-Prolog and Warren's original design (as explained by Ait-Kaci in his book). In Warren's design, all arguments are passed in registers, and only /permanent/ variables (variables that occur in more than one subgoal) are stored in frames. This means that the frame can be progressively trimmed as it executes and permanent variables become free; however if the first N slots of the frame are permanent in use as the arguments for the frame, this becomes harder to do (though, not impossible).

The fact that these arguments are permanent, which LCO can overwrite them, means that some other changes are necessary. The TRY_ME_ELSE construct must copy the arguments to the choicepoint it creates, and applying that choicepoint must restore them to ARGS. Additionally, if we are to overwrite the current frame, extra care must be taken with some variables if they are used in the future. For example, in "foo(A):- bar(a(A))", the frame for foo/1 will have one variable ('A') configured, but the ultimate subgoal (bar/1) will trigger LCO and overwrite the frame. In this situation, A needs to be moved to the heap so that when B tries to access the argument of the structure a/1 passed in as ARGS[0], the value does not point to memory which has since been reused.

Memory
------
There are 3 memory areas, organised at increasing addresses:
1) The (Prolog) heap (aka Global Stack). This is pointed to by the symbol HEAP, and the symbol H refers to the next available cell on the heap. HTOP is the end of the heap.
2) The (Prolog) stack (aka Local Stack). This is pointed to by the symbol STACK, and the symbol SP refers to the next available address. STOP is the end of the stack.
3) The trail. This is pointed to by the symbol TRAIL, and the symbol TR refers to the next available cell. TRTOP is the end of the trail.

Constants
---------
Every object in Proscript is a 'word'. There are 4 base types, indicated by the tag (lowest 2 bits) of the word. A variable (tag 00) is essentially a pointer. If the pointer is to itself, the variable is unbound. Otherwise it is bound to what the value dereferences to. The direction of the pointer should always be from a higher address to a lower one, for safety. In particular, a variable on the heap should never point to a location on the stack. The second type (tag 01) is a general purpose pointer. The third type (tag 10) is a compound term - the rest of the word is a pointer to a functor, and the term is comprised of that functor and the following N cells (depending on the arity of the functor), which are the arguments of the term. The final type (tag 11) is a constant.

Constants are accessed via the interface in ctable.h. They're stored in an array CTable, which holds structs of type 'constant'. This struct contains a type field and a data union, which in turn stores the underlying data for the constant. Everything except integers are stored as pointers to other structs (Floats could conceivably be done like this too). Atoms are stored as a data/length pair, big numbers (MPZ and MPQ) are stored as pointers to their associated GMP structures, etc.

Each constant should only appear once in the table. To facilitate this, there is a structure called a bihashmap that can look up an entry based on a 3-part key, where the first part is a void* and the second an int, and the third a hashcode. It also uses a customizable comparison function (which differs for each constant type) to confirm whether a hash collision is indeed a match. (In fact we need two such comparison functions to enable us to rehash the data easily. The 3-part key is designed to minimize object churn - if we already have the atom in the table, it's a waste of effort to create a new Atom object, duplicate the atom data, pass it in to the hashtable for lookup, determine we already have it, then free all the structures we just created. This is especially true for very large atoms, since atom lookup is a frequent operation in the foreign interface, and it would make using constants prohibitively slow).

Blobs are not stored in the bihashmap, and are currently the only kind of constants that can ever be released. It's assumed that if you call MAKE_BLOB() that you know this is a new blob; creating two blobs for the same underlying object means that they won't unify, even though they point to the same thing.

Currently there is no constant garbage collection. Augmenting each constant with a reference count could help, but there are a LOT of places that we would have to make sure that we cleaned them up from (aside from the obvious places like predicate definitions and stack frames, there are also local copies of structures and foreign code)

Local Terms
-----------
Sometimes, to prevent backtracking from destroying a term, it's helpful to make a copy of the term /off/ the heap. This can be easily accomplished via copy_local(), which allocates sufficient memory on the (C) heap to store the term in a minimal representation. It's possible to exploit the pointer nature of variables to make it easy to clean these up too - see asserta() in module.c for an explanation of why this works:

```
word* w;
copy_local(myterm, &w);
// (word)w is now a copy of myterm and can be used however myterm would have been used
free(w);
```

Recursive Queries
-----------------
It's possible for a foreign predicate to initiate a query. In order for this to be safe, you can call push_state() to save the current execution state, and restore_state() to restore it. Note that this will undo anything which has happened in the meantime, so if you want the result of a binding for something, you will first need to make a copy.

Foreign language Interface (FLI)
--------------------------------
There are two FLIs in Proscript. C and Javascript (the latter is only available if transpiling with emscripten). You can install foreign C predicates using define_foreign_predicate_c(), indicating in the flags whether the predicate is deterministic or not. If nondeterministic, you must pass a function pointer taking the same number of (word) arguments as the functor you are defining a predicate for. If nondeterministic, there will be one extra word passed in at the end. This will be a pointer to whatever word was created when make_foreign_choicepoint() was called, or if this is the first call into the predicate, the argument will be (word)0. Note that it is not (yet) possible to get notification of when your predicate has been cut, so it is not a good idea to store a pointer to an allocated object in the backtrack pointer as you may not get a chance to clean it up.

There's an equivalent define_foreign_predicate_js to define foreign Javascript predicates, which are /always/ nondeterministic. The backtrack pointer is accessible as `this.backtrack`.






Known Bugs
----------
* If you assert two clauses of a fact, enter the predicate, abolish the fact (or retract the clauses) then backtrack, the system will likely crash. This could be fixed by changing the is_local attribute of clauses into a reference counting scheme; query clauses will start with a count of 0, predicate clauses with a count of 1 (since they're 'owned' by the predicate).
* The TRY_ME_OR_NEXT_CLAUSE is inefficient because it destroys the choicepoint and creates a new one, and this requires copying the ARGS array needlessly around. It would be much better to do a TRY_ME_ELSE, RETRY_ME_ELSE, TRUST_ME triple as per the original design. Care must be taken with dynamic predicates though, since you might end up at a TRUST_ME or a RETRY_ME_ELSE without a preceeding TRY_ME_ELSE if clauses are being added as they're executed.


Licensing Issues
----------------
It is possible to compile proscript for iOS, but there is a licensing problem. libgmp is distributed under GPLv2 and LGPLv3. It seems like it would not be possible to distribute code with such restrictions through the App store since it is not possible to relink an application and run it. It seems that running it locally, on your own device (using your own signing key) and possibly distribution via enterprise channels would be OK, but it's a complicated legal issue.

If necessary, libgmp could be replaced with a BSD-licensed library such as the OpenSSL MP library to avoid this restriction.
