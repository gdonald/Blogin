---
title: Overview
slug: overview
order: 1
description: The latest version of this documentation lives at https://gdonald.github.io/Template-HAML/.
toc: true
---
The latest version of this documentation lives at [https://gdonald.github.io/Template-HAML/](https://gdonald.github.io/Template-HAML/).

The homepage for Template::HAML is [https://github.com/gdonald/Template-HAML](https://github.com/gdonald/Template-HAML).

## Synopsis

Template::HAML is an HTML Abstraction Markup Language (HAML) implementation for [Raku](https://raku.org/), built on Raku Grammars. HAML lets you write HTML as an indentation-driven outline rather than as a sea of opening and closing tags.

Currently developed against Raku `v6.d`.

## Example

`example.haml`

```haml
%html
  %head
    %title Page Title
  %body
    %section.container
      %h1 Title
      %h2 Subtitle
      %p Content
      %ul
        %li Foo
        %li Bar
```

```raku
use Template::HAML;

my $html = HAML.render(:src(slurp 'example.haml'));
say $html;
```

Output:

```html
<html>
  <head>
    <title>Page Title</title>
  </head>
  <body>
    <section class='container'>
      <h1>Title</h1>
      <h2>Subtitle</h2>
      <p>Content</p>
      <ul>
        <li>Foo</li>
        <li>Bar</li>
      </ul>
    </section>
  </body>
</html>
```

## Install

Template::HAML can be installed using the zef module installation tool:

```shell
zef install Template::HAML
```
