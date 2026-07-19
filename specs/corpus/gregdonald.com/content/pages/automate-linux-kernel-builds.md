---
title: Automate Linux Kernel Builds
date: 2024-02-25
slug: automate-linux-kernel-builds
description: I wrote a small shell script™ to automate building new Linux Kernels. I use this to build new kernels on my Debian systems: #!/usr/bin/env bash WORKSPACE='/home/gd/workspace'...
tags: [bash, kernel, linux]
archives: [2024-02]
---
I wrote a small shell script™ to automate building new Linux kernels. I use this to build new kernels on my Debian systems:

```bash
#!/usr/bin/env bash

WORKSPACE='/home/gd/workspace'
LINUX="${WORKSPACE}/linux"

find "${WORKSPACE}" -type f \( -name 'linux-*deb' -o -name 'linux-upstream*' \) -mtime -3 -exec rm {} \;

cd "${LINUX}" || exit

git checkout .
git checkout master --force

git branch | while read -r branch; do
    [[ "$branch" =~ ^v ]] && git branch -D "${branch}"
done

git pull

version=$(git tag | sort -V | grep -v -E '*-rc*' 2>/dev/null | tail -1)
git checkout tags/"${version}" -b "${version}"

make mrproper
yes "" | make localmodconfig
make -j"$(nproc --all)" bindeb-pkg
```

1. First I setup two constant values, `WORKSPACE` and `LINUX`, to store paths to my workspace and my Linux source code directory, respectively.
2. Next I delete old files. I use the `find` command to search for files in my `WORKSPACE` that match the specific patterns of old builds (`linux-*deb` and `linux-upstream*`) and that are older than 3 days, then I delete them. This helps with keeping my filesystem cleared of old builds and package files.
3. Then I change the current directory to `LINUX`. If the directory does not exist or the command fails, my script exits to prevent further commands from executing in the wrong directory.
4. After than I reset and update the Linux kernel git repository:   `git checkout .` discards changes in the working directory.   `git checkout master --force` switches to the master branch, discarding any changes or differences forcefully.   I then delete all git branches starting with "v". These are from previous builds and are no longer needed.   `git pull` updates my local repository to the latest commit.
5. Next I select the latest stable version:  I find the latest git tag that does not include a release candidate (`-rc`) version and set that as the current version. Then I check out this version into a new branch, naming it after the version tag.
6. Finally I compile the Linux kernel:   `make mrproper` cleans my local source tree, removing any previous configurations and compiled files.   `make defconfig` creates a default configuration file. I do this so I'm not asked about any new defaults when I run the next command.   `make localmodconfig` tailors my configuration for my exact system and also disables any modules that are not needed.   `make -j"$(nproc --all)" bindeb-pkg` compiles my kernel into Debian packages using all available processor cores, speeding up the compilation process.
