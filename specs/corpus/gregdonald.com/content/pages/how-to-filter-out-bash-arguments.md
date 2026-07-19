---
title: How to filter out Bash arguments
date: 2014-09-22
slug: how-to-filter-out-bash-arguments
description: Ever want to know how to drop an argument (and value), --dir in this case, from a Bash script? Someone from my local LUG asked how to do it and this is what I came up with: Fun ;) #!/usr/bin/env bash...
tags: [bash]
archives: [2014-09]
---
Ever want to know how to drop an argument (and value), --dir in this case, from a Bash script?

Someone from [my local LUG](https://www.meetup.com/Nashville-Linux-Users-Group/) [asked how to do it](https://groups.google.com/d/msg/nlug-talk/Rc01te2N85k/zu9MRbGNgtcJ) and this is what I came up with:

Fun ;)

```bash
#!/usr/bin/env bash

args=("$@")
myargs=()
nextarg=-1

for ((i=0; i<$#; i++)) {
   if [ $nextarg == $i ]; then continue; fi
   case ${args[$i]} in
       --dir) nextarg=$((i+1)) ;;
       *) myargs+="${args[$i]} "
   esac
}

echo $myargs
``` ```bash
./remove_dir.bash --dir foo --bar baz
--bar baz
```
