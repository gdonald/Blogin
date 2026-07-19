---
title: LearnRaku.dev is live
date: 2026-07-21
slug: learnraku-dev-is-live
tags: [raku, tdd, test, spec]
archives: [2026-07]
---
I just released [LearnRaku.dev](https://learnraku.dev) into production. It is an interactive, browser-based trainer for learning [Raku](https://raku.org). You get an exercise, a failing spec, and an editor. You write code until the spec passes, and your progress is recorded.

I built it this way because I am a strong proponent of test-driven development, enough that I wrote [a book](https://tddbook.com) about it. TDD is the practice of writing a failing test first, then writing just enough code to make it pass, then cleaning up. The failing test comes before the code and drives it. That red-to-green cycle is the entire model behind LearnRaku.dev: every exercise hands you a failing spec, and your job is to write the code that turns it green. You are not running code and reading the output, you are making an expectation pass. The one difference from writing your own tests is that here the spec is already written for you, so you spend your time on the code the test is asking for.

Each exercise covers a single idea: scalars, numbers, strings, arrays, hashes, junctions, modules, and on up. There is a short description, a spec to satisfy, and a skeleton to fill in. When you pass, the exercise is marked done and the site keeps the solution you wrote. Your edits autosave as a draft per exercise, so you can leave an exercise half-finished, work on another, and come back to exactly what you had typed.

Without an account, that progress and those drafts live only in the browser you are using. Sign in and your work is saved to your account instead, so the exercises you complete and the drafts you write follow you across browsers and machines.

Each exercise also has a reference solution, hidden at first. It unlocks once you pass, and it also unlocks after a few failed Check runs, so you can compare your working answer against a known one or get unstuck without leaving the page.

Each exercise starts you off with a code scaffold: a module with the sub or class already declared and a marker where your code goes. You are never staring at an empty file. The scaffold names the thing you are implementing and its signature, so you can focus on the logic the spec is checking rather than the boilerplate around it. It loads like this:

```perl
unit module Exercise;

sub greeting($name) is export {
  # your code here
}
```

And this is the spec you are trying to make pass, written in [BDD::Behave](https://behave.dev):

```perl
use Exercise;

describe 'scalars and string interpolation', {
  it 'builds a greeting from a name', {
    expect(greeting('Raku')).to.be('Hello, Raku!');
  }
}
```

Fill in the body until the expect is satisfied. Press Check and the results come back example by example, pass or fail, with the expected and actual values shown when something is off.

The exercises are open source. They live in their own repository, [learnraku-exercises](https://github.com/gdonald/learnraku-exercises), separate from the site code. Each one is three small files: a README with the description, the skeleton you edit, and the spec that grades it. If you spot a typo, want to sharpen a description, or have an idea for a new exercise, fork the repo, run the checker locally against your changes, and open a pull request.

Alongside the exercises there is a Playground. When you do not want a spec and just want to try something, it is a blank editor where you write any Raku you like and run it. The output comes back: standard out, standard error, and the exit code. No exercise, no grading, no account required.

```perl
say "Hello, Raku!";

say (1..10).grep(* %% 2).sum;
```

Press Run and you get:

```text
Hello, Raku!
30
```

Your playground code is kept between visits, so you can close the tab and come back to it.

Some smaller features round it out. There is a Top Rakuists leaderboard you can opt into, ranking people by how many exercises they have completed and how recently. Clicking the logo jumps to your next unfinished exercise. Editing autosaves as you type. The site is a single-page app with a code editor, syntax highlighting for Raku, and a light and dark theme.

It is Raku running Raku. The site is built on [Keayl](https://keayl.dev), a Raku web framework, and the specs run with [Behave](https://behave.dev), a Raku testing library.

If you want to pick up Raku, or you already know it and want to help teach it, try [LearnRaku.dev](https://learnraku.dev). If you find a rough edge in an exercise, send a pull request.
