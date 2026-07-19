---
title: Through associations
slug: through-associations
order: 33
description: A :through association reaches a second model by way of a join model. A user subscribes to magazines through subscriptions; the subscription is a real...
toc: true
---
A `:through` association reaches a second model by way of a join model. A user
subscribes to magazines through subscriptions; the subscription is a real model
with its own row.

## Setup

The join model belongs to both sides:

```perl6
class Subscription is Model {
  submethod BUILD {
    self.belongs-to: user     => class-name => 'User';
    self.belongs-to: magazine => class-name => 'Magazine';
  }
}
```

Each side declares the direct `has-many` to the join model, then a second
`has-many` that travels `through` it:

```perl6
class User is Model {
  submethod BUILD {
    self.has-many: subscriptions => class-name => 'Subscription';
    self.has-many: magazines     => %(through => :subscriptions, class-name => 'Magazine');
  }
}

class Magazine is Model {
  submethod BUILD {
    self.has-many: subscriptions => class-name => 'Subscription';
    self.has-many: users         => %(through => :subscriptions, class-name => 'User');
  }
}
```

The join table carries the two foreign keys:

```perl6
self.create-table: 'subscriptions', [
  user     => { :reference },
  magazine => { :reference },
];
self.add-index: 'subscriptions', <user_id magazine_id> => { :unique };
```

## Reading through the join

Create the join rows, then read straight through:

```perl6
my $user = User.create({fname => 'Greg'});
my $mag  = Magazine.create({title => 'Mad'});
Subscription.create({user => $user, magazine => $mag});

$user.magazines.first.id;   # $mag.id
```

The collection behaves like any other: `.elems`, `.map`, `.grep`.

```perl6
$user.magazines.elems;
$user.magazines.map(*.attrs<title>).sort;
```

## Eager loading a through association

`preload` loads the whole graph in batches and also caches the intermediate
join collection, so the join rows are there too:

```perl6
my @users = User.where({}).preload(:magazines).all;
my $alice = @users.first({ .attrs<fname> eq 'Alice' });

$alice.magazines.elems;                 # no extra query
$alice.subscriptions.elems;             # the join rows are cached as well
```

See [Eager loading](/docs/ORM-ActiveRecord/eager-loading) for `preload` vs `includes` vs
`eager-load`.

## has-one :through

The singular form works the same way. A user has one account through their
profile:

```perl6
class Profile is Model {
  submethod BUILD {
    self.belongs-to: user    => class-name => 'User';
    self.belongs-to: account => %(class-name => 'Account', optional => True);
  }
}

class User is Model {
  submethod BUILD {
    self.has-one: account => %(through => :profile);
  }
}
```

```perl6
my $account = Account.create({name => 'gdonald'});
Profile.create({user => $user, account => $account, bio => 'Raku enthusiast'});

User.find($user.id).account.id;   # $account.id
```

When no join row exists, the singular accessor returns an undefined value:

```perl6
User.find($other.id).account.defined;   # False
```
