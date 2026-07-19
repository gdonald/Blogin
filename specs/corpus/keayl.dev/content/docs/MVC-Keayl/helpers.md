---
title: View helpers
slug: helpers
order: 12
description: View helpers build HTML. They return SafeString values, so their markup is not re-escaped when emitted with !=. In templates they are called as bare...
toc: true
---
View helpers build HTML. They return `SafeString` values, so their markup is not
re-escaped when emitted with `!=`. In templates they are called as bare functions
through the view context (`link-to`, `image-tag`, and so on), with arguments and
no sigil.

## Tag building

`content-tag` builds an element with content; `tag` builds a self-closing
element. Attribute values are escaped, a `True` value becomes a bare boolean
attribute, and a `False` or undefined value is omitted:

```perl6
content-tag('p', 'hi', %( class => 'lead' ));        # <p class="lead">hi</p>
tag('input', %( type => 'text', disabled => True )); # <input disabled type="text" />
```

String content is escaped; a `SafeString` is emitted as-is.

### Nested data and aria attributes

A `data` or `aria` value that is a hash expands to dasherized attributes, and a
structured value is JSON encoded:

```perl6
content-tag('div', 'x', %( data => %( user_id => 5, toggle => 'modal' ) ));
# <div data-toggle="modal" data-user-id="5">x</div>

content-tag('div', 'x', %( data => %( ids => [1, 2] ) ));
# <div data-ids="[1,2]">x</div>
```

`data-attributes` builds the same dasherized keys as a hash for merging into
other attributes.

### Class helper

A `class` attribute accepts an array of tokens or a hash of conditional tokens,
and `class-names` builds the same token list on its own:

```perl6
tag('span', %( class => ['a', 'b'] ));                       # class="a b"
content-tag('div', 'x', %( class => %( active => $on ) ));   # class="active" when $on

class-names('btn', %( active => True, disabled => False ));  # "btn active"
class-names(['a', 'b'], 'a', 'c');                           # "a b c"
```

Falsy and duplicate tokens are dropped, and an empty class attribute is omitted.

### Script, time, and feed tags

`javascript-tag` wraps script content (emitted unescaped) in a `<script>`
element, `time-tag` builds a `<time>` element with a `datetime` attribute from a
date, and `auto-discovery-link-tag` builds a feed `<link>` for `atom`, `rss`, or
`json`:

```perl6
javascript-tag('alert(1)');                  # <script>alert(1)</script>
time-tag(Date.new(2020, 1, 1), 'New Year');  # <time datetime="2020-01-01">New Year</time>
auto-discovery-link-tag('atom', '/feed.atom');
```

`atom-feed` builds an Atom feed document. The feed builder takes `title`,
`updated`, and `id`, and `entry` yields an entry builder with `title`,
`content`, `updated`, `author`, and `link`:

```perl6
atom-feed(url => 'http://example.com', content => -> $feed {
  $feed.title('Posts');
  $feed.entry($post, url => '/posts/1', block => -> $entry {
    $entry.title($post.title);
    $entry.content($post.body, type => 'html');
  });
});
```

## URL helpers

`url-for` turns a target into a URL string. It passes a string through and reads
`path` from a hash. `link-to` builds an anchor, and `button-to` builds a small
form that posts to a URL, adding a `_method` override for other verbs:

```perl6
link-to('Posts', '/posts', %( class => 'nav' ));   # <a class="nav" href="/posts">Posts</a>
link-to('/only');                                   # <a href="/only">/only</a>
button-to('Delete', '/posts/1', %( method => 'delete' ));
```

## Asset helpers

`asset-path` resolves a source to a URL. A bare source is prefixed with
`/assets/`; an absolute path or external URL is left alone; a `:type` adds an
extension when the source has none:

```perl6
asset-path('logo.png');           # /assets/logo.png
asset-path('app', :type('css'));  # /assets/app.css
```

`image-tag`, `stylesheet-link-tag`, and `javascript-include-tag` build the
matching elements, resolving their sources through `asset-path`. `image-tag`
derives a default `alt` from the filename, and the link and script helpers accept
multiple sources:

```perl6
image-tag('logo.png');                       # <img alt="Logo" src="/assets/logo.png" />
stylesheet-link-tag('app', 'admin');
javascript-include-tag('app');
```

### Pluggable resolver

`asset-path` and the asset tags accept a `resolver`, a routine of
`($source, $type)` that returns the final URL. It is the seam for fingerprinting
or a CDN host. A `MVC::Keayl::View` carries an `asset-resolver` that its asset
helpers use:

```perl6
my $view = MVC::Keayl::View.new(
  :paths(['app/views']),
  asset-resolver => -> $source, $type { 'https://cdn.example/' ~ $source },
);
```

## Forms

`form-with` builds a form around a `FormBuilder`. The builder is passed to a
`content` callback and scopes field names to the model (or an explicit `scope`),
prefills values, and annotates errors. The form posts by default, adding a
`_method` override for other verbs and an `authenticity_token` field when a
`csrf-token` is given:

```perl6
form-with(
  model      => $post,
  url        => '/posts/1',
  method     => 'patch',
  csrf-token => $token,
  content    => -> $f {
    $f.text-field('title') ~ $f.submit('Save')
  },
);
```

### Field helpers

The builder provides `text-field`, `password-field`, `hidden-field`,
`text-area`, `check-box`, `radio-button`, `select`, `label`, `submit`, and
`button`. Field names are scoped (`post[title]`) and ids derived
(`post_title`). A `check-box` renders a hidden companion for its unchecked
value, a `select` marks the option matching the model value, and a
`password-field` never emits a value. `fields-for` builds a nested builder whose
names are scoped under the parent:

```perl6
$form.fields-for('author', block => -> $author {
  $author.text-field('name')   # name="post[author][name]"
});
```

#### HTML5 typed inputs

The builder also has the HTML5 typed inputs, each setting its `type` and
prefilling from the model like `text-field`: `email-field`, `url-field`,
`telephone-field` (`type="tel"`), `number-field`, `range-field`, `search-field`,
`color-field`, `date-field`, `time-field`, `datetime-field`
(`type="datetime-local"`), `month-field`, and `week-field`:

```perl6
$form.email-field('email');          # type="email"
$form.number-field('rank');          # type="number"
$form.date-field('born');            # type="date"
```

`file-field` builds a file input. A `multiple` option appends `[]` to the name
and adds the `multiple` attribute, and a `direct-upload` option sets a
`data-direct-upload-url`:

```perl6
$form.file-field('avatar');
$form.file-field('photos', %( multiple => True ));        # name="post[photos][]"
$form.file-field('photo', %( direct-upload => '/uploads' ));
```

Outside a builder, `button-tag` builds a `<button type="submit">`, and
`image-submit-tag` builds an image submit input that resolves its source through
`asset-path`:

```perl6
button-tag('Save');                  # <button type="submit">Save</button>
image-submit-tag('search.png');      # <input alt="Search" src="/assets/search.png" type="image" />
```

### Select and option helpers

`options-for-select` builds `<option>` tags from a list of strings, label-value
pairs, or `[label, value, attrs]` triples, marking the selected value.
`options-from-collection-for-select` reads value and text methods off each
member of a collection, and `grouped-options-for-select` wraps choices in
`<optgroup>` elements. `select-tag` wraps option markup in a `<select>`, with
`multiple`, `include-blank`, and `prompt` options:

```perl6
options-for-select(['a', 'b'], 'b');           # <option value="a">a</option><option selected value="b">b</option>
options-from-collection-for-select(@cities, 'id', 'name', $selected);
grouped-options-for-select(['North' => ['NY', 'NJ']]);
select-tag('city', options-for-select(['NY', 'LA']), %( include-blank => True ));
```

`time-zone-select` builds a select from a built-in zone list. The date-part
helpers `select-year`, `select-month` (with month names), `select-day`,
`select-hour`, `select-minute`, and `select-second` build a single select each,
and `select-date` and `select-time` combine them. They name their fields under a
`date` prefix by default:

```perl6
select-year(2020, start-year => 2018, end-year => 2022);
select-month(3);                     # marks March
select-date(Date.new(2020, 3, 15));  # year, month, and day selects
```

On the builder, `collection-select`, `collection-radio-buttons`, and
`collection-check-boxes` build a select, a radio set, or a checkbox set from a
collection, reading value and text methods and marking the model value.
`date-select`, `time-select`, and `datetime-select` build multiparameter selects
(`post[born](1i)`, `(2i)`, `(3i)`) prefilled from a model date or time:

```perl6
$form.collection-select('city-id', @cities, 'id', 'name');
$form.collection-check-boxes('tag-ids', @tags, 'id', 'name');
$form.date-select('born');
```

### Model awareness

When the builder has a model, fields prefill from it (calling the
attribute-named method), an explicit `value` overrides the model, and a field
whose attribute reports errors gets a `field-with-errors` class. The builder
reads errors through the model's `errors-on($attribute)` method, and
`errors-for` renders the messages.

## simple_form-style inputs

`SimpleFormBuilder` adds an `input` method that assembles a label, control, hint,
and error into a wrapper, inferring the control type. `simple-form-for` wraps it
in a form like `form-with`:

```perl6
simple-form-for($post, url => '/posts', required => ['title'], content => -> $f {
  $f.input('title', hint => 'Keep it short')
    ~ $f.input('body')          # textarea, inferred
    ~ $f.input('published')     # checkbox, inferred from a boolean value
    ~ $f.input('state', as => 'select', collection => ['draft', 'live'])
});
```

The type is taken from `as`, then the attribute name (`password`, `email`, the
long-text names `body`/`content`/`description`/`notes`), then a boolean model
value, defaulting to a string input. A required attribute marks the wrapper and
adds a `*` to the label, and an attribute with errors marks the wrapper and
appends the messages.

## Formatting helpers

### Text

`truncate` cuts a string to a length (counting the omission) and can break at a
separator. `pluralize` pairs a count with a singular or pluralized word, using a
small inflector with irregulars and an explicit `plural` override.
`simple-format` wraps text in paragraphs, turning blank lines into new paragraphs
and single newlines into `<br />`, escaping the content:

```perl6
truncate('This is a long sentence', length => 12);   # This is a...
pluralize(2, 'person');                               # 2 people
simple-format("Para one\n\nPara two");                # <p>Para one</p>\n<p>Para two</p>
```

`highlight` wraps matches of one or more phrases (case-insensitively, keeping the
original text) using a `highlighter` template where `\1` is the match, escaping
the surrounding text. `excerpt` extracts a window of text around a phrase with
omission markers, and `word-wrap` breaks lines at a width:

```perl6
highlight('You searched for tiger', 'tiger');         # You searched for <mark>tiger</mark>
excerpt('This is a beautiful morning', 'beautiful', radius => 5);   # ...is a beautiful morn...
word-wrap('The quick brown fox', line-width => 10);   # "The quick\nbrown fox"
```

`strip-tags` removes every tag, `strip-links` removes anchors but keeps their
text, and `sanitize-css` drops style declarations whose values contain unsafe
tokens (`javascript:`, `expression`, `@import`, `behavior`, `-moz-binding`):

```perl6
strip-tags('<p>Hello <b>world</b></p>');              # Hello world
strip-links('Visit <a href="/x">here</a> now');       # Visit here now
sanitize-css('color: red; background: url(javascript:alert(1))');   # color: red
```

`cycle` rotates through a set of values across calls (keyed by an optional
`name`), `current-cycle` reports the last value, and `reset-cycle` restarts it.
`capture` returns a block's output as a safe string, and `provide` accumulates
content under a name for a layout `yield`:

```perl6
cycle('odd', 'even');     # "odd", then "even", then "odd" on the next calls
current-cycle;            # the last value cycle returned
reset-cycle;              # the next cycle starts over
capture(-> { content-tag('p', 'hi') });   # <p>hi</p>
provide('head', stylesheet-link-tag('app'));
```

### Numbers

```perl6
number-with-delimiter(1234567);     # 1,234,567
number-to-currency(1234.5);         # $1,234.50
number-to-percentage(66.666, precision => 1);   # 66.7%
number-to-human-size(1536);         # 1.5 KB
```

`number-to-currency` accepts `unit`, `precision`, `delimiter`, `separator`, and a
`format` string (`%u` for the unit, `%n` for the number).

`number-to-phone` groups a phone number, with `area-code`, `delimiter`,
`country-code`, and `extension` options. `number-to-human` scales a number to a
word (Thousand, Million, Billion, Trillion, Quadrillion) at a significant-digit
`precision`:

```perl6
number-to-phone(1235551234);                     # 123-555-1234
number-to-phone(1235551234, area-code => True);  # (123) 555-1234
number-to-human(1234);                            # 1.23 Thousand
number-to-human(12345678);                        # 12.3 Million
```

### Date and time

`distance-of-time-in-words` describes the gap between two times in words, with an
optional `include-seconds` for finer detail under a minute. `time-ago-in-words`
is the same measured against the current time (or an explicit reference):

```perl6
distance-of-time-in-words($from, $to);                 # about 5 hours
distance-of-time-in-words($from, $to, include-seconds => True);   # less than 5 seconds
time-ago-in-words($posted-at);
```

## Helper modules

Beyond the built-in helpers, an application defines its own helpers as `our sub`s
in modules under `app/helpers/`. Each sub becomes a bare template call.

`ApplicationHelper` is global, available in every view:

```perl6
# app/helpers/ApplicationHelper.rakumod
unit module ApplicationHelper;

our sub nav-link($label, $href) {
  qq[<a href="$href">{$label}</a>]
}
```

```haml
%nav
  != nav-link('Home', '/')
```

A per-controller helper named after the controller (`UsersController` →
`app/helpers/UsersHelper.rakumod`) is available only in that controller's views,
and the chain follows controller inheritance: a controller sees its own helper,
its ancestors' helpers, and `ApplicationHelper`.

A helper is a plain function. For request state, read `$*KEAYL-CONTROLLER`:

```perl6
our sub current-user-name {
  $*KEAYL-CONTROLLER.current-user.name
}
```

Helper modules reload on change in development (gated by the view's `reload`
flag), the same as templates. The `controller` and `scaffold` generators write a
matching helper module, and `keayl new` writes `ApplicationHelper`.
