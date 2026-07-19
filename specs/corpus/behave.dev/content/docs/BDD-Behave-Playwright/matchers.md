---
title: Matchers
slug: matchers
order: 5
description: The matchers assert on WWW::Playwright Locator and Page objects. Apply them method-style on an expectation:
toc: true
---
The matchers assert on WWW::Playwright `Locator` and `Page` objects. Apply them
method-style on an expectation:

```raku
expect(.page.locator('#greeting')).to.be-visible;
expect(.page.locator('#name')).to.have-value('Ada');
```

Every matcher negates with `.not`:

```raku
expect(.page.locator('#status')).not.to.be-visible;
```

## Auto-waiting

Matchers poll until the condition holds or a timeout elapses, so an element that
renders shortly after an action still passes:

```raku
.page.locator('#go-async').click;
expect(.page.locator('#async-status')).to.be-visible;
```

The default timeout is 4 seconds. Override it per assertion with `timeout`
(seconds), which is useful to fail fast in negative cases:

```raku
expect(.page.locator('#status')).not.to.be-visible(timeout => 0.3);
```

## Presence and state

These operate on a `Locator`.

- `be-visible` — the element is visible.
- `be-hidden` — the element is present but not visible.
- `be-enabled` — the element is enabled.
- `be-disabled` — the element is disabled.
- `be-checked` — the checkbox or radio is checked.

```raku
expect(.page.locator('#greeting')).to.be-visible;
expect(.page.locator('#locked')).to.be-disabled;
expect(.page.locator('#agree')).to.be-checked;
```

## Content

These operate on a `Locator`.

- `have-text($expected)` — the element's text contains `$expected` (a string
  substring, or a `Regex` to match).
- `have-value($expected)` — the form control's value equals `$expected` (a
  string), or matches a `Regex`.
- `have-attribute($name)` — the attribute is present.
- `have-attribute($name, $expected)` — the attribute equals `$expected`, or
  matches a `Regex`.
- `have-count($n)` — the locator resolves to exactly `$n` elements.

```raku
expect(.page.locator('#greeting')).to.have-text('Hello, world');
expect(.page.locator('#name')).to.have-value('Ada');
expect(.page.locator('#name')).to.have-attribute('type', 'text');
expect(.page.locator('button')).to.have-count(2);
```

## Selector existence

`have-css` operates on a `Page` or a scoping `Locator`. It asserts the subject
contains nodes matching a CSS selector, mirroring Capybara's `have_css`.

- `have-css($selector)` — at least one match.
- `have-css($selector, count => $n)` — exactly `$n` matches.
- `have-css($selector, minimum => $n)` — at least `$n` matches.
- `have-css($selector, maximum => $n)` — at most `$n` matches.
- `have-css($selector, text => $string)` — restrict to nodes containing `$string`.

```raku
expect(.page).to.have-css('#greeting');
expect(.page).to.have-css('button', count => 2);
expect(.page).to.have-css('li', minimum => 3);
```

## Page-level

These operate on a `Page`.

- `have-title($expected)` — the document title contains `$expected` (a string
  substring, or a `Regex` to match).
- `have-current-path($expected)` — the path of the current URL equals `$expected`
  (a string), or matches a `Regex`.
- `have-current-path($expected, url => True)` — match against the full URL rather
  than just the path.

```raku
expect(.page).to.have-title('Hello');
expect(.page).to.have-current-path('/app/users');
expect(.page).to.have-current-path(rx/'/users/' \d+/);
expect(.page).to.have-current-path('https://example.test/users', url => True);
```
