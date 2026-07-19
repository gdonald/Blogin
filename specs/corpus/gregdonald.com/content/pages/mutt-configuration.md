---
title: Mutt Configuration
date: 2011-04-03
slug: mutt-configuration
description: # basic .muttrc for use with Gmail # Change the following six lines to match your Gmail account details set imap_user = "username@gmail.com" set imap_pass = "" set smtp_url =...
tags: [gmail, mutt]
archives: [2011-04]
---
Real Men use [Mutt](http://www.mutt.org/) :)

```bash
# basic .muttrc for use with Gmail
# Change the following six lines to match your Gmail account details
set imap_user = "username@gmail.com"
set imap_pass = ""
set smtp_url = "smtp://username@smtp.gmail.com:587/"
set smtp_pass = ""
set from = "username@gmail.com"
set realname = "Firstname Lastname"
#
# # Change the following line to a different editor you prefer.
set editor = 'vim + -c "set textwidth=72" -c "set wrap"'
# Basic config
set folder = "imaps://imap.gmail.com:993"
set spoolfile = "+INBOX"
set imap_check_subscribed=yes
set hostname = gmail.com
set mail_check = 120
set timeout = 300
set imap_keepalive = 300
set postponed = "+[GMail]/Drafts"
set header_cache=~/.mutt/cache/headers
set message_cachedir=~/.mutt/cache/bodies
set certificate_file=~/.mutt/certificates
set move = no
set include
set sort = 'threads'
set sort_aux = 'reverse-last-date-received'
set auto_tag = yes
set pager_index_lines = 10
ignore "Authentication-Results:"
ignore "DomainKey-Signature:"
ignore "DKIM-Signature:"
hdr_order Date From To Cc
alternative_order text/plain text/html *
auto_view text/html
bind editor  complete-query
bind editor ^T complete
bind editor  noop
# # Gmail-style keyboard shortcuts
macro index,pager am "unset trash\n " "Gmail archive message" # different from Gmail, but wanted to keep "y" to show folders.
macro index,pager d "set trash=\"imaps://imap.googlemail.com/[GMail]/Bin\"\n " "Gmail delete message"
macro index,pager gi "=INBOX" "Go to inbox"
macro index,pager ga "=[Gmail]/All Mail" "Go to all mail"
macro index,pager gs "=[Gmail]/Starred" "Go to starred messages"
macro index,pager gd "=[Gmail]/Drafts" "Go to drafts"
macro index,pager gl "?" "Go to 'Label'" # will take you to a list of all your Labels (similar to viewing folders).
```
