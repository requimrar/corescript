# Partial Changelog


### 2018-12-09 `(1350296)`
- fix a refcounting bug where things were getting freed too early. the cause of this bug is when we transitioned to the lvalue system, around
	`24addf2`, when we added arguments to the refcounting stack; we forgot to include a corresponding increment in the call argument codegen, which
	has since been rectified.


---
### 2018-12-08 `(201a2ce)`
- allow `::` to refer to the topmost scope, like in C++
- allow `^` at any location in the scope path to refer to the parent scope -- eg. `::qux::^::foo::^::bar` (naturally will fail at the root scope!)
- for some reason, added a more verbose error message when you try to use a value as a type -- it searches up the tree for an actual type with the name,
	and tells you how to refer to it -- using the parent-scope specifier mentioned above.


---
### 2018-12-07 `(621aea2)`
- add location info to `pts` types -- which will soon need to be carried over to `fir` -- probably some kind of `TypeLocator` struct that I have in mind.
- abstract away which `hash_map` we use -- preliminary tests show `ska::flat_hash_map` gives us a ~9% perf improvement across-the-board -- using the
	`massive.flx` and associated test framework.


---
### 2018-12-06 `(aa33f64)`
- add a flag to call `abort()` on error -- now the default behaviour is to `exit(-1)`.


---
### 2018-12-06 `(f04726c)`
- fixed our `std::map` implementation a bit
- calls to `malloc` should go through our new malloc wrapper which checks for `null` returns
- hoist `make_lval` similar to `alloca` -- was causing stack overflow issues!
- apply type transforms (`poly/transforms.cpp`) in reverse, duh! surprised we didn't run into this sooner.
- fix an oversight where we allowed variables to be 'used' in nested functions (due to the scoping rules implicitly), and let it slip to llvm-land (!)


---
### 2018-12-05 `(3119634)`
- fixed a bug where we failed to infer union types -- was just checking the wrong thing.
- add a check for unwrapping (with `as`) union values to variants with no values (eg. `opt::none`)
- fixed a bug where we were not checking for duplicates when adding unresolved generic defns and operator overloads.


---
### 2018-12-04 `(2dbb858)`
- fix omission of floating-point comparison ops (oops)
- further patches to merge-block-elision
- fix ir-block error when doing multiple logical operators (`&&` and `||`)
- `is` check parses its rhs-operand the same way as `as` now (aka properly!)
- add the concept of `realScope`, which is the original scope of a definition -- aka where it got defined. we need this idea because we can call `ast::typecheck()` from *any* scope, usually when instantiating generics
- clean up string output to use `::` for scopes instead of `.`
- change the criteria of finding an existing definition (for generic things) to only match the last N items of the current
	state with the stored state -- instead of the previous vice-versa situation. (ie. the current state can be smaller than the stored state)
- add a distance penalty for resolving a generic function, also add a check where we don't re-add a function twice (eg. when recursively calling a
	generic function)
- `ast::Block`s now push an anonymous scope -- no idea why they didn't. fixes a pretty massive bug where bodies of an if statement shared the same scope
- probably some other misc fixes i forgot


---
### 2018-12-02 `(05b3953)`
- fix a using bug (`d1a0efe`)
- clean up the generated IR blocks, and implement 'merge-block-elision' properly (`76481a2`)
- fix implicit method calls (that i forgot to fix after refactoring the self-handling previously)


---
### 2018-12-01 `(076e176)`
- allow `export`-ing a path, eg. `std::limits`. this is different from namespacing the entire file (unlike C++), because we would then
	get `limits::std::...` instead
- allow `import`-ing a path as well, so we can do `import std::limits as foo::bar::qux`.
- `import`s of string paths (eg. `import "foo/bar"`) no longer append an `.flx` extension automatically!


---
### 2018-11-06 `(59f8469)`
- flip the declaration of `ffi-as` so the external name is after `as` -- and now as a string literal (because your external names might have
	flax-illegal characters inside)


---
### 2018-11-06 `(a2721fc)`
- declare foreign functions `as` something -- useful for OpenGL especially, (1) to deduplicate the `gl` prefix, and (2) to allow overloading of all
	the different `glVertex<N><T>` where `N = 2, 3, 4`, `T = i, f, d` or something.


---
### 2018-11-06 `(e2f235b)`
- `public` and `private` imports, where the former re-exports things and the latter is the default behaviour.


---
### 2018-11-05 `(494a01e)`
- static access now uses `::` instead of `.`, along the veins of C++ and Rust.
- polymorph instantiations now take the form of `Foo!<...>` (the exclamation mark) -- making it less likely to be ambiguous.
- polymorph arguments can now be positional, meaning `Foo!<int>` instead of `Foo!<T: int>`. The rules are similar to that of funtion calls
	-- no positional arguments after named arguments.


---
### 2018-10-28 `(0b937a5)`
- add chained comparison operators, eg. `10 < x < 30` would *succinctly* check for `x` between 10 and 30.
- consequently, changed the precedence for all six comparison operators (`==`, `!=`, `<`, `<=`, `>`, and `>=`) to be the same (500)


---
### 2018-10-28 `(f7fd4e6)`
- using unions -- including both generic and instantiated unions
- vastly improved (i'd say) inference for variants of unions (when accessing them implicitly)


---
### 2018-10-27 `(e365997)`
- infer polymorphs with union variants (except singleton variants)
- clean up some of the `TypecheckState` god object
- fixed a number of related bugs regarding namespaced generics
- give polymorphic types a `TypeParamMap_t` if there was a `type_infer`


---
### 2018-10-05 `(9de42cb)`
- major revamp of the generic solver -- the algorithm is mostly unchanged, but the interface and stuff is reworked.
- finally implemented the iterative solver
- variadic generics work
- took out tuple splatting for now.


---
### 2018-07-31 `(efc961e)`
- generic unions, but they're quite verbose.


---
### 2018-07-29 `(e475e99)`
- add tagged union types, including support for `is` and `as` to check and unwrap respectively
- infer type parameters for a type from a constructor call


---
### 2018-07-23 `(c89c809)`
- fix the recursive instantiation of generic functions.


---
### 2018-07-23 `(ff2a45a)`
- no longer stack allocate for arguments
- new lvalue/rvalue system in the IRBuilder, to make our lives slightly easier; we no longer need to pass a value/pointer pair,
	and we handle the thing in the translator -- not very complicated.
- add `$` as an alias for `.length` when in a subscript or slicing context, similar to how D does it.
- we currently have a bug where we cannot recursively instantiate a generic function.


---
### 2018-07-16 `(fdc65d7)`
- factor the any-making/getting stuff into functions for less IR mess.
- fix a massive bug in `alloc` where we never called the user-code, or set the length.
- add stack-allocs for arguments so we can take their address. might be in poor taste, we'll see.
- enable more tests in `tester.flx`.


---
### 2018-07-15 `(70d78b4)`
- fix string-literal-backslash bug.
- clean up some of the old cruft lying around.
- we can just make our own function called `char` that takes a slice and returns the first character. no compile-time guarantees,
	but until we figure out something better it's better than nothing.


---
### 2018-07-15 `(0e53fce)`
- fix the bug where we were dealing incorrectly with non-generic types nested inside generic types.
- fix calling variadic functions with no variadic args (by inserting an empty thing varslice in the generated arg list)
- fix 0-cost casting between signed/unsigned ints causing overload failure.


---
### 2018-07-15 `(7e8322d)`
- add `is` to streamline checking the `typeid` of `any` types.


---
### 2018-06-10 `(d1284a9)`
- fix a bug wrt. scopes; refcount decrementing now happens at every block scope (not just loops) -- added a new `BlockPoint` thing to convey this.
- fix a massive bug where `else-if` cases were never even being evaluated.
- `stdio.flx`!


---
### 2018-06-10 `(1aade5f)`
- replace the individual IR stuff for strings and dynamic arrays with their SAA equivalents.
- add an `any` type -- kinda works like pre-rewrite. contains 32-byte internal buffer to store SAA types without additional heap allocations,
	types larger than 32-bytes get heap-allocated.
- add `typeid()` to deal with the `any` types.


---
### 2018-06-10 `(8ab9dad)`
- finally add the check for accessing `refcount` when the pointer is `null`; now we return `0` instead of crashing.


---
### 2018-06-10 `(8d9c0d2)`
- fix a bug where we added generic versions for operators twice. (this caused an assertion to fire)


---
### 2018-06-10 `(aa8f9a9)`
- fixed a bug wrt. passing generic stuff to generic functions -- now we check only for a subset of the generic stack instead of the whole thing
	when checking for existing stuff.


---
### 2018-06-08 `(e530c81)`
- remove mpfr from our code


---
### 2018-06-03 `(ef6326c)`
- actually fix the bug. we were previously *moving out* of arrays in a destructuring binding, which is definitely not what we want.
	now we increment the refcount before passing it off to the binding, so we don't prematurely free.


---
### 2018-06-03 `(4d0aa96)`
- fix a massive design flaw wrt. arrays -- they now no longer modify the refcount of their elements. only when they are completely freed,
	then we run a decrement loop (once!) over the elements.
- this should fix the linux crash as well. note: classes test fails, so we've turned that off for now.


---
### 2018-06-03 `(c060a50)`
- fix a recursion bug with checking for placeholders
- fix a bug where we would try to do implicit field access on non-field stuff -- set a flag during typechecking so we know.
- add more tests.


---
### 2018-06-02 `(a86608b)`
- fix a parsing bug involving closing braces and namespaces.


---
### 2018-06-02 `(e35c883)`
- still no iterative solver, but made the error output slightly better.
- also, if we're assigning the result to something with a concrete type, allow the inference thing to work with that
	information as well.


---
### 2018-05-31 `(dbc7cd2)`
- pretty much complete implementation of the generic type solver for arbitrary levels of nesting.
- one final detail is the lack of the iterative solver; that's trivial though and i'll do that on the weekend.


---
### 2018-05-28 `(1e41f88)`
- fix a couple of bugs relating to SAA types and their refouncting pointers
- fix a thing where single-expr functions weren't handling `doBlockEndThings` properly.
- start drilling holes for the generic inference stuff


---
### 2018-05-27 `(337a6a5)`
- eliminate `exitless_error`, and made everything that used to use it use our new error system.


---
### 2018-05-27 `(e479ba2)`
- overhaul the error output system for non-trivial cases (it's awesome), remove `HighlightOptions` because we now have `SpanError`.
- make osx travis use `tester.flx` so we stop having the CI say failed.


---
### 2018-05-27 `(6385652)`
- sort the candidates by line number, and don't print too many of the fake margin/gutter things.
- change typecache (thanks adrian) to be better.


---
### 2018-05-27 `(ce9f113)`
- magnificently beautiful and informative errors for function call overload failure.


---
### 2018-05-18 `(80d5297)`
- fix a couple of unrelated bugs
- varidic arrays are now slice-based instead of dynarray-based
- variadic functions work, mostly. not thoroughly tested (nothing ever is)


---
### 2018-05-03 `(65b25b3)`
- parse the variadic type as `[T: ...]`
- allow `[as T: x, y, ... z]` syntax for specifying explicitly that the element type should be `T`.


---
### 2018-05-03 `(25dadf0)`
- remove `char` type; everything is now `i8`, and `isCharType()` just checks if its an `i8`
- constructor syntax for builtin types, and strings from slices and/or ptr+data
- fix a couple of mutability bugs here and there with the new gluecode.


---
### 2018-05-01 `(f3a06c3)`
- actually add the instructions and stuff, fix a couple of bugs
- `fir::ConstantString` is now actually a slice.
- move as many of the dynamic array stuff to the new `SAA` functions as possible.
- increase length when appending


---
### 2018-05-01 `(0ceb391)`
- strings now behave like dynamic arrays
- new concept of `SAA`, string-array-analogues (for now just strings and arrays lol) that have the `{ ptr, len, cap, refptr }` pattern.


---
### 2018-04-30 `(312b94a)`
- check generic things when looking for duplicate definitions (to the best of our abilities)
- actually make generic constructors work properly instead of by random chance


---
### 2018-04-30 `(1734444)`
- generic functions work
- made error reporting slightly better, though now it becames a little messy.


---
### 2018-04-20 `(c911408)`
- actually make generic types work, because we never tested them properly last time.
- fixed a bug in `pts::NamedType` that didn't take the generic mapping into account -- also fixed related issue in the parser


---
### 2018-04-20 `(860b61e)`
- move to a `TCResult` thing for typechecking returns, cleans up generic types a bunch
- fix a bug where we couldn't define generic types inside of a scope (eg. in a function)


---
### 2018-04-14 `(1b85906)`
- make init methods always mutable, for obvious reasons. Also, virtual stuff still works and didn't break, which is a plus.


---
### 2018-04-14 `(7107d5e)`
- remove all code with side effects (mostly `eat()` stuff in the parser) from within asserts.


---
### 2018-04-14 `(e9ebbb0)`
- fix type-printing for the new array syntax (finally)
- string literals now have a type of `[char:]`, with the appropriate implicit casts in place from `string` and to `&i8`
- add implicit casting for tuple types if their elements can also be implicitly casted (trivial)
- fix an issue re: constant slices in the llvm translator backend (everything was null)
- distinguish appending and construct-from-two-ing for strings in the runtime-glue-code; will probably use `realloc` to implement mutating via append for
	strings once we get the shitty refcount madness sorted out.


---
### 2018-04-09 `(81a0eb7)`
- add `[mut T:]` syntax for specifying mutable slices; otherwise they will be immutable. If using type inference, then they'll be inferred depending on
	what is sliced.


---
### 2018-04-08 `(f824a97)`
- overhaul the mutability system to be similar to Rust; now, pointers can indicate whether the memory they point to is mutable, and `let` vs `var` only
	determines whether the variable itself can be modified. Use `&mut T` for the new thing.
- allow `mut` to be used with methods; if it is present, then a mutable `self` is passed in, otherwise an immutable `self` is passed.
- add casting to `mut`: `foo as mut` or `foo as !mut` to cast an immutable pointer/slice to a mutable one, and vice versa.
- distinguish mutable and non-mutable slices, some rules about which things become mutable when sliced and which don't.


---
### 2018-04-07 `(ec9adb2)`
- add generic types for structs -- presumably works for classes and stuff as well.
- fix bug where we couldn't do methods in structs.
- fix bug where we treated variable declarations inside method bodies as field declarations
- fix bug where we were infinite-looping on method/field stuff on structs


---
### 2018-03-05 `(d9133a8)`
- change type syntax to be `[T]` for dynamic arrays, `[T:]` for slices, and `[T: N]` for fixed arrays
- change strings to return `[char:]` instead of making a copy of the string. This allows mutation... at your own risk (for literal strings??)
- add `str` as an alias for the aforementioned `[char:]`


---
### 2018-03-04 `(b48e10f)`
- change `alloc` syntax to be like this: `alloc TYPE (ARGS...) [N, M, ...] { BODY }`, where, importantly, `BODY` is code that will be run on each element in
	the allocated array, with bindings `it` (mutable, for obvious reasons), and `i` (immutable, again obviously) representing the current element and the index
	respectively.
- unfortunately what we said about how `&T[]` parses was completely wrong; it parses as `&(T[])` instead.


---
### 2018-02-27 `(b89aa2c)`
- fix how we did refcounts for arrays; instead of being 8 bytes behind the data pointer like we were doing for strings, they're now just stored in a separate
	pointer in the dynamic array struct itself. added code in appropriate places to detect null-ness of this pointer, as well as allocating + freeing it
	appropriately.
- add for loops with tuple destructuring (in theory arbitrary destructuring, since we use the existing framework for such things).
- add iteration count binding for for-loops; `for (a, b), it in foo { it == 1, 2, ... }`


---
### 2018-02-19 `(3eb36eb)`
- fix the completely uncaught disaster of mismatched comparison ops in binary arithmetic
- fix bug where we were double-offsetting the indices in insertvalue and extractvalue (for classes)
- fix certain cases in codegen where our `TypeDefn` wasn't code-generated yet, leading to a whole host of failures. // ! STILL NOT ROBUST
- fix typo in operator for division, causing it not to work properly (typoed `-` instead of `/`)
- change pointer syntax to use `&T` vs `T*`. fyi, `&T[]` parses intuitively as `(&T)[]`; use `&(T[])` to get `T[]*` of old
- change syntax of `alloc` (some time ago actually) to allow passing arguments to constructors; new syntax is `alloc(T, arg1, arg2, ...) [N1, N2, ...]`


---
### 2018-02-19 `(6dc5ed5)`
- fix the behaviour of foreach loops such that they don't unnecessarily make values (and in the case of classes, call the constructor) for the loop variable


---
### 2018-02-19 `(f7568e9)`
- add dynamic dispatch for virtual methods (WOO)
- probably fix some low-key bugs somewhere


---
### 2018-02-19 `(7268a2c)`
- enforce calling superclass constructor (via `init(...) : super(...)`) in class constructor definitions
- fix semantics, by calling superclass inline-init function in derived-class inline-init function
- refactor code internally to pull stuff out more.


---
### 2018-02-17 `(ba4de52)`
- re-worked method detection (whether we're in a method or a normal function) to handle the edge case of nested function defs (specifically in a method)
- make base-class declarations visible in derived classes, including via implicit-self
- method hiding detection -- it is illegal to have a method in a derived class with the same signature as a method in the base class (without virtual)


---
### 2018-02-15 `(e885c8f)`
- fix regression wrt. scoping and telporting in dot-ops (ref `rants.md` dated 30/11/17)


---
### 2018-02-11 `(c94a6c1)`
- add `using ENUM as X`, where `X` can also be `_`.


---
### 2018-02-11 `(8123b13)`
- add `using X as _` that works like import -- it copies the entities in `X` to the current scope.


---
### 2018-02-11 `(1830146)`
- add `using X as Y` (but where `Y` currently cannot be `_`, and `X` must be a namespace of some kind)


---
### 2018-02-10 `(23b51a5)`
- fix edge cases in dot operator, where `Some_Namespace.1234` would just typecheck 'correctly' and return `1234` as the value; we now report an error.


---
### 2018-01-28 `(00586be)`
- add barebones inheritance on classes. Barebones-ness is explained in `rants.md`


---
### 2018-01-21 `(f7a72b6)`
- fix variable decompositions
- enable the decomposition test we had.
- disable searching beyond the current scope when resolving definitions, if we already found at least one thing here. Previous behaviour was wrong, and
	screwed up shadowing things (would complain about ambiguous references, since we searched further up than we should've)


---
### 2018-01-21 `(1be1271)`
- fix emitting `bool` in IR that was never caught because we never had a function taking `bool` args, thus we never mangled it.
- add class constructors; all arguments must be named, and can only call declared init functions.


---
### 2018-01-20 `(c6a204a)`
- add order-independent type usage, a-la functions. This allows `A` to refer to `B` and `A` to simultaneously refer to `B`.
- fix detection (rather add it) of recursive definitions, eg. `struct A { var x: A }`, or `struct A { var x: B }; struct B { var x: A }`
- add `sizeof` operator


---
### 2018-01-19 `(b7fb307)`
- add static fields in classes, with working initialisers


---
### 2018-01-14 `(81faedb)`
- add error backtrace, but at a great cost...


---
### 2018-01-14 `(b4dabf6)`
- add splatting for single values, to fill up single-level tuple destructures, eg. `let (a, b) = ...10; a == b == 10`


---
### 2018-01-14 `(597b1f2)`
- add array and tuple decomposition, and allow them to nest to arbitrarily ridiculous levels.


---
### 2018-01-14 `(f8d983c)`
- allow assignment to tuples containing lvalues, to enable the tuple-swap idiom eg. `(a, b) = (b, a)`


---
### 2018-01-13 `(e91b4a2)`
- add splatting of tuples in function calls; can have multiple tuples


---
### 2018-01-12 `(9e3356d)`
- improve robustness by making range literals `(X...Y)` parse as binary operators instead of hacky postfix unary ops.


---
### 2018-01-07`(7cb117f)`
- fix a bug that prevented parsing of function types taking 0 parameters, ie. `() -> T`


---
### 2018-01-07`(4eaae34)`
- fix custom unary operators for certain cases (`@` was not being done properly, amongst other things)


---
### 2018-01-07 `(d06e235)`
- add named arguments for all function calls, including methods, but excluding fn-pointer calls


---
### 2018-01-07 `(d0f8c93)`
- fix some bugs re: the previous commit.


---
### 2018-01-06 `(ec728cd)`
- add constructor syntax for types (as previously discussed), where you can do some fields with names, or all fields positionally.


---
### 2018-01-06 `(ec7b2f3)`
- fix false assertion (assert index > 0) for InsertValue by index in FIR. Index can obviously be 0 for the first element.


---
### 2017-12-31 `(3add15b)`
- fix implicit casting arguments to overloaded operators
- add ability to overload unicode symbols as operators


---
### 2017-12-31 `(d2f8dbd)`
- add ability to overload prefix operators
- internally, move Operator to be a string type to make our lives easier with overloading.


---
### 2017-12-31 `(dcc28ba)`
- fix member access on structs that were passed as arguments (ie. 'did not have pointer' -- solved by using ExtractValue in such cases)
- fix method calling (same thing as above) -- but this time we need to use ImmutAlloc, because we need a `this` pointer
- add basic operator overloading for binary, non-assigment operators.


---
### 2017-12-29 `(45e818e)`
- check for, and error on, duplicate module imports.


---
### 2017-12-17 `(5a9aa9e)`
- add deferred statements and blocks


---
### 2017-12-16 `(80a6619)`
- add single-expression functions with `fn foo(a: T) -> T => a * 2`


---
### 2017-12-10 `(3b438c2)`
- add support for reverse ranges (negative steps, start > end)


---
### 2017-12-10 `(f3f8dbb)`
- add ranges
- add foreach loops on ranges, arrays, and strings
- add '=>' syntax for single-statement blocks, eg. `if x == 0 => x += 4`


---
### 2017-12-08 `(dacc809)`
- fix lexing/parsing of negative numerical literals


---
### 2017-12-07 `(ca3ae4b)`
- better type inference/support for empty array literals
- implicit conversion from pointers to booleans for null checking


---
### 2017-12-03 `(5be4db1)`
- fix runtime glue code for arrays wrt. reference counting


---
### 2017-12-02 `(e3a2b55)`
- add alloc and dealloc
- add dynamic array operators (`pop()`, `back()`)


---
### 2017-11-19 `(b7c6f74)`
- add enums


---
### 2017-10-22 `(408260c)`
- add classes


