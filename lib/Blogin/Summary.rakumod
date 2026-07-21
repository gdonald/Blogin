use v6.d;

unit module Blogin::Summary;

constant MORE = '<!--more-->';

sub truncate(Str $text, Int $length --> Str) {
  return $text if $text.chars <= $length;

  my $cut = $text.substr(0, $length).trim;

  with $cut.rindex(' ') -> $space {
    $cut = $cut.substr(0, $space);
  }

  $cut.trim ~ '…';
}

# The first non-empty block of stripped text, capped at $length. Blocks in the
# renderer's plain text are newline-separated.
our sub first-block(Str $text, Int $length --> Str) {
  my $block = ($text.split("\n").first(*.trim.chars) // '').trim;

  truncate($block, $length);
}

# Explicit front-matter summary wins, then the text before a <!--more--> marker,
# then the capped first block of the body.
our sub choose(Str :$explicit = '', Str :$excerpt = '', Str :$text = '', Int :$length = 200 --> Str) {
  return $explicit.trim if $explicit.trim.chars;
  return $excerpt.trim  if $excerpt.trim.chars;

  first-block($text, $length);
}
