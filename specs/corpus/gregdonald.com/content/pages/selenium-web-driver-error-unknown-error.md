---
title: Selenium::WebDriver::Error::UnknownError
date: 2020-01-20
slug: selenium-web-driver-error-unknown-error
description: Selenium::WebDriver::Error::UnknownError: unknown error: Chrome failed to start: exited abnormally (unknown error: DevToolsActivePort file doesn't exist) (The process started from chrome location...
tags: [chrome, selenium]
archives: [2020-01]
---
I forget this every time I run Rspec on a new Debian. Maybe if I write it down it will help me in the future.

```bash
Selenium::WebDriver::Error::UnknownError:
       unknown error: Chrome failed to start: exited abnormally
         (unknown error: DevToolsActivePort file doesn't exist)
         (The process started from chrome location /usr/bin/google-chrome is no longer running, so ChromeDriver is assuming that Chrome has crashed.)
```

Solution:

```bash
export DISPLAY=:99
/usr/bin/Xvfb -ac $DISPLAY -screen 0 1024x768x16
```
