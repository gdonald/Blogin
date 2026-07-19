---
title: Convert one m4a file to many mp3s
date: 2015-09-22
slug: convert-one-m4a-file-to-many-mp3s
description: #!/bin/bash while [ $# -gt 0 ]; do ffmpeg -i "$1" 2> tmp.txt while read -r first _ _ start _ end; do if [[ $first = Chapter ]]; then read # discard line with Metadata: read _ _ chapter ffmpeg -vsync...
tags: [mp3s]
archives: [2015-09]
---
```bash
#!/bin/bash
while [ $# -gt 0 ]; do

ffmpeg -i "$1" 2> tmp.txt

while read -r first _ _ start _ end; do
  if [[ $first = Chapter ]]; then
    read  # discard line with Metadata:
    read _ _ chapter
    ffmpeg -vsync 2 -i "$1" -ss "${start%?}" -to "$end" -vn -ar 44100 -ac 2 -ab 128  -f mp3 "$chapter.mp3"
```
