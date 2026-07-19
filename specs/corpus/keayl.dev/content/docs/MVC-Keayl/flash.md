---
title: Flash
slug: flash
order: 18
description: The flash carries short messages across a redirect. It is stored in the session, so it needs a session store. A controller exposes it as flash.
toc: true
---
The flash carries short messages across a redirect. It is stored in the session,
so it needs a session store. A controller exposes it as `flash`.

```perl6
self.flash<notice> = 'Saved';   # shown on the next request
redirect-to('/posts');
```

A value written to `flash` survives exactly one request. It is loaded on the next
request, readable there, and then dropped, so a message shown after a redirect
does not linger.

When the flash has entries, it is exposed to views as the `flash` local:

```haml
- with $flash<notice>
  %p.notice= $flash<notice>
```

## flash.now

`flash.now` makes a message available in the current request only, for a page
rendered without a redirect. It is readable this request but is never carried to
the next one:

```perl6
self.flash.now<alert> = 'Could not save';
render('edit');
```

## keep and discard

`keep` carries the current flash to another request instead of dropping it.
`discard` removes an entry before it is saved. Both take an optional key, or act
on the whole flash when called without one:

```perl6
self.flash.keep;            # carry everything one more request
self.flash.keep('notice');  # carry just this entry
self.flash.discard('alert');
```

## Flash types

`add-flash-types` registers named flash types that read and write through a
method on the flash, alongside the usual `flash<key>` access:

```perl6
ApplicationController.add-flash-types('success', 'error');

self.flash.success('Saved');   # writes flash<success>
self.flash.success;            # reads flash<success>
```

It also has an `is add-flash-types` trait form for the class header:

```perl6
class ApplicationController is MVC::Keayl::Controller is add-flash-types('success', 'error') { }
```

A method call for a type that was never registered raises. Registration is
global, so a type added on the base controller is available everywhere.
