---
title: Attribute types
slug: types
order: 9
description: On top of the adapter's column-type coercion, models can declare a per-attribute type — a small casting layer that converts values at three points:
toc: true
---
On top of the adapter's column-type coercion, models can declare a per-attribute
**type** — a small casting layer that converts values at three points:

| Hook          | Direction        | When                          |
| ------------- | ---------------- | ----------------------------- |
| `cast`        | user input → Raku | `build` / `assign-attributes` |
| `deserialize` | DB value → Raku   | reading a record from the DB  |
| `serialize`   | Raku → DB value   | saving a record               |

Declare types in the model's `submethod BUILD`, alongside associations and
scopes. Import the type system with `use ORM::ActiveRecord::Type;`.

## Custom attribute types

A type is any object that does the `AttributeType` role. Override the hooks you
need (each defaults to identity; `deserialize` defaults to `cast`):

```perl6
use ORM::ActiveRecord::Type;

# A Raku list <-> comma-separated text column.
class CsvType does AttributeType {
  method cast($v)        { $v ~~ Positional ?? $v.list !! ($v // '').split(',').list }
  method deserialize($v) { ($v // '').split(',').list }
  method serialize($v)   { $v ~~ Positional ?? $v.join(',') !! $v }
}

class Post is Model {
  submethod BUILD {
    self.attribute('tags', CsvType.new);     # `tags` is a text column
  }
}

my $p = Post.create({ tags => <ruby raku perl> });
Post.find($p.id).tags;       # ('ruby', 'raku', 'perl')
```

## Serialized columns

`serialize` stores a structured Raku value in a text column through a *coder*.
`JsonCoder` is built in; any object with `.dump` / `.load` works as a custom
coder:

```perl6
class User is Model {
  submethod BUILD {
    self.serialize('prefs', JsonCoder.new);   # `prefs` is a text column
  }
}

my $u = User.create({ prefs => { theme => 'dark', density => 'compact' } });
User.find($u.id).prefs<theme>;     # 'dark'
```

`YamlCoder` stores the value as YAML; import it from
`ORM::ActiveRecord::Type::Yaml`:

```perl6
use ORM::ActiveRecord::Type::Yaml;

self.serialize('prefs', YamlCoder.new);
```

A custom coder is any object with `.dump($value)` (Raku → string) and
`.load($string)` (string → Raku):

```perl6
class PipeCoder {
  method dump($v) { $v ~~ Positional ?? $v.join('|') !! $v }
  method load($s) { ($s // '').split('|').list }
}

self.serialize('tags', PipeCoder.new);
```

## Store accessors

`store` serializes a column and exposes named accessors that read and write
keys inside the stored hash. It defaults to `JsonCoder`; pass `coder` for
another:

```perl6
class Account is Model {
  submethod BUILD {
    self.store('prefs', accessors => ['theme', 'locale']);
  }
}

my $a = Account.create({ prefs => { theme => 'dark', locale => 'en' } });
$a.theme;            # 'dark'
$a.theme = 'light';  # writes prefs<theme>
$a.save;
```

`store-accessor` adds accessors to an already-serialized column after the fact:

```perl6
self.serialize('settings', YamlCoder.new);
self.store-accessor('settings', 'sound');
```

## Defaults

`attribute` takes a `default` — a plain value or a block evaluated per new
record. The default applies only when the attribute is not supplied:

```perl6
class Widget is Model {
  submethod BUILD {
    self.attribute('level', :default(5));
    self.attribute('token', :default(-> { SecureToken.generate }));
  }
}

Widget.build({}).level;            # 5
Widget.build({ level => 9 }).level # 9 — supplied value wins
```

## Overriding a column's type

When the attribute name matches a column, the declared type replaces the
adapter's default coercion for that column (the `CsvType` example above
overrides a text column). Everything else about the column is unchanged.

## Virtual attributes

An attribute whose name matches no column is **virtual**: it is read, written,
typed, and defaulted like any other attribute, but it is left out of every
`INSERT` and `UPDATE`. Use it for derived or transient state.

```perl6
class Widget is Model {
  submethod BUILD {
    self.attribute('score', 'integer', :default(10));   # no `score` column
  }
}

my $w = Widget.create({ name => 'a', score => 99 });
$w.score;                       # 99 (in memory, cast to Int)
Widget.find($w.id).score;       # 10 — not persisted, so it falls back to the default
```

## The type registry

Built-in types are registered under names (`integer`, `string`, `boolean`,
`float`, `decimal`, `datetime`). Register your own and reference it by name:

```perl6
Type.register('csv', CsvType.new);
Type.lookup('csv');                # the CsvType instance
Type.is-registered('integer');     # True

self.attribute('tags', 'csv');     # resolve a registered type by name
```
