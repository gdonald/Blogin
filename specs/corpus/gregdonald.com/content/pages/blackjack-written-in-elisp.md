---
title: Blackjack written in Elisp
date: 2023-04-28
slug: blackjack-written-in-elisp
description: I wrote Blackjack in Elisp . It allows you to play Blackjack in your Emacs editor.
tags: [blackjack, elisp, emacs]
archives: [2023-04]
---
I wrote Blackjack in [Elisp](https://www.gnu.org/software/emacs/manual/elisp.html). It allows you to play Blackjack in your [Emacs](https://www.gnu.org/software/emacs/) editor.

[https://github.com/gdonald/blackjack-el](https://github.com/gdonald/blackjack-el)

You can install it from [MELPA](https://melpa.org/#/blackjack):

```bash
M-x package-install blackjack
```

[![Blackjack in Elisp](https://raw.githubusercontent.com/gdonald/blackjack-el/main/imgs/ss1.png){width=640}](https://raw.githubusercontent.com/gdonald/blackjack-el/main/imgs/ss1.png){target=_blank rel=noopener}

[![Blackjack in Elisp](https://raw.githubusercontent.com/gdonald/blackjack-el/main/imgs/ss2.png){width=640}](https://raw.githubusercontent.com/gdonald/blackjack-el/main/imgs/ss2.png){target=_blank rel=noopener}

`make test` and `make test-coverage` are available. Tests are written using [Buttercup](https://github.com/jorgenschaefer/emacs-buttercup). Test coverage is generated using [undercover](https://github.com/undercover-el/undercover.el). Both packages are available from MELPA.

You will need to install [Ruby](https://www.ruby-lang.org/) and [Simplecov](https://github.com/simplecov-ruby/simplecov) for test coverage to build.
