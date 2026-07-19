---
title: Action Text
slug: action-text
order: 32
description: Action Text adds rich-text content to a model. Content is stored as sanitized HTML against an allowlist, edited through a Trix editor, rendered with...
toc: true
---
Action Text adds rich-text content to a model. Content is stored as sanitized
HTML against an allowlist, edited through a Trix editor, rendered with
safe-buffer output, and can embed Active Storage attachments.

## Rich-text content

A `MVC::Keayl::ActionText::Content` wraps a fragment of rich-text HTML.
`Content.from-html` sanitizes the input, keeping the rich-text allowlist of tags
and attributes and dropping everything else (scripts, event handlers, unknown
tags):

```perl6
use MVC::Keayl::ActionText;

my $content = MVC::Keayl::ActionText::Content.from-html(
  '<h1>Title</h1><script>evil()</script>'
);

$content.to-trix-html;   # '<h1>Title</h1>' as a safe string, for editing
$content.to-html;        # rendered html (attachments expanded), a safe string
$content.to-plain-text;  # 'Title'
$content.is-empty;       # False
```

`sanitize-rich-text($html)` runs the same allowlist on its own.

## Attaching rich text to a record

A model includes `MVC::Keayl::ActionText::RichTextable` and declares its
rich-text fields with `has-rich-text`:

```perl6
use MVC::Keayl::ActionText::RichTextable;

class Article does RichTextable {
  has $.id;
}
Article.has-rich-text('content');
```

Each declared name returns a proxy backed by a rich-text record:

```perl6
$article.content.assign('<h1>Hello</h1>');   # sanitizes and stores
$article.content.is-present;                  # True
$article.content.to-html;                     # the rendered html
$article.content.to-plain-text;               # 'Hello'
```

Records persist through a repository.
`MVC::Keayl::ActionText::Repository` defines the interface and
`MemoryRepository` is the in-process implementation. Configure an ORM-backed
repository at boot:

```perl6
set-rich-text-repository(MemoryRepository.new);
```

## Editing

`rich-text-area` on the form builder emits the Trix editor markup: a hidden
input that carries the value plus a `<trix-editor>` bound to it.

```perl6
use MVC::Keayl::Helpers::Form;

FormBuilder.new(object-name => 'post').rich-text-area('body');
```

```html
<input type="hidden" id="post_body_trix_input" name="post[body]" value="...">
<trix-editor input="post_body_trix_input" class="trix-content"></trix-editor>
```

`rich-text-area-tag(name, value?, %options)` renders the same markup without a
form builder.

## Rendering

The `rich-text` helper renders content for a view with safe-buffer output. It
accepts a `Content`, a rich-text proxy, or a raw HTML string (which it
sanitizes):

```perl6
rich-text($article.content);
rich-text('<p>hello</p>');
```

## Embedded attachments

An Active Storage blob can be embedded in the document as an
`<action-text-attachment>` element carrying a signed blob id. `embed-tag($blob)`
builds one:

```perl6
my $tag = embed-tag($blob);
# <action-text-attachment sgid="..." content-type="image/png" filename="photo.png"></action-text-attachment>
```

When the content renders, each attachment resolves its signed id to a blob and
expands into a `<figure>`. An image becomes a preview pointing at the proxy
serving path; any other file becomes a download link. An attachment whose signed
id no longer resolves is dropped.

```html
<figure class="attachment attachment--preview attachment--png">
  <img src="/keayl/blobs/proxy/..." alt="photo.png">
</figure>
```

`content.attachment-sgids` lists the signed ids embedded in a document.
