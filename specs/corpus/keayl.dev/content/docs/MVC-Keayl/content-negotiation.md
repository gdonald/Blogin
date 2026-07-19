---
title: Content negotiation
slug: content-negotiation
order: 15
description: MVC::Keayl::Mime is the format registry. mime-type maps a format to its MIME type and mime-format maps a type (ignoring parameters and aliases) back to a...
toc: true
---
## MIME types

`MVC::Keayl::Mime` is the format registry. `mime-type` maps a format to its MIME
type and `mime-format` maps a type (ignoring parameters and aliases) back to a
format. `register-mime` adds or extends entries:

```perl6
mime-type('json');                       # application/json
mime-format('application/json; q=1');     # json
register-mime('msgpack', 'application/msgpack');
```

`parse-accept` orders an `Accept` header's types by quality, and `negotiate`
chooses the best of a list of available formats, honouring `*/*` and `type/*`
wildcards and falling back to the first available format when there is no
preference:

```perl6
negotiate(['html', 'json'], 'application/json;q=0.4, text/html;q=0.8');   # html
```

## respond-to

A controller action serves several formats with `respond-to`, taking an ordered
list of format-to-block pairs. The format is taken from the path extension if
present, otherwise negotiated from the `Accept` header; the first declared format
is the default. A request that matches none gets `406`:

```perl6
method show {
  self.respond-to([
    html => { self.render('show') },
    json => { self.render(:json($post.to-hash)) },
  ]);
}
```

`/posts.json` forces JSON; `/posts` with `Accept: text/html` serves HTML.

## Variants

A variant selects a device-specific view of the same format. `request.variant`
holds the variant, set explicitly or from a user-agent heuristic:

```perl6
self.request.set-variant('phone');
self.request.set-variant(self.request.detect-variant);  # phone, tablet, or none
```

A format block in `respond-to` may be a map of variant names to blocks instead of
a single block. The block for the current variant runs, falling back to `any`:

```perl6
self.respond-to([
  html => {
    phone => { self.render('show_phone') },
    any   => { self.render('show') },
  },
]);
```

Variants also steer template lookup. When the variant is set, the renderer
prefers a variant template before the plain one, so `phone` resolves
`show.html+phone.haml` and falls back to `show.html.haml`.

## API controllers

`MVC::Keayl::APIController` is a JSON-first controller. It needs no view renderer:
rendering a bare object or a collection produces JSON, while string templates and
explicit render options still work as on a normal controller.

```perl6
class PostsController is MVC::Keayl::APIController {
  method show  { self.render($post) }            # {"title":"..."}
  method index { self.render($posts) }           # [{"title":"..."}, ...]
  method made  { self.render($post, :status(201)) }
}
```

A model is serialized through `serialize`, which calls the object's `to-hash`
(recursing into collections) by default. Provide a `serializer` to control the
shape:

```perl6
PostsController.new(serializer => -> $model { %( id => $model.id, title => $model.title ) });
```
