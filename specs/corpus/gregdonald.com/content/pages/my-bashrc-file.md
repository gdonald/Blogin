---
title: My bashrc file
date: 2012-05-20
slug: my-bashrc-file
description: alias ls='ls -ah --color=always' alias ll='ls -lavh --color=always' alias cp='cp -i' alias vi='/usr/bin/emacs' alias ..='cd ..' alias ...='cd ../..'
tags: [bash]
archives: [2012-05]
---
```bash
alias ll='ls -lavh --color=always'
alias cp='cp -i'
alias vi='/usr/bin/emacs'
alias ..='cd ..'
alias ...='cd ../..'

Black="\[\033[0;30m\]"
DarkGray="\[\033[1;30m\]"
LightGray="\[\033[0;37m\]"
White="\[\033[1;37m\]"
Blue="\[\033[0;34m\]"
LightBlue="\[\033[1;34m\]"
Green="\[\033[0;32m\]"
LightGreen="\[\033[1;32m\]"
Cyan="\[\033[0;36m\]"
LightCyan="\[\033[1;36m\]"
Red="\[\033[0;31m\]"
LightRed="\[\033[1;31m\]"
Purple="\[\033[0;35m\]"
LightPurple="\[\033[1;35m\]"
Brown="\[\033[0;33m\]"
Yellow="\[\033[1;33m\]"

if [ `/usr/bin/whoami` = 'root' ]
then
        # Do not set PS1 for dumb terminals
        if [ "linux" != 'dumb'  ] && [ -n "" ]
        then
                export PS1='\[\033[01;31m\]\h \[\033[01;34m\]\W $ \[\033[00m\]'
        fi
        UColor=$Red
else
        # Do not set PS1 for dumb terminals
        if [ "linux" != 'dumb'  ] && [ -n "" ]
        then
                export PS1='\[\033[01;32m\]\u@\h \[\033[01;34m\]\W $ \[\033[00m\]'
        fi
        UColor=$LightPurple
fi

PS1="\n$Yellow-$LightCyan+$LightBlue($LightGreen\D{%D %r}$LightBlue)"
PS1="$PS1\n$Yellow-$LightCyan+$LightBlue($UColor\u$LightCyan@$Purple\h$LightBlue)"
PS1="$PS1\n$Yellow-$LightCyan+$LightBlue($Yellow\w$LightBlue)$DarkGray>$LightGray>>$White> "
PS2="$DarkGray-$LightGray-$LightGray-$White "

EDITOR="/usr/bin/emacs"

PATH="/bin:/sbin"
PATH="/usr/bin:/usr/sbin:$PATH"
PATH="/usr/local/bin:/usr/local/sbin:$PATH"
PATH="/usr/games:$PATH"
PATH="$HOME/bin:$PATH"

export PS1 PS2 EDITOR PATH
```
