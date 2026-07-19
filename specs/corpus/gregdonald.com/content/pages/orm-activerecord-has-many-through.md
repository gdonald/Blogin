---
title: ORM::ActiveRecord has-many through
date: 2019-10-24
slug: orm-activerecord-has-many-through
description: ORM::ActiveRecord now has support for has-many through relationships.
tags: [raku]
archives: [2019-10]
---
[ORM::ActiveRecord](https://rakuist.io/modules/orm-activerecord) now has support for [has-many through](http://docs.rakuist.io/orm-activerecord/models/models/#has-many-through) relationships.

For example, a **user** has access to **magazines** *through* the **subscriptions** relationship:

```actionscript
class Subscription {...} # stub

class User is Model {
  submethod BUILD {
    self.has-many: subscriptions => class => Subscription;
    self.has-many: magazines => through => :subscriptions;
  }
}

class Magazine is Model {}

class Subscription is Model {
  submethod BUILD {
    self.belongs-to: user => class => User;
    self.belongs-to: magazine => class => Magazine;
  }
}

my $user = User.create({fname => 'Greg'});
my $magazine = Magazine.create({title => 'Mad'});
Subscription.create({:$user, :$magazine});

say $user.magazines.first == $magazine;
```
