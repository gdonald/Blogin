---
title: Syntax Highlighting
date: 2026-07-15
order: 7
tags: [markdown]
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

Recognized languages are raku, ruby, python, javascript, typescript, bash, json,
c, cpp, java, go, and rust. An unrecognized language falls back to escaped plain
text and its code block is labeled with an `hl-plain` class, so a stylesheet can
tell a highlighted block from an unhighlighted one.

## Styling

Highlighting emits `hl-keyword`, `hl-string`, `hl-number`, and `hl-comment`
classes. The build emits `blogin.css` with a default color scheme for them, along
with base styling for code blocks and heading anchors. Link it from `base.haml`
and override any rule in your own stylesheet. Because the spans are plain HTML,
any color scheme is a matter of CSS.
