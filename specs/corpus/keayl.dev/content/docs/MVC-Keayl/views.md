---
title: Views
slug: views
order: 11
description: MVC::Keayl::View renders templates. It resolves a template by name, format, and handler, compiles it through a pluggable handler, and caches the result....
toc: true
---
`MVC::Keayl::View` renders templates. It resolves a template by name, format, and
handler, compiles it through a pluggable handler, and caches the result. HAML is
the default handler, backed by [Template::HAML](https://github.com/gdonald/Template-HAML).

```perl6
use MVC::Keayl::View;

my $view = MVC::Keayl::View.new(:paths(['app/views']));

$view.render-template('users/show', { user => $user });
```

## Template resolution

`render-template` resolves a name against each search path in turn, then by
handler extension, preferring a format-qualified file:

```
app/views/users/show.html+phone.haml  # users/show, format html, variant phone
app/views/users/show.html.haml        # users/show, format html
app/views/users/show.haml             # format-less fallback
```

The default format is `html`, overridable per call with `:format`. A `:variant`
prefers a variant-qualified file before the plain one, so `phone` resolves
`show.html+phone.haml` and falls back to `show.html.haml`. When the View is given
a controller, a name with no `/` is resolved relative to the controller's path
(`UsersController` resolves under `users/`).

## Handlers

A handler compiles a template source and renders it with locals. The default
`haml` handler renders HAML. Register others by extension:

```perl6
$view.register-handler('txt', MyTextHandler.new);
```

A handler does the `MVC::Keayl::View::Handler` role: `compile($source)` returns a
compiled template, and `render($compiled, %locals)` produces the output.

## Caching and reloading

Compiled templates are cached by file. `reload` (on by default) re-checks the
file's modification time and recompiles when it changes, which suits development.
Setting `cache => False` disables caching so every render recompiles:

```perl6
my $view = MVC::Keayl::View.new(:paths(['app/views']), :!reload);   # trust the cache
```

## Inline rendering

`render-inline` renders a template string directly, without resolving a file.

## Layouts

When an action renders a template, the rendered body is wrapped in a layout from
`layouts/`. With no layout chosen, the `application` layout is used if it exists;
otherwise the template renders on its own. Inside the layout, the wrapped body is
available through the `yield` helper:

```haml
%html
  %body
    != yield()
```

The body is also exposed as a `content` local, so `$content` works in place of
`yield()`:

```haml
%html
  %body
    != $content
```

Passing an explicit `content` local when rendering overrides the wrapped body.

Choose a layout per action with the `:layout` option, or for every action of a
controller with the class-level `layout` declaration:

```perl6
self.render('show', :layout('print'));   # this action only

class AdminController is MVC::Keayl::Controller { }
AdminController.layout('admin');          # every action

self.render('show', :layout(False));      # no layout
```

The `layout` declaration also has an `is layout` trait form for the class header:

```perl6
class AdminController is MVC::Keayl::Controller is layout('admin') { }
```

An action-level `:layout` overrides a controller declaration, which overrides the
`application` default.

## content-for and yield

A template captures named content with `content-for`, and the layout places it
with `yield`:

```haml
-# in the template
= content-for('title', 'Dashboard')

-# in the layout
%title= yield('title')
%body
  != yield()
```

`yield()` returns the main body; `yield(name)` returns the matching
`content-for` capture, or an empty string when none was set.

## Partials

A partial is a template whose file name begins with an underscore. It is referred
to without the underscore. `render-partial` renders one with locals:

```perl6
$view.render-partial('users/form', { user => $user });   # renders users/_form
```

A name with no path segment resolves under the controller's view path; a name
with a `/` resolves from the views root, so `shared/menu` renders
`shared/_menu` from any controller.

Inside a template, the `partial` helper embeds a partial. Use `!=` so its HTML is
not escaped:

```haml
%ul
  != partial('users/row', %( user => $user ))
```

### Object partials

`render-object` derives the partial from an object. By default the partial name
and the local both come from the object's class name; an object can override the
path with a `to-partial-path` method:

```perl6
class Post {
  method to-partial-path { 'posts/post' }   # renders posts/_post, local `post`
}

$view.render-object($post);                  # in a template: != partial-for($post)
```

A controller renders an object directly with `render($post)`.

### Collection partials

`render-collection` renders a partial once per item. Each render receives the
item under the partial's local name and a zero-based `{local}_counter`:

```haml
-# greetings/_line.html.haml
%p= $line ~ ' #' ~ $line_counter
```

```haml
!= partial-each('greetings/line', $lines)
```

A `spacer` partial renders between items:

```perl6
$view.render-collection('greetings/line', @lines, spacer => 'greetings/divider');
```

A controller renders a collection with `render(:partial('line'), :collection(@lines))`.

## Output safety

HAML escapes interpolated values by default, so `= $value` is safe and `!= $value`
emits raw HTML. `MVC::Keayl::SafeString` adds a safe-buffer type and helpers for
composing and cleaning HTML.

`html-escape` encodes the markup-significant characters. `html-safe` and `raw`
wrap a string as a `SafeString`, which reports `is-html-safe` and stringifies to
its raw content. Concatenation escapes anything that is not already safe:

```perl6
html-safe('<b>a</b>').concat('<unsafe>');     # <b>a</b>&lt;unsafe&gt;
safe-join([html-safe('<b>a</b>'), '<x>']);    # <b>a</b>&lt;x&gt;
```

`sanitize` keeps an allowlist of tags and attributes, removes `script` and `style`
elements with their content, and strips event-handler attributes and
`javascript:` URLs:

```perl6
sanitize('<a href="javascript:x()" onclick="y()">hi</a>');   # <a>hi</a>
sanitize('<b>ok</b>', tags => <b i em>);                     # custom allowlist
```

`json-escape` encodes the characters that could end a `<script>` block (`<`, `>`,
`&`, and the line and paragraph separators) as `\uXXXX`, for embedding JSON in a
script context.

In templates these are available as the `escape`, `raw`, `sanitize`, and `json`
helpers; pair `sanitize`/`raw` with `!=` so the cleaned HTML is not escaped again.
