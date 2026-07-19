---
title: Options
slug: options
order: 29
description: Beyond the per-validator settings (min:, max:, with:, etc.) every validator accepts a small set of shared options that change *when* the validator runs or...
toc: true
---
Beyond the per-validator settings (`min:`, `max:`, `with:`, etc.) every validator accepts a small set of shared options that change *when* the validator runs or *how* it reports failure.

## `allow-nil` / `allow_nil`

Skips every validator on the declaration when the attribute's value is undefined (`Nil`). The validator is silently bypassed; nothing is appended to `errors`.

```perl6
use ORM::ActiveRecord::Model;

class Event is Model {
  submethod BUILD {
    self.validate: 'score',
      { :presence, numericality => { gt => 0 }, allow-nil => True }
  }
}

my $e = Event.build({name => 'A'});
$e.write-attribute('score', Nil);
say $e.is-valid;
```

Output

```shell
True
```

Both `allow-nil` and the underscore-spelled `allow_nil` are accepted.

## `allow-blank` / `allow_blank`

Skips every validator on the declaration when the attribute is undefined, the empty string, whitespace-only, an empty collection, or `False`. Use this when an attribute is optional but, when present, must meet the validators' constraints.

```perl6
use ORM::ActiveRecord::Model;

class Event is Model {
  submethod BUILD {
    self.validate: 'name',
      { :presence, length => { min => 3 }, allow-blank => True }
  }
}

say Event.build({name => ''}).is-valid;      # True — blank is allowed
say Event.build({name => '   '}).is-valid;   # True — whitespace counts as blank
say Event.build({name => 'AB'}).is-valid;    # False — non-blank, fails length
```

## `strict`

Raises [`X::StrictValidationFailed`](/docs/ORM-ActiveRecord/errors) instead of pushing onto `errors`. The exception carries `model`, `attribute`, and `message-text` so callers can inspect what failed.

```perl6
use ORM::ActiveRecord::Model;
use ORM::ActiveRecord::Errors::X;

class Event is Model {
  submethod BUILD {
    self.validate: 'name', { :presence, :strict }
  }
}

my $e = Event.build({name => ''});
try {
  $e.is-valid;
  CATCH {
    when X::StrictValidationFailed {
      say .attribute;     # name
      say .message-text;  # must be present
    }
  }
}
```

## `as`

Overrides the value substituted for `{attribute}` in error message templates. Useful when the column name is internal but the user-facing label needs to be friendlier.

```perl6
use ORM::ActiveRecord::Model;

class Event is Model {
  submethod BUILD {
    self.validate: 'max_score',
      { :presence, as => 'Maximum Score', message => '{attribute} must be present' }
  }
}

my $e = Event.build({});
$e.is-valid;
say $e.errors.max_score[0];   # Maximum Score must be present
```

`as` does not change which key the error is stored under in `errors` — `errors.max_score` still works.

## `case-sensitive` (uniqueness only)

Controls whether the uniqueness lookup matches case-sensitively. Defaults to `True`. On MySQL the case-sensitive branch uses `BINARY` to bypass the default `_ci` collation, so the same declaration behaves identically across PostgreSQL, MySQL, and SQLite.

```perl6
use ORM::ActiveRecord::Model;

class User is Model {
  submethod BUILD {
    self.validate: 'username', { uniqueness => { case-sensitive => False } }
  }
}

User.create({username => 'Alfred'});
say User.build({username => 'alfred'}).is-valid;   # False
```

## `conditions` (uniqueness only)

Narrows the uniqueness lookup to records matching an extra `WHERE` clause. Combines with `scope` when both are given.

```perl6
use ORM::ActiveRecord::Model;

class User is Model {
  submethod BUILD {
    self.validate: 'username',
      { uniqueness => { conditions => { is_active => True } } }
  }
}

User.create({username => 'gone',   is_active => False});
say User.build({username => 'gone', is_active => True}).is-valid;
# True — the existing 'gone' row is inactive and ignored by the conditions clause.
```

Combine `scope` and `conditions` to enforce "unique-per-tenant, only among active rows":

```perl6
self.validate: 'username',
  { uniqueness => { scope => :tenant_id, conditions => { is_active => True } } }
```
