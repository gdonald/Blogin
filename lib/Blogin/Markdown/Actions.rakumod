use v6.d;

use Blogin::Markdown::Node;
use Blogin::Markdown::Grammar;

unit class Blogin::Markdown::Actions;

sub merge-text(@nodes) {
  my @out;

  for @nodes -> $node {
    if $node ~~ Text && @out.elems && @out[*-1] ~~ Text {
      @out[*-1] = Text.new(text => @out[*-1].text ~ $node.text);
    }
    else {
      @out.push($node);
    }
  }

  @out;
}

our sub parse-inline(Str $text --> Array) is export {
  my $actions = Blogin::Markdown::Actions.new;
  my $match   = Blogin::Markdown::Grammar.parse($text, :$actions);

  ($match ?? $match.made !! [ Text.new(:$text) ]).Array;
}

sub build-attrs($block) {
  my %attrs;
  my @classes;

  return %attrs unless $block;

  for $block<attr>.list -> $attr {
    if    $attr<class> { @classes.push(~$attr<class>) }
    elsif $attr<id>    { %attrs<id> = ~$attr<id> }
    elsif $attr<key>   { %attrs{ ~$attr<key> } = ~$attr<val> }
  }

  %attrs<class> = @classes.join(' ') if @classes;

  %attrs;
}

method TOP($/) {
  make merge-text($<inline>.map(*.made));
}

method inline($/) {
  my $child = $<esc> // $<code> // $<image> // $<footref> // $<link> // $<reflink>
           // $<autolink> // $<strong> // $<emph> // $<strike> // $<hardbreak>
           // $<softbreak> // $<text>;

  make $child.made;
}

method footref($/) {
  make FootnoteRef.new(label => ~$<label>);
}

method reflink($/) {
  my $text  = ~$<text>;
  my $label = (~$<label>).trim;
  $label = $text.trim unless $label.chars;

  my %defs = (try %*LINK-DEFS) // %();

  if %defs{ $label.lc }:exists {
    my %def = %defs{ $label.lc };

    make Link.new(
      url      => %def<url>,
      title    => (%def<title> // ''),
      children => parse-inline($text),
    );
  }
  else {
    make Text.new(text => "[$text][{ ~$<label> }]");
  }
}

method esc($/) {
  make Text.new(text => ~$<char>);
}

method code($/) {
  my $text = ~$<body>;

  if $text.chars >= 2 && $text.starts-with(' ') && $text.ends-with(' ') && $text.trim ne '' {
    $text = $text.substr(1, $text.chars - 2);
  }

  make CodeSpan.new(:$text);
}

method image($/) {
  make Image.new(
    url   => ~$<url>,
    title => ($<title> ?? ~$<title> !! ''),
    alt   => ~$<alt>,
    attrs => build-attrs($<attr-block>),
  );
}

method link($/) {
  make Link.new(
    url      => ~$<url>,
    title    => ($<title> ?? ~$<title> !! ''),
    attrs    => build-attrs($<attr-block>),
    children => parse-inline(~$<text>),
  );
}

method autolink($/) {
  my $url = ~$<url>;

  make Link.new(:$url, children => [ Text.new(text => $url) ]);
}

method strong($/) {
  make Strong.new(children => merge-text($<inline>.map(*.made)));
}

method emph($/) {
  make Emphasis.new(children => merge-text($<inline>.map(*.made)));
}

method strike($/) {
  make Strikethrough.new(children => merge-text($<inline>.map(*.made)));
}

method hardbreak($/) {
  make LineBreak.new;
}

method softbreak($/) {
  make SoftBreak.new;
}

method text($/) {
  make Text.new(text => ~$/);
}
