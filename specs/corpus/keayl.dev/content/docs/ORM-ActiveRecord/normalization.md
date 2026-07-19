---
title: Normalisation
slug: normalization
order: 11
description: normalizes declares a transform applied to an attribute before validation and save, so the stored value is always canonical. Declare it in submethod BUILD.
toc: true
---
`normalizes` declares a transform applied to an attribute before validation and
save, so the stored value is always canonical. Declare it in `submethod BUILD`.

```perl6
class Contact is Model {
  submethod BUILD {
    self.normalizes('email', :with(-> $v { $v.trim.lc }));
  }
}

my $c = Contact.create({ email => '  Foo@Bar.COM ' });
$c.email;                       # 'foo@bar.com'
```

The normaliser runs before validation, so validations see the normalised value.

## Multiple attributes

Pass several attribute names to apply the same normaliser to each:

```perl6
self.normalizes('first-name', 'last-name', :with(-> $v { $v.trim }));
```

## Queries

A lookup on a normalised attribute normalises the search value too, so a query
matches what was stored regardless of the input's casing or whitespace:

```perl6
Contact.where({ email => '  FOO@BAR.COM ' });   # finds 'foo@bar.com'
```

`normalize-value-for(attribute, value)` returns the normalised form of a value
(or the value unchanged when the attribute has no normaliser), which is what the
query layer uses.
