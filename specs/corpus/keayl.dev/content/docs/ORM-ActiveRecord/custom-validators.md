---
title: Custom validators
slug: custom-validators
order: 36
description: When the built-in validators don't fit, write your own. There are two shapes: a validator class for reusable rules, and an inline block for one-off...
toc: true
---
When the built-in validators don't fit, write your own. There are two shapes: a
validator class for reusable rules, and an inline block for one-off per-attribute
checks.

## A validator class

A validator is any class with a `validate($record)` method that pushes onto the
record's `errors`. Attach state through attributes:

```perl6
use ORM::ActiveRecord::Errors::Error;
use ORM::ActiveRecord::Schema::Field;

class NotEvilValidator is export {
  has Str $.banned = 'Evil';

  method validate($record) {
    if $record.attrs<name> eq $!banned {
      my $field = Field.new(:name('base'), :type('association'));
      $record.errors.push(
        Error.new(:$field, :message("'$!banned' is not allowed"))
      );
    }
  }
}

class ScoreInsanityValidator is export {
  has Int $.cap = 1000;

  method validate($record) {
    if $record.attrs<score> > $!cap {
      my $field = Field.new(:name('score'), :type('integer'));
      $record.errors.push(
        Error.new(:$field, :message("score exceeds cap of $!cap"))
      );
    }
  }
}
```

Use `Field.new(:name('base'), ...)` for a record-level error, or the attribute
name for an attribute-level one. See [Errors](/docs/ORM-ActiveRecord/errors) for the error API.

## Attaching with validates-with

Pass either an instance (when you don't need options) or the class plus options
(it constructs the validator for you):

```perl6
class Cabaret is Model {
  method table-name { 'concerts' }

  submethod BUILD {
    self.validates-with(NotEvilValidator.new);
    self.validates-with(ScoreInsanityValidator, :cap(50));
  }
}
```

The errors land where the validator put them:

```perl6
my $bad = Cabaret.build({name => 'Evil', score => 5, max_score => 10});
$bad.is-invalid;                  # True
$bad.errors.base[0];              # "'Evil' is not allowed"

my $over = Cabaret.build({name => 'OK', score => 500, max_score => 1000});
$over.is-invalid;
$over.errors.score[0];            # 'score exceeds cap of 50'
```

## Inline per-attribute checks with validates-each

`validates-each` runs a block once per named attribute, receiving the record,
the attribute name, and its value:

```perl6
class Symphony is Model {
  method table-name { 'concerts' }

  submethod BUILD {
    self.validates-each: <name>, -> $rec, $attr, $value {
      if $value && $value ~~ /^ <:Ll> / {
        my $field = Field.new(:name($attr), :type('string'));
        $rec.errors.push(Error.new(:$field, :message('must start with capital letter')));
      }
    }
  }
}
```

Pass several attributes to apply the same block to each:

```perl6
self.validates-each: <score max_score>, -> $rec, $attr, $value {
  if $value < 0 {
    my $field = Field.new(:name($attr), :type('integer'));
    $rec.errors.push(Error.new(:$field, :message('must not be negative')));
  }
}
```

Both reach `errors` under the attribute name:

```perl6
my $sym = Symphony.build({name => 'lowercase', score => 1, max_score => 1});
$sym.is-invalid;
$sym.errors.name[0];   # 'must start with capital letter'
```

`validates-each` takes the same `:if` / `:unless` guards as the built-in
validators:

```perl6
self.validates-each: <name>, &check-name, { :if => { self.score > 0 } };
```
