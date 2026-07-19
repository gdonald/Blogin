---
title: Raku Fizz Buzz
date: 2019-11-08
slug: raku-fizz-buzz
description: Fizz Buzz written in Raku
tags: [fizzbuzz, raku]
archives: [2019-11]
---
```perl
for (1..100) {
  given $_ {
    when not $_ % 15 { 'FizzBuzz'.say }
    when not $_ % 3  { 'Fizz'    .say }
    when not $_ % 5  { 'Buzz'    .say }
    default          {           .say }
  }
}
```
