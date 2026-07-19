---
title: Locator
slug: locator
order: 10
description: A WWW::Playwright::Locator points at one or more elements matched by a selector. Get one from Page.locator or by chaining from another locator. Playwright...
toc: true
---
A `WWW::Playwright::Locator` points at one or more elements matched by a selector.
Get one from [`Page.locator`](/docs/WWW-Playwright/page) or by chaining from another locator.
Playwright auto-waits for the element before each action.

## Chaining

## `locator(Str $selector --> Locator)`

Returns a locator scoped to the matches of this one.

```raku
$page.locator('body').locator('#greeting');
```

## Actions

| Method                            | Effect                                      |
|-----------------------------------|---------------------------------------------|
| `click(--> Nil)`                  | Clicks the element.                         |
| `fill(Str $value --> Nil)`        | Replaces an input's value.                  |
| `type(Str $text --> Nil)`         | Types text one key at a time.               |
| `press(Str $key --> Nil)`         | Dispatches a single key.                    |
| `check(--> Nil)`                  | Ticks a checkbox.                           |
| `uncheck(--> Nil)`                | Clears a checkbox.                          |
| `select-option(Str $value --> List)` | Selects an option, returns chosen values. |
| `hover(--> Nil)`                  | Moves the pointer over the element.         |

See the [Action reference](/docs/WWW-Playwright/actions) for examples.

## Queries

| Method                                | Returns                                  |
|---------------------------------------|------------------------------------------|
| `text-content(--> Str)`               | The element's `textContent`.             |
| `inner-text(--> Str)`                 | The rendered `innerText`.                |
| `get-attribute(Str $name)`            | A named attribute's value.               |
| `input-value(--> Str)`                | The current input/select value.          |
| `count(--> Int)`                      | The number of matched elements.          |
| `is-visible(--> Bool)`                | Whether the element is visible.          |
| `is-enabled(--> Bool)`                | Whether the element is enabled.          |
| `is-checked(--> Bool)`                | Whether a checkbox is ticked.            |

See the [Query reference](/docs/WWW-Playwright/queries) for examples.

## Auto-waiting

## `wait-for(Str :$state --> Nil)`

Waits until the element reaches a state: `attached`, `detached`, `visible`, or
`hidden`.

```raku
$page.locator('#greeting').wait-for(state => 'visible');
```
