use v6.d;

unit module Blogin::Metrics;

our sub word-count(Str $text --> Int) {
  $text.words.elems;
}

our sub reading-time(Int $words, Int :$wpm = 200 --> Int) {
  return 0 unless $words > 0;

  max(1, ($words / $wpm).ceiling);
}
