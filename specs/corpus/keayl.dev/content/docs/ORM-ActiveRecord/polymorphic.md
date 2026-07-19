---
title: Polymorphic associations
slug: polymorphic
order: 34
description: A polymorphic belongs-to can point at more than one kind of parent. A picture is imageable — it might belong to a user or to a post. The row stores both...
toc: true
---
A polymorphic `belongs-to` can point at more than one kind of parent. A picture
is `imageable` — it might belong to a user or to a post. The row stores both the
id and the parent's type.

## Setup

The child declares a polymorphic `belongs-to`:

```perl6
class Picture is Model {
  submethod BUILD {
    self.belongs-to: imageable => :polymorphic;
  }
}
```

Each possible parent declares the inverse `has-many` with `as` naming the
polymorphic role:

```perl6
class User is Model {
  submethod BUILD {
    self.has-many: pictures => %(class-name => 'Picture', as => 'imageable');
  }
}

class Post is Model {
  submethod BUILD {
    self.has-many: pictures => %(class-name => 'Picture', as => 'imageable');
  }
}
```

The `:polymorphic` column option creates both `imageable_id` and
`imageable_type`:

```perl6
self.create-table: 'pictures', [
  name      => { :string, limit => 80 },
  imageable => { :reference, :polymorphic },
];
```

## Assigning different parent types

Pass the parent record to the polymorphic attribute; the loader records its type:

```perl6
my $user = User.create({fname => 'Greg', lname => 'Donald'});
my $post = Post.create({title => 'Hello'});

my $avatar = Picture.create({name => 'avatar.png', imageable => $user});
my $hero   = Picture.create({name => 'hero.png',   imageable => $post});

$avatar.attrs<imageable_id>;     # $user.id
$avatar.attrs<imageable_type>;   # 'User'
```

Reading the association back resolves to the right class:

```perl6
my $picture = Picture.find($avatar.id);
$picture.imageable.WHAT === User;   # True
$picture.imageable.id;              # $user.id
```

## The inverse collection

Each parent sees only its own children, filtered by `imageable_type`:

```perl6
$user.pictures.elems;
$user.pictures.grep({ .attrs<imageable_type> eq 'User' }).elems;
```

Reassigning the parent moves the row between collections:

```perl6
$avatar.update({imageable => $post});

User.find($user.id).pictures.elems;   # one fewer
Post.find($post.id).pictures.elems;   # one more
```

## Optional parents

Declare `:optional` to allow a child with no parent:

```perl6
class Attachment is Model {
  submethod BUILD {
    self.belongs-to: attachable => %(:polymorphic, :optional);
  }
}
```

```perl6
my $bare = Attachment.create({name => 'unattached.txt'});
$bare.attachable.defined;          # False
$bare.attrs<attachable_id>;        # 0
```

## Eager loading

`preload` resolves a polymorphic `belongs-to` by grouping per type:

```perl6
my @atts = Attachment.where({}).preload(:attachable).all;
$atts[0].attachable.WHAT;          # User or Post, already loaded
```

The polymorphic `has-many` preloads too, each parent getting only its own rows:

```perl6
my @users = User.where({}).preload(:pictures).all;
@users[0].pictures.elems;          # no extra query
```

Nested preloads work from a polymorphic parent — load attachments, their
parents, and each parent's pictures:

```perl6
my @atts = Attachment.where({}).preload(attachable => :pictures).all;
$atts[0].attachable.pictures.elems;
```
