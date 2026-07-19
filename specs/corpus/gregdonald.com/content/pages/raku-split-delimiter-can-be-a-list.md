---
title: Raku split delimiter can be a list
date: 2019-10-20
slug: raku-split-delimiter-can-be-a-list
description: Raku's string split routine accepts a scalar as well as a list for its delimiter.
tags: [raku]
archives: [2019-10]
---
Raku's string [split routine](https://docs.perl6.org/routine/split) accepts a scalar as well as a list for its delimiter.

```actionscript
my Str $str = 'This string,has commas,and spaces';
my @delimiters of Str = [' ', ','];
my @array of Str = $str.split(@delimiters);

say @array.perl;
```

Which gives you:

```clike
Array[Str].new("This", "string", "has", "commas", "and", "spaces")
```

Other languages might require you to write a regular expression if you need to split a string on multiple delimiters at the same time.
