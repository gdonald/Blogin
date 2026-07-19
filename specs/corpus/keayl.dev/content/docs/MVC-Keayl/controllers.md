---
title: Controllers
slug: controllers
order: 10
description: A controller is a subclass of MVC::Keayl::Controller. Each public method is an action. The framework builds one controller instance per request,...
toc: true
---
A controller is a subclass of `MVC::Keayl::Controller`. Each public method is an
action. The framework builds one controller instance per request, dispatches the
named action on it, and returns the [response](/docs/MVC-Keayl/response).

```perl6
use MVC::Keayl::Controller;

class UsersController is MVC::Keayl::Controller {
  method index {
    'all users';
  }

  method show {
    'user ' ~ self.params<id>;
  }
}
```

## Per-request state

A controller instance carries the request, a fresh response, and the merged
params for that request:

```perl6
self.request;    # the incoming Request
self.response;   # the outgoing Response
self.params;     # path, query, and body params
```

Each instance gets its own response, so actions never share state across
requests.

## Params

`self.params` merges the path params (from routing), the query string, and the
request body into one `MVC::Keayl::Parameters` collection. Path params win over
the body, which wins over the query. Access is indifferent: a key is coerced to a
string, so `params<id>` and `params{5}` reach the same value.

Bracketed names build nested structures, the same way Rack does:

```
user[name]=Ada&user[email]=a@b.com   # params<user><name>, params<user><email>
ids[]=1&ids[]=2                      # params<ids> is ['1', '2']
user[roles][]=admin                  # params<user><roles> is ['admin']
users[][name]=A&users[][age]=1       # params<users>[0] is { name => 'A', age => '1' }
```

The body is parsed by its `Content-Type`:

- `application/x-www-form-urlencoded` parses like a query string.
- `application/json` parses the JSON object into params.
- `multipart/form-data` parses text fields, and file fields into a
  `{ filename, content, type }` hash. The file `content` is a `Blob` of the
  raw uploaded bytes, so binary uploads (images, PDFs) survive byte-for-byte.

`build-params(%path-params, $request)` produces the merged `Parameters` for a
request, which the dispatcher passes to the controller.

## Dispatch and implicit render

`dispatch($action)` runs the named action and returns the response. An action
interacts with the response directly:

```perl6
method create {
  self.response.status = 201;
  self.response.body('created');
}
```

When an action does not write to the response, its return value is rendered
implicitly. An action whose body is still empty has its string return value
written to the response:

```perl6
method show {
  'user ' ~ self.params<id>;   # becomes the response body
}
```

Only actions defined on the controller are dispatchable. Dispatching an unknown
action, or one of the base controller's own methods, raises.

## Render

`render` writes the response explicitly. The content modes set a default content
type and the body:

```perl6
self.render(json => { ok => True });   # application/json
self.render(plain => 'hello');         # text/plain
self.render(html  => '<b>hi</b>');     # text/html
self.render(body  => 'id,name', content-type => 'text/csv');
```

`status` sets the status, on its own or alongside content, and `content-type`
overrides the default:

```perl6
self.render(plain => 'created', status => 201);
self.render(status => 204);            # no body
```

A template is rendered by name, by another action, or inline. Locals and a layout
are passed as options:

```perl6
self.render('show');                   # the show template
self.render(action => 'edit');         # another action's template
self.render('show', locals => { user => $user });
self.render(inline => '<p>= name</p>');
self.render('show', layout => 'admin');
self.render('show', layout => False);  # no layout
```

Template, inline, and layout rendering go through the controller's view renderer.
When a renderer is configured, an action that does not render explicitly
implicitly renders the template named after the action. An `index` action with no
`render` call renders the `index` template, so the explicit call is rarely needed.

Implicit render follows the request format. The default is `html`, but when the
request asks for another format (a path extension such as `/feed.atom`) and a
template exists for it, that template is rendered with the matching content type.
The format-specific template is `name.<format>.<handler>` (for example
`index.xml.haml`); the html layout is skipped for non-html formats. When no
template exists for the requested format, implicit render falls back to the html
template. Pass `format` to choose one explicitly:

```perl6
self.render('index', format => 'xml');   # renders index.xml.haml, types it application/xml
```

The handler is the file's last extension, so the default HAML handler covers the
markup formats it can emit (html, xml, atom, rss, svg). It does not produce JSON.
For JSON, render data directly with `render(:json($data))` or
[`respond-to`](/docs/MVC-Keayl/content-negotiation), or register a handler for a JSON template
language so `index.json.<handler>` resolves.

Rendering twice raises a double-render error.

## Redirects and head

`redirect-to` sends a redirect with an empty body. It takes a path or URL, and an
optional status (numeric or named, defaulting to 302). `:back` redirects to the
`Referer`, with a fallback when there is none:

```perl6
self.redirect-to('/dashboard');
self.redirect-to('https://example.com');
self.redirect-to('/new', status => 301);
self.redirect-to('/x', status => 'see-other');   # 303
self.redirect-to(:back, fallback => '/home');
```

A named-route redirect is the path a URL helper produces, passed as the target.

`head` sends a status and headers with no body. The status is numeric or named,
and named arguments become headers (`location` becomes the `Location` header):

```perl6
self.head(204);
self.head('not-found');
self.head('created', location => '/users/5');
```

A redirect, like a render, marks the response performed, so a render or redirect
after one raises a double-render error.

## Sending files and data

`send-data` sends an in-memory payload, a string or a `Blob`, with a content type
and disposition. The default disposition is `attachment`:

```perl6
self.send-data($csv, type => 'text/csv', filename => 'report.csv');
self.send-data($bytes, filename => 'image.png');
self.send-data($html, disposition => 'inline');
```

`send-file` sends a file from disk. The content type is guessed from the
extension unless given, and the filename defaults to the basename:

```perl6
self.send-file('tmp/report.csv');
self.send-file('public/logo.png', disposition => 'inline');
```

`send-file` advertises `Accept-Ranges: bytes` and honours a `Range` request
header, replying with `206 Partial Content`, a `Content-Range` header, and the
requested slice. Open-ended (`bytes=100-`) and suffix (`bytes=-100`) ranges both
work.

## Callbacks

`before-action`, `after-action`, and `around-action` register callbacks on the
controller class. A callback is a method name or a block. Around callbacks
receive a continuation they invoke to run the rest of the chain. The registration
calls go after the class definition, because the class name is not yet bound to
the composed type while the body is still being built:

```perl6
class UsersController is MVC::Keayl::Controller {
  method authenticate { ... }
  method timer($next) { ...; $next(); ... }
  method show { ... }
}

UsersController.before-action('authenticate');
UsersController.around-action('timer');
```

A request runs the before callbacks in order, then the around callbacks wrapping
the action, then the after callbacks in reverse order.

`only` and `except` scope a callback to specific actions, and `if` / `unless`
gate it on a method or block:

```perl6
UsersController.before-action('authenticate', except => <index show>);
UsersController.before-action('require-admin', if => 'is-admin');
```

### Declaring callbacks inside the class

To keep registration in the class body, call through `$?CLASS`, the handle to the
class being defined:

```perl6
class UsersController is MVC::Keayl::Controller {
  $?CLASS.before-action('authenticate', except => <index show>);
  $?CLASS.around-action('timer');

  method authenticate { ... }
  method timer($next) { ...; $next(); ... }
  method show { ... }
}
```

Or attach a callback to its method with the `is before-action`, `is
around-action`, and `is after-action` traits. The method is the callback, and the
trait takes the same `only` / `except` / `if` / `unless` options:

```perl6
class UsersController is MVC::Keayl::Controller {
  method authenticate is before-action(except => <index show>) { ... }
  method timer($next)  is around-action { ...; $next(); ... }
  method audit         is after-action  { ... }
  method show { ... }
}
```

A subclass inherits its parents' callbacks and can drop one with
`skip-before-action` (and `skip-after-action` / `skip-around-action`), with the
same `only` / `except` scoping:

```perl6
PublicController.skip-before-action('authenticate');
```

A before or around callback that renders or redirects halts the chain: the action
and the remaining callbacks do not run.

## Strong parameters

`require` and `permit` whitelist params before they reach the model. `require`
returns the value at a key, raising when it is missing or empty. `permit` returns
a new `Parameters` containing only the listed keys:

```perl6
my $attrs = self.params.require('user').permit('name', 'email');
```

`permit` accepts scalar keys, arrays (an empty-array spec), nested hashes, and
arrays of hashes:

```perl6
self.params.require('user').permit(
  'name', 'email',
  roles   => [],              # an array of scalars
  address => <street city>,   # a nested hash with these keys
  tags    => <id name>,       # an array of hashes with these keys
);
```

A value that is not a permitted scalar (a nested hash or array) is dropped unless
it is permitted explicitly. `permit-all` is the escape hatch that marks the
parameters permitted and keeps every key.

Unpermitted keys are dropped. The action taken is configurable, globally with
`MVC::Keayl::Parameters.unpermitted-action('raise')` or per call with
`permit(..., :on-unpermitted<raise>)`, which raises instead of dropping.

### Expecting parameters

`expect` combines `require` and `permit` into one strict call. It returns the
permitted value at a key, raising `X::MVC::Keayl::ParameterMissing` when the key
is missing or when the value is the wrong shape:

```perl6
my $user = self.params.expect(user => <name email>);
```

The default rescue turns that exception into a `400`, so a malformed payload (a
scalar where a hash is expected, or a missing key) is rejected without reaching
the model. `expect` follows the value's shape: a hash value permits the listed
keys, an array value permits each element as a hash, and an empty-array spec
returns an array of scalars:

```perl6
self.params.expect(ids => []);          # an array of scalars
self.params.expect(rows => <id name>);  # an array of hashes, when rows is a list
self.params.expect('id');               # a required scalar
```

### Wrapping parameters

`wrap-parameters` nests a JSON request body under a root key, so a client that
posts `{"name": "Ada"}` to `UsersController` reads it back as `params<user>`. The
key defaults to the controller name singularized, and only JSON requests are
wrapped:

```perl6
UsersController.wrap-parameters;
```

Pass an explicit key as the first argument, restrict the formats with `:format`,
and choose attributes with `:include` or `:exclude`:

```perl6
UsersController.wrap-parameters('person', include => <name email>);
```

Or declare it on the class with the `is wrap-parameters` trait:

```perl6
class UsersController is MVC::Keayl::Controller is wrap-parameters('person', include => <name email>) { }
```

Wrapping is skipped when the root key is already present in the params, so an
explicitly nested body is left untouched.

## Custom renderers

`add-renderer` registers a render option backed by a block, so `render csv: $rows`
dispatches to it. The block receives the controller, the rendered value, and the
remaining render options, sets its own content type, and returns the body:

```perl6
MVC::Keayl::Controller.add-renderer('csv', -> $controller, $rows, %options {
  $controller.response.content-type('text/csv');
  $rows.map(*.join(',')).join("\n")
});

method export { self.render(csv => @rows) }
```

Renderers are registered globally and are available to every controller.

## Rescuing exceptions

`rescue-from` maps an exception type to a handler, a method name or a block, that
turns the exception into a response:

```perl6
class ArticlesController is MVC::Keayl::Controller {
  method not-found($error) { self.head(404) }
}

ArticlesController.rescue-from(X::MVC::Keayl::NotFound, 'not-found');
```

Or attach the mapping to the handler method with the `is rescue-from` trait. The
method is the handler, and the trait takes the exception type (or several):

```perl6
class ArticlesController is MVC::Keayl::Controller {
  method not-found($error) is rescue-from(X::MVC::Keayl::NotFound) { self.head(404) }
}
```

Lookup is inheritance-aware: when more than one registered type matches a raised
exception, the most specific handler wins. A subclass inherits its parents'
mappings and can override them.

The base controller ships default mappings: `X::MVC::Keayl::NotFound` becomes a
404, and `X::MVC::Keayl::ParameterMissing` and
`X::MVC::Keayl::UnpermittedParameters` become a 400. An exception with no matching
handler propagates.

## Helpers and shared state

`helper-method` exposes a controller method to the template under its own name:

```perl6
class UsersController is MVC::Keayl::Controller {
  method current-user { ... }
}

UsersController.helper-method('current-user');
```

Or mark the method with the `is helper-method` trait:

```perl6
class UsersController is MVC::Keayl::Controller {
  method current-user is helper-method { ... }
}
```

`assign` records a value for the template:

```perl6
method show {
  self.assign('title', 'Profile');
  self.render('show');
}
```

When an action renders, the template locals are built from the helper-method
values, then the assigned values, then any explicit `locals` (which win). The
recorded assigns are also readable through `self.assigns`.

## The ApplicationController pattern

Put shared callbacks, rescue handlers, and helpers on a base controller, and
subclasses inherit them:

```perl6
class ApplicationController is MVC::Keayl::Controller {
  method authenticate { ... }
  method site-name { ... }
}
ApplicationController.before-action('authenticate');
ApplicationController.helper-method('site-name');

class UsersController is ApplicationController { ... }
```

Callbacks, `rescue-from` mappings, and `helper-method` declarations all collect
across the inheritance chain, and a subclass can add to or override what it
inherits.

Every class-level declaration shown here has an `is` trait form so it can be
written in the class header instead of a call after the class. The ones that
attach to a method (`before-action`, `after-action`, `around-action`,
`rescue-from`, `helper-method`) go on the method declaration; the ones that
configure the whole controller (`layout`, `filter-parameters`, `add-flash-types`,
`protect-from-forgery`, `rate-limit`, `wrap-parameters`,
`http-basic-authenticate-with`) go on the class:

```perl6
class UsersController is MVC::Keayl::Controller
  is layout('admin')
  is protect-from-forgery
{
  method authenticate is before-action(except => <index show>) { ... }
  method not-found($error) is rescue-from(X::MVC::Keayl::NotFound) { self.head(404) }
  method current-user is helper-method { ... }
}
```
