---
title: Git commands for pair programming
date: 2019-05-02
slug: git-commands-for-pair-programming
description: At my work I'm on an agile team and we do pair programming. Here are some git commands I find useful and use almost every day.
tags: [git]
archives: [2019-05]
---
At my work I'm on an agile team and we do pair programming. Here are some `git` commands I find useful and use almost every day:

We always make sure to branch off the latest main when creating new feature branches:

```bash
git checkout main && git pull
git checkout -b new-feature
```

At the end of the day, if we're not done with the new feature we create a "work in progress" WIP commit:

```bash
git add .
git commit -m 'WIP'
```

And push it up:

```bash
git push origin head
```

* As a rule we don't ever leave work on our development computers overnight.  
* If someone is out the next day someone else can pickup that work and continue on.  
* Pushing the code triggers a CI build showing any issues with specs as early as possible.

You can also use the feature branch name here:

```bash
git push origin new-feature
```

`"head"` is easier to remember at the end of the work day, so that's what I always use.

The next day we may find we're pairing with different people.

If I'm driving again then I will reset my feature branch back to before the WIP commit to continue on with development:

```bash
git reset head~1
```

If the other person was driving last I will checkout the branch they WIP'd and then reset the commit:

```bash
git checkout main && git pull
git checkout new-feature
git reset head~1
```

If I already have the branch checked out locally I'll do a hard reset to sync up with the contents of our remote branch:

```bash
git reset --hard origin/new-feature
```

When the development feature branch is complete I will rebase it against main before merging:

```bash
git rebase main
```

If I have more than one commit on the feature branch then I will rebase interactively and squash the commits:

```bash
git rebase -i main
```

Side note: Dash `(-)` is very useful with `git`. It means "the last branch I was on". So if I was on my new-feature branch and switched to main and then wanted to switch back I could do:

```bash
git checkout -
```

Finally, when I'm ready to merge my feature branch to main I use `--no-ff`:

```bash
git checkout main
git merge new-feature
```

This explicitly creates a merge commit.

I could also use dash here since my feature branch was likely the last branch I was on:

```bash
git merge -
```

The git-pair gem is very useful for pair programming:

[https://github.com/chrisk/git-pair](https://github.com/chrisk/git-pair)

It lets you commit as a pair:

```bash
git pair gd yc
git add . && git commit -m 'WIP'
```

A configured `.pairs` file might look something like this:

```bash
pairs:
    yc: Yungchih Chen; yunchih.chen
    gd: Greg Donald; greg.donald
email:
   domain: example.com
```

Happy pairing!
