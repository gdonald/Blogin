---
title: Actions
slug: actions
order: 4
description: Actions run against a Locator. Get one from $page.locator($selector), then call an action on it. Playwright auto-waits for the element to be actionable...
toc: true
---
Actions run against a `Locator`. Get one from `$page.locator($selector)`, then
call an action on it. Playwright auto-waits for the element to be actionable
before each action.

```raku
my $page = $context.new-page;
$page.goto('file:///path/to/hello.html');
```

## `click`

Clicks the element.

```raku
$page.locator('#go').click;
```

## `fill`

Sets the value of an input, replacing any existing value.

```raku
$page.locator('#name').fill('Ada');
```

## `type`

Types text one key at a time, appending to the current value. Use `fill` to
replace; use `type` when per-keystroke events matter.

```raku
$page.locator('#name').type('Hi');
```

## `press`

Dispatches a single key.

```raku
$page.locator('#name').press('Enter');
```

## `check` and `uncheck`

Tick or clear a checkbox.

```raku
$page.locator('#agree').check;
$page.locator('#agree').uncheck;
```

## `select-option`

Selects an option in a `<select>` by value and returns the chosen values.

```raku
my @selected = $page.locator('#color').select-option('green');   # ('green',)
```

## `hover`

Moves the pointer over the element.

```raku
$page.locator('#hoverable').hover;
```
