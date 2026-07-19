---
title: Custom Operator - Basic Object Identity
date: 2019-10-12
slug: custom-operator-basic-object-identity
description: It is often the case in object oriented programming you need to compare two objects to see if they are the same. Usually it just means doing a deeper comparison of the attributes on each object.
tags: [raku]
archives: [2019-10]
---
It is often the case in object oriented programming you need to compare two objects to see if they are the same. Usually it just means doing a deeper comparison of the attributes on each object. Rakudo Perl 6 lets you create custom operators to make the actual comparison cleaner and reusable:

```actionscript
class User { has %.attrs }

multi sub infix:<==>(User $a, User $b --> Bool) {
  my @keys = $a.attrs.keys;
  return False unless @keys.elems == $b.attrs.keys.elems;

  for @keys -> $k {
    given $a.attrs{$k} {
      when Numeric { return False unless $a.attrs{$k} == $b.attrs{$k} }
      when Str     { return False unless $a.attrs{$k} eq $b.attrs{$k} }
      default      { say 'Unknown type: ' ~ $a.attrs{$k}.^name; die }
    }
  }

  True;
}

use Test;

my $fred = User.new(:attrs(id => 1, name => 'Fred'));
my $barney = User.new(:attrs(id => 2, name => 'Barney'));
nok $fred == $barney;

my $fred2 = User.new(:attrs(id => 1, name => 'Fred'));
ok $fred == $fred2;
```
