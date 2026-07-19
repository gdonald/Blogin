---
title: End to end
slug: end-to-end
order: 7
description: A complete example: load a form, fill it, submit it, and assert the result.
toc: true
---
A complete example: load a form, fill it, submit it, and assert the result.

The fixture is a small HTML form. The button writes a greeting into a result
node when clicked:

```html
<form id="signup">
  <input id="username" type="text" value="">

  <button id="submit" type="button"
    onclick="document.getElementById('welcome').textContent = 'Welcome, ' + document.getElementById('username').value;">
    Sign up
  </button>

  <p id="welcome"></p>
</form>
```

The spec drives it through `playwright-page` and the matchers:

```raku
use BDD::Behave;
use BDD::Behave::Playwright;

describe 'signing up', {
  playwright-page(fixture => 'specs/fixtures/form.html');

  it 'greets the user after the form is submitted', {
    page.locator('#username').fill('Ada');
    page.locator('#submit').click;

    expect(page.locator('#welcome')).to.have-text('Welcome, Ada');
  }
}
```

`playwright-page` opens a fresh page on the fixture for the example. The `page`
term (from `use BDD::Behave::Playwright`) returns that page. The action methods
(`fill`, `click`) come from WWW::Playwright's `Locator`; `have-text` polls until
the greeting appears, so there is no explicit wait.
