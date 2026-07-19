---
title: Raku spiral matrix
date: 2019-08-28
slug: raku-spiral-matrix
description: Given a matrix of m x n elements (m rows, n columns), return all elements of the matrix in spiral order.
tags: [matrix, raku]
archives: [2019-08]
---
```perl
# Given a matrix of m x n elements (m rows, n columns),
# return all elements of the matrix in spiral order.
#
# Example 1:
# Input:
# [[ 1, 2, 3 ], [ 4, 5, 6 ], [ 7, 8, 9 ]]
# Output:
# [1, 2, 3, 6, 9, 8, 7, 4, 5]
#
# Example 2:
# Input:
# [[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12]]
# Output:
# [1, 2, 3, 4, 8, 12, 11, 10, 9, 5, 6, 7]

enum Dir <Right Down Left Up>;

class Cur {
  has Int $.row is rw = 0;
  has Int $.col is rw = 0;
  has Dir $.dir is rw = Dir::Right;

  has $!d = ((0, 1), (1, 0), (0, -1), (-1, 0));

  method next {
    $.dir = self.next-dir($.dir);
    $.row += $!d[$.dir][0];
    $.col += $!d[$.dir][1];
  }

  method same-dir(:@a, --> Bool) {
    if @a[$.row + $!d[$.dir][0]][$.col + $!d[$.dir][1]] {
      $.row += $!d[$.dir][0];
      $.col += $!d[$.dir][1];
      True;
    } else {
      False;
    }
  }

  method next-dir(Dir $dir) {
    given $dir {
      when Dir::Right { Dir::Down }
      when Dir::Down { Dir::Left }
      when Dir::Left { Dir::Up }
      when Dir::Up { Dir::Right }
    }
  }
}

class Spiral {
  has @!a;
  has @.b of Int is rw;
  has Cur $!c;

  submethod BUILD(:@!a) {
    $!c = Cur.new;
    my $e = @!a.flatmap(|*).elems;

    while @!b.elems != $e {
      if @!a[$!c.row][$!c.col] {
        @!b.push(@!a[$!c.row][$!c.col]);
        @!a[$!c.row][$!c.col] = 0;
        $!c.next unless $!c.same-dir(:@!a);
      } else {
        $!c.next;
      }
    }
  }
}

use Test;

my @a = (1..9).Array.rotor(3);
ok Spiral.new(:@a).b == [1, 2, 3, 6, 9, 8, 7, 4, 5];

@a = (1..12).Array.rotor(3);
ok Spiral.new(:@a).b == [1, 2, 3, 4, 8, 12, 11, 10, 9, 5, 6, 7];
```
