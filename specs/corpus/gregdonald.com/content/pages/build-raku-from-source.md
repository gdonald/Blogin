---
title: Build Raku from source
date: 2019-10-26
slug: build-raku-from-source
description: Here's my shell script for building MoarVM, NQP, and Rakudo from source.
tags: [build, moarvm, nqp, raku, readline, zef]
archives: [2019-10]
---
Here's my shell script for building [MoarVM](https://github.com/MoarVM/MoarVM), [Not Quite Perl](https://github.com/Raku/nqp), and [Rakudo](https://github.com/rakudo/rakudo) from source.

```bash
#!/usr/bin/env bash

SRC=~/rakudo/src
INSTALL=~/rakudo

rm -rf $INSTALL

if [ ! -d $SRC ]
then
    echo "Creating $SRC"
    mkdir -p $SRC
fi

cd $SRC

git clone git@github.com:MoarVM/MoarVM.git
git clone git@github.com:Raku/nqp.git
git clone git@github.com:rakudo/rakudo.git
git clone git@github.com:ugexe/zef.git

#cd $SRC/rakudo/t; pwd
#git clone git@github.com:Raku/roast.git spec

cd $SRC/MoarVM; pwd
perl Configure.pl --prefix $INSTALL && make -j17 && make install

cd $SRC/nqp; pwd
perl Configure.pl --backend=moar --prefix=$INSTALL && make -j17 && make install

cd $SRC/rakudo; pwd
perl Configure.pl --backend=moar --prefix=$INSTALL && make -j17 && make install

$INSTALL/bin/rakudo -v

PATH=$INSTALL/bin:$INSTALL/share/perl6/site/bin:$PATH
printf "\nDon't forget to add $INSTALL/bin and $INSTALL/share/perl6/site/bin to your \$PATH\n"

cd $SRC/zef; pwd
raku -I. bin/zef install .

zef install Linenoise
zef install TAP::Harness
zef install App::Prove6
zef install JSON::Tiny
zef install DBIish --force-test
zef install Log::Async
zef install Test::Output
```

You have to use **--force-test** to install certain Raku modules if you don't have all the prerequisites for the tests to pass.
