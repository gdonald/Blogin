---
title: Customize New zsh
date: 2021-09-12
slug: customize-new-zsh
description: Install packages: apt install zsh curl git fzf build-essential libssl-dev zlib1g-dev Switch default shell to zsh: chsh -s `which zsh`
tags: [linux, ohmyzsh, zsh]
archives: [2021-09]
---
Install packages:

```bash
apt install zsh curl git fzf build-essential libssl-dev zlib1g-dev
```

Switch default shell to zsh:

```bash
chsh -s `which zsh`
```

Log out, log back in, and verify:

```bash
echo $SHELL
/usr/bin/zsh
```

Install Oh My Zsh:

```bash
sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"
```

Install POWERLEVEL10K:

```bash
git clone --depth=1 https://github.com/romkatv/powerlevel10k.git \
    ${ZSH_CUSTOM:-$HOME/.oh-my-zsh/custom}/themes/powerlevel10k
```

Select new theme in ~/.zshrc:

```bash
ZSH_THEME="powerlevel10k/powerlevel10k"
```

Sorry robbyrussell :(

Download and install fonts for POWERLEVEL10K:

```bash
wget \
https://github.com/romkatv/powerlevel10k-media/raw/master/MesloLGS%20NF%20Regular.ttf \
https://github.com/romkatv/powerlevel10k-media/raw/master/MesloLGS%20NF%20Bold.ttf \
https://github.com/romkatv/powerlevel10k-media/raw/master/MesloLGS%20NF%20Italic.ttf \
https://github.com/romkatv/powerlevel10k-media/raw/master/MesloLGS%20NF%20Bold%20Italic.ttf
```

Install zsh-autosuggestions:

```bash
git clone https://github.com/zsh-users/zsh-autosuggestions \
    ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions
```

Configure zsh plugins:

```bash
plugins=(
    git
    bundler
    dotenv
    osx
    ruby
    zsh-autosuggestions
    fzf
)
```

Install lsd:

```bash
wget https://github.com/Peltoche/lsd/releases/download/0.20.1/lsd_0.20.1_amd64.deb
dpkg -i lsd*
```

Check for new version: [lsd releases](https://github.com/Peltoche/lsd/releases)

Add aliases and export PATH in ~/.zshrc

```bash
setopt no_share_history

alias ..='cd ..'
alias ...='cd ../..'
alias be='bundle exec'
alias cp='cp -i'
alias ll='lsd --all --long'
alias ls='ls -ahG'

EDITOR="/usr/bin/vim"

PATH="/bin:/sbin"
PATH="/usr/bin:/usr/sbin:$PATH"
PATH="/usr/local/bin:/usr/local/sbin:$PATH"
PATH="$HOME/bin:$PATH"

export EDITOR PATH
```
