---
title: Serialization
slug: serialization
order: 12
description: A small family of methods that answer "how do I identify this record?" and "how do I turn it into a hash/string?" — used for URL helpers, cache keys, JSON...
toc: true
---
A small family of methods that answer "how do I identify this record?" and
"how do I turn it into a hash/string?" — used for URL helpers, cache keys,
JSON APIs, and human-readable inspection.

## to-param

`to-param` returns the record's primary key as a `Str`, suitable for
embedding in URLs. New (unsaved) records return an undefined `Str`.

```perl6
my $user = User.create({fname => 'Greg'});
$user.to-param;          # '42'

User.new(:id(0)).to-param;
# undefined Str — record is not persisted yet
```

## to-key

`to-key` returns a list of values that identify the record — currently
`[id]` for persisted records, `Nil` for new ones. It returns a list so that
future composite-key support can extend it without breaking callers.

```perl6
$user.to-key;            # [42]
User.new(:id(0)).to-key; # Nil
```

## cache-key, cache-version, cache-key-with-version

These produce stable cache fingerprints **for a single record** (compare
with the relation-level versions in [Inspection helpers](/docs/ORM-ActiveRecord/inspection)).

```text
cache-key                 →  "users/42"
cache-version             →  "2026-05-13T14:23:11Z"   (record's updated_at)
cache-key-with-version    →  "users/42-2026-05-13T14:23:11Z"
```

For a new (unsaved) record, `cache-key` returns `"<table>/new"`. If the
model has no `updated_at` column, `cache-version` is undefined and
`cache-key-with-version` falls back to the bare `cache-key`.

```perl6
my $key = $user.cache-key-with-version;

# Use it in your own cache store; the key rolls forward on every save:
$cache.write($key, $user.serializable-hash);
```

`Model.cache-key` (called on the class, not an instance) routes to
`Model.all.cache-key` — the relation fingerprint. Instance-level keys require
a defined record.

## serializable-hash

Returns a `Hash` of the record's attributes, ready for JSON / YAML / any
other encoder. Options:

| Option     | Meaning                                                 |
| ---------- | ------------------------------------------------------- |
| `:only`    | Include only these attribute names                      |
| `:except`  | Exclude these attribute names                           |
| `:methods` | Also call these methods and include the results         |

```perl6
$user.serializable-hash;
# { id => 42, fname => 'Greg', lname => 'Donald',
#   created_at => DateTime.new(...), updated_at => DateTime.new(...) }

$user.serializable-hash(:only<fname lname>);
# { fname => 'Greg', lname => 'Donald' }

$user.serializable-hash(:except<created_at updated_at>);
# { id => 42, fname => 'Greg', lname => 'Donald' }

$user.serializable-hash(:only<fname>, :methods<fullname>);
# { fname => 'Greg', fullname => 'Greg Donald' }
```

`serializable-hash` does **not** apply `filter-attribute` redaction — it is
the data-export hook and treats every attribute as visible. Use `:except`
to drop sensitive fields when you build a payload.

## as-json

`as-json` is `serializable-hash` with one extra step: it walks the hash
and stringifies `DateTime` / `Date` values so they survive JSON encoding.
It accepts the same options as `serializable-hash`.

```perl6
my %payload = $user.as-json(:except<password>);
# { id => 42, fname => 'Greg', updated_at => '2026-05-13T14:23:11Z', ... }
```

## to-json

`to-json` is `as-json` piped through `JSON::Tiny::to-json`. Returns a JSON
string.

```perl6
say $user.to-json(:only<id fname lname>);
# '{ "id" : 42, "fname" : "Greg", "lname" : "Donald" }'
```

## attribute-for-inspect

Renders a single attribute the way `inspect` does — strings are quoted and
truncated to 50 characters, dates are quoted as ISO strings, booleans as
`True`/`False`, numbers bare. Filtered attributes show as `[FILTERED]`.

```perl6
$user.attribute-for-inspect('fname');     # '"Greg"'
$user.attribute-for-inspect('updated_at'); # '"2026-05-13T14:23:11Z"'
$user.attribute-for-inspect('password');   # '[FILTERED]'  (see below)
```

## inspect

`inspect` returns a single-line summary of the record:

```perl6
$user.inspect;
# '#<User id: 42, fname: "Greg", lname: "Donald", active: True>'
```

`gist` (the Raku stringification hook used by `say`) routes to `inspect`
for defined instances, so `say $user;` prints the same shape.

## filter-attribute

Names of attributes that should be redacted in `inspect` /
`attribute-for-inspect` output. Declare them in `BUILD`:

```perl6
class User is Model {
  submethod BUILD {
    self.filter-attribute('password', 'ssn');
  }
}

$user.inspect;
# '#<User id: 42, fname: "Greg", password: [FILTERED], ssn: [FILTERED], ...>'
```

Filtering only affects inspection output. `serializable-hash`, `as-json`,
and `to-json` continue to include the raw values — drop them explicitly
with `:except` when you build a payload you do not want to leak.
