---
title: Meta-programming capabilities using FALLBACK
date: 2019-10-04
slug: meta-programming-capabilities-using-fallback
description: Being a long time Ruby user and Lisp dabbler, Rakudo Perl 6 meta-programming capabilities ranked high on my list of curiosities. Meta-programming can be described as "writing code that writes code"....
tags: [raku]
archives: [2019-10]
---
Being a long time Ruby user and Lisp dabbler, Rakudo Perl 6 meta-programming capabilities ranked high on my list of curiosities.

Meta-programming can be described as "writing code that writes code". This includes code that can alter itself.

I was experimenting and discovered I **could not** add an attribute to a class at runtime:

```actionscript
class Foo does Metamodel::AttributeContainer {
  submethod BUILD(:$attr) {
    my $a = Attribute.new(:name($attr), :type(Str), :package(Foo));
    self.add_attribute(self, $a);
  }
}

my Foo $foo = Foo.new(:attr('bar'));
$foo.bar = 'baz';  # No such method 'bar' for invocant of type 'Foo'
say $foo.bar;
```

I found the attribute could be managed in a Hash and if I used *return-rw* inside FALLBACK I could even return a mutable l-value:

```actionscript
class Foo {
  has %.attrs;
  method FALLBACK(Str:D $name, *@rest) {
    return-rw %!attrs{$name};
  }
}

my $foo = Foo.new;
$foo.bar = 'baz';
say $foo.bar;
```

Thanks to Jonathan Worthington for the help:

[https://stackoverflow.com/questions/57993203/how-to-add-a-class-attribute-dynamically](https://stackoverflow.com/questions/57993203/how-to-add-a-class-attribute-dynamically)
