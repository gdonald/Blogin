---
title: Syntax Highlighting
date: 2026-07-15
tags: [authoring]
description: Server-side highlighting for fenced code.
---
Set `"highlight": true` in `blogin.json` and Blogin highlights fenced code during
the build. Highlighting is server-side: there is no client JavaScript, and the
pages work without it.

A fenced block's info string names the language:

````
```raku
my $x = 1;  # a comment
```
````

The highlighter tokenizes the code into spans, escaping the source:

```raku
my $greeting = "hello";  # keywords, strings, numbers, comments
```

Recognized languages are raku, ruby, python, javascript, bash, and json. An
unrecognized language falls back to escaped plain text with no spans.

## Styling

Highlighting emits `hl-keyword`, `hl-string`, `hl-number`, and `hl-comment`
classes. The scaffold's `static/style.css` colors them; change those rules to
restyle. Because the spans are plain HTML, any color scheme is a matter of CSS.
