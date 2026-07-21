use v6.d;

unit module Blogin::Toc;

sub esc(Str $text --> Str) {
  $text.trans([ '&', '<', '>', '"' ] => [ '&amp;', '&lt;', '&gt;', '&quot;' ]);
}

# Flat heading records (level, text, id) into a nested tree of
# { title, id, level, children } by heading level.
our sub build(@headings --> Array) {
  my @roots;
  my @stack;

  for @headings -> $heading {
    my $node = {
      title    => $heading<text>,
      id       => $heading<id>,
      level    => $heading<level>,
      children => [],
    };

    @stack.pop while @stack && @stack[*-1]<level> >= $heading<level>;

    (@stack ?? @stack[*-1]<children> !! @roots).push($node);

    @stack.push($node);
  }

  @roots;
}

our sub render(@nodes --> Str) {
  return '' unless @nodes;

  my $out = '<ul>';

  for @nodes -> $node {
    $out ~= '<li><a href="#' ~ esc($node<id>) ~ '">' ~ esc($node<title>) ~ '</a>';
    $out ~= render($node<children>) if $node<children>.elems;
    $out ~= '</li>';
  }

  $out ~ '</ul>';
}
