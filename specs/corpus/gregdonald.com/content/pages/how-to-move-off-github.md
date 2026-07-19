---
title: How To Move off GitHub
date: 2012-04-24
slug: how-to-move-off-github
description: If you want to leave Github for whatever reason, you probably want to take all your code with you, and all your history and branches, etc. Here's an example for how I moved my Android Eclipse...
tags: [git, github]
archives: [2012-04]
draft: true
---
If you want to leave Github for whatever reason, you probably want to take all your code with you, and all your history and branches, etc.

Here's an example for how I moved my Android Eclipse workspace from Github to my own remote server.

First I make a new git repo on my remote server:

```bash
$ git --bare init ~/git/workspace
```

The ***--bare*** option means I'm not going to work on the code in the remote git repo directly.

Next I push my current local 'master' to the new repo:

```bash
$ git checkout master
$ git push ssh://me@myserver.com/git/workspace master
```

After that I push my working branch:

```bash
$ git checkout work
$ git push ssh://me@myserver.com/git/workspace work
```

You could repeat this step if you have more branches.

I created my local repo using 'clone' so it has an 'origin' remote branch defined. This 'remote' branch is where git fetches and pushes changes.

Right now my 'fetch' and 'push' remote origins point to Github:

```bash
$ git remote -v
git@github.com:gdonald/workspace.git (fetch) origin
git@github.com:gdonald/workspace.git (push)
```

So I remove my 'origin' and add the new one:

```bash
$ git remote rm origin
$ git remote add origin ssh://me@myserver.com/git/workspace
```

Then I update my local master's 'remote' to the new 'origin':

```bash
$ git config branch.master.remote origin
$ git config branch.master.merge refs/heads/master
```

After this change I can automatically push my commits to the new repo, 'origin' is now selected as my default 'remote' for 'master' :)

```bash
$ git remote -v
ssh://me@myserver.com/git/workspace (fetch) origin
ssh://me@myserver.com/git/workspace (push)
``` ```bash
$ git push
$ git pull
```
