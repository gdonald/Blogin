---
title: Indentation
slug: indentation
order: 5
description: HAML uses indentation to express nesting. A line that's indented further than the previous line becomes a child of that line; a line at the same indent is...
toc: true
---
HAML uses indentation to express nesting. A line that's indented further than the previous line becomes a child of that line; a line at the same indent is a sibling; a line at a shallower indent closes one or more parents.

## Two-space indent

Template::HAML currently expects **two-space** indentation:

```haml
%section.container
  %h1 Title
  %ul
    %li Foo
    %li Bar
```

```html
<section class='container'>
  <h1>Title</h1>
  <ul>
    <li>Foo</li>
    <li>Bar</li>
  </ul>
</section>
```

## Inconsistent indentation is rejected

An odd-spaced indent is rejected with `X::IllegalIndent`:

```haml
%section
   %h1 Title
```

```
Illegal indentation
```
