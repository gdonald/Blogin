---
title: DSL design
slug: dsl-design
order: 3
description: ORM::Factory's DSL needs to capture arbitrary attribute names inside a block without pre-declaring them. Raku's identifier resolution is lexical and...
toc: true
---
`ORM::Factory`'s DSL needs to capture arbitrary attribute names inside a
block without pre-declaring them. Raku's identifier resolution is lexical
and compile-time; arbitrary bare-identifier calls cannot resolve to
runtime-discovered names without either parser surgery or topic-driven
method dispatch. This page surveys the candidate mechanisms and records
the chosen design.

## Candidate mechanisms

### 1. Topic-`$_` builder + `FALLBACK`

The block is run with `$_` bound to a `FactoryBuilder` instance, so method
calls on the topic use the leading-dot shorthand. The builder defines
`FALLBACK` to capture any unknown method name as an attribute.

```raku
factory 'user', {
  .fname:  'Greg';
  .email:  { generate('email') };
  .admin;
};
```

- **Pros:** stays inside the standard language, dispatches at runtime,
  composes with `:`-positional sugar, lets us mix declared DSL methods
  (`.variant`, `.transient`, `.association`) with FALLBACK-captured
  attributes on the same object.
- **Cons:** the leading `.` is mandatory — `fname 'Greg'` would parse as a
  sub call, not a method, and Raku will not synthesize an unknown sub on
  demand.
- **Disambiguation:** a `Callable` arg means *dynamic*; everything else means
  *static*. `add-attribute` is the escape hatch for a literal `Callable`
  value or a name that collides with a DSL method.

### 2. Bare identifiers via exported sub stubs

Every attribute would have to be an exported sub. Since attributes are open
sets, this requires either:

- a build step generating a sub per attribute, or
- a fallback sub that uses `&?ROUTINE` introspection.

Neither preserves the *defined inside the block* property: `factory 'user'`
would need to know each user-defined attribute name at compile time.

- **Pros:** zero leading-dot noise (`fname 'Greg'`).
- **Cons:** does not work for runtime-defined attribute names; collides
  with built-in sub names (`given`, `when`, …); pollutes the global sub
  namespace. **Rejected.**

### 3. `&postcircumfix`

`postcircumfix:<{ }>` and similar are operator overloads on the *container*,
not a way to capture an identifier call. Attribute access would degenerate
into `<fname> = 'Greg'` syntax (subscript-on-builder), which loses the
attribute-as-statement shape.

- **Pros:** none meaningful here.
- **Cons:** does not solve the identifier problem. **Rejected.**

### 4. Custom slang (`use slang ...`)

Define a sub-grammar that re-parses the inside of `factory { ... }` to turn
bare identifiers into builder method calls.

- **Pros:** can drop the leading dot: `fname 'Greg'`.
- **Cons:** slang code is non-trivial, fragile across Rakudo releases, hard
  to debug (mis-tokenisation reports point into the slang), and bleeds into
  editor / static-analysis tooling. Every later piece of the library pays an
  ongoing tax. **Rejected as default; revisit only if the topic-`$_` shape
  proves unworkable in real specs.**

### 5. `EXPORTHOW` / metamodel injection

`EXPORTHOW` swaps the meta-class for a unit. It can give a class a custom
declarator (e.g. `factory user { ... }`), but does not affect what
identifiers inside the block resolve to.

- **Pros:** could produce factory declarations that look like class
  declarations.
- **Cons:** does not solve the attribute-capture problem; pushes definitions
  out of `define { ... }` into top-level declarators, which
  fights the per-suite registry-reset model. **Rejected.**

## Decision

**Topic-`$_` builder + `FALLBACK` (mechanism #1)** is the chosen DSL.

| Concern                       | Resolution                                                                    |
|-------------------------------|-------------------------------------------------------------------------------|
| Entry point                   | `define { ... }` runs the block with `$_` bound to a `DefinitionBuilder`. |
| `factory`, `sequence`, `variant`, `transient`, `association`, `after`, `before`, `initialize-with`, `to-create`, `modify`, `skip-create` | Explicit methods on the relevant builder — declared, not FALLBACK'd, so typos surface as method-not-found. |
| Attribute capture             | `FALLBACK` on `FactoryBuilder`, dispatching to `add-attribute` with the captured name and the call's arguments. |
| Static vs dynamic             | A single `Callable` positional ⇒ dynamic block; anything else ⇒ static value. The disambiguation is exhaustive because attribute calls take at most one positional. |
| Escape hatch                  | `add-attribute` for names that collide with declared DSL methods, or to force a `Callable` literal as a static value. |
| Variant application inside a factory | `.admin;` resolves through `FALLBACK`. A registered variant of the matching name wins over attribute capture; missing-name errors mention both possibilities. |

## Trade-off accepted

The user-visible cost is the leading `.` on every attribute, plus the `:`
between method and positional. Both are standard Raku — no slang, no
metamodel surgery. The win is that the DSL is *just methods on an object*,
which means it composes with Raku's existing tooling: introspection, `^methods`,
`does` for behaviour mix-ins, and ordinary stack traces on failure.
