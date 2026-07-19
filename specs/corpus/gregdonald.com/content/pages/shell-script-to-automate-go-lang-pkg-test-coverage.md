---
title: Shell script to automate go lang pkg test coverage
date: 2015-01-13
slug: shell-script-to-automate-go-lang-pkg-test-coverage
description: #!/bin/sh PACKAGE=mypkg # set mode go test -coverprofile=coverage.out $PACKAGE go tool cover -func=coverage.out go tool cover -html=coverage.out # count mode go test -covermode=count...
tags: [go-lang, testing]
archives: [2015-01]
---
```bash
#!/bin/sh
PACKAGE=mypkg

# set mode
go test -coverprofile=coverage.out $PACKAGE
go tool cover -func=coverage.out
go tool cover -html=coverage.out

# count mode
go test -covermode=count -coverprofile=count.out $PACKAGE
go tool cover -func=count.out
go tool cover -html=count.out

# more info: http://blog.golang.org/cover
```
