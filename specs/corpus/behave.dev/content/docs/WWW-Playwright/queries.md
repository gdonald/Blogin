---
title: Queries
slug: queries
order: 5
description: Queries read state rather than mutating the page. Most run against a Locator; url and title run against the Page.
toc: true
---
Queries read state rather than mutating the page. Most run against a `Locator`;
`url` and `title` run against the `Page`.

## `url` (Page)

The current page URL.

```raku
$page.url;        # 'file:///path/to/hello.html'
```

## `title` (Page)

The document title.

```raku
$page.title;      # 'Hello'
```

## `text-content`

The element's `textContent`.

```raku
$page.locator('#greeting').text-content;   # 'Hello, world'
```

## `inner-text`

The rendered `innerText`, reflecting visibility and CSS.

```raku
$page.locator('#greeting').inner-text;      # 'Hello, world'
```

## `get-attribute`

The value of a named attribute, or the type object when absent.

```raku
$page.locator('#name').get-attribute('type');   # 'text'
```

## `input-value`

The current value of an input, textarea, or select.

```raku
$page.locator('#name').input-value;
```

## `count`

The number of elements the locator matches.

```raku
$page.locator('#color option').count;       # 3
```

## `is-visible`, `is-enabled`, `is-checked`

Boolean state checks.

```raku
$page.locator('#greeting').is-visible;       # True
$page.locator('#name').is-enabled;           # True
$page.locator('#agree').is-checked;          # False until checked
```

## `wait-for`

Waits until the element reaches a state: `attached`, `detached`, `visible`, or
`hidden`.

```raku
$page.locator('#greeting').wait-for(state => 'visible');
```

## Chaining

`locator` chains from a page or another locator, scoping the selector to the
parent.

```raku
$page.locator('body').locator('#greeting').text-content;
```
