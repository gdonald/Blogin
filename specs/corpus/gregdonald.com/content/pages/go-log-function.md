---
title: go log function
date: 2015-01-08
slug: go-log-function
description: func log(s ...interface{}) { f, err := os.OpenFile("info.log", os.O_RDWR|os.O_CREATE|os.O_APPEND, 0666) if err != nil { fmt.Printf("error opening log file: %v", err) os.Exit(1) } defer f.Close()...
tags: [go-lang]
archives: [2015-01]
---
```go
func log(s ...interface{}) {
	f, err := os.OpenFile("info.log", os.O_RDWR|os.O_CREATE|os.O_APPEND, 0666)
	if err != nil {
		fmt.Printf("error opening log file: %v", err)
		os.Exit(1)
	}
	defer f.Close()
	log.SetOutput(f)
	ss := ""
	for _, p := range s {
		switch p.(type) {
		case bool:
			ss += fmt.Sprintf("%t ", p.(bool))
		case int:
			ss += fmt.Sprintf("%d ", p.(int))
		case float64:
			ss += fmt.Sprintf("%.2f ", p.(float64))
		case string:
			ss += fmt.Sprintf("%s ", p.(string))
		}
	}
	log.Println(ss)
}
```
