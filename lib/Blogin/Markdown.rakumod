use v6.d;

use Blogin::Markdown::Node;
use Blogin::Markdown::Actions;

unit module Blogin::Markdown;

sub is-blank(Str $line --> Bool) {
  so $line ~~ /^ \h* $/;
}

sub is-thematic-break(Str $line --> Bool) {
  so $line ~~ /^ \h*
    [ '*' [ \h* '*' ] ** 2..*
    | '-' [ \h* '-' ] ** 2..*
    | '_' [ \h* '_' ] ** 2..*
    ] \h* $/;
}

sub is-atx(Str $line --> Bool) {
  so $line ~~ /^ \h* '#' ** 1..6 [ \h | $ ] /;
}

sub fence-of(Str $line) {
  return $/ if $line ~~ /^ \h* $<fence>=[ '`' ** 3..* | '~' ** 3..* ] \h* $<info>=[ \N* ] $/;
  Nil;
}

sub is-close-fence(Str $line, Str $fence-char, Int $length --> Bool) {
  return False unless $line ~~ /^ \h* $<run>=[ $fence-char+ ] \h* $/;
  $<run>.Str.chars >= $length;
}

sub marker-of(Str $line) {
  if $line ~~ /^ $<indent>=(\h*) $<marker>=(<[-+*]> | \d+ <[.)]>) [ $<space>=(\h+) | $ ] / {
    my $indent = $<indent>.Str.chars;
    my $marker = $<marker>.Str;
    my $space  = $<space> ?? $<space>.Str.chars !! 1;

    return {
      indent      => $indent,
      marker      => $marker,
      ordered     => so($marker ~~ /\d/),
      content-col => $indent + $marker.chars + $space,
    };
  }

  Nil;
}

sub split-row(Str $line) {
  my $cells = $line.trim;

  $cells .= substr(1) if $cells.starts-with('|');
  $cells .= chop      if $cells.ends-with('|');

  $cells.split('|').map(*.trim).List;
}

sub is-delimiter-row(Str $line --> Bool) {
  return False unless $line.contains('-');

  my @cells = split-row($line);

  return False unless @cells;

  so @cells.all ~~ /^ ':'? '-'+ ':'? $/;
}

sub cell-align(Str $spec) {
  my $left  = $spec.starts-with(':');
  my $right = $spec.ends-with(':');

  return 'center' if $left && $right;
  return 'left'   if $left;
  return 'right'  if $right;

  Str;
}

sub is-block-start(Str $line --> Bool) {
  return True if is-atx($line);
  return True if is-thematic-break($line);
  return True if fence-of($line);
  return True if $line ~~ /^ \h* '>' /;
  return True if marker-of($line);

  False;
}

sub is-definition(Str $line --> Bool) {
  so $line ~~ /^ \h* ':' \h+ \S/;
}

sub parse-definition-list(@lines, Int $index is rw --> DefinitionList) {
  my @items;

  while $index < @lines.elems {
    my $line = @lines[$index];

    last if is-blank($line);
    last if is-definition($line) || is-block-start($line);
    last unless $index + 1 < @lines.elems && is-definition(@lines[$index + 1]);

    my @term = parse-inline($line);
    $index++;

    my @definitions;

    while $index < @lines.elems && is-definition(@lines[$index]) {
      my $body = @lines[$index].subst(/^ \h* ':' \h+ /, '');
      @definitions.push(parse-inline($body).Array);
      $index++;
    }

    @items.push(DefinitionItem.new(:@term, :@definitions));
  }

  DefinitionList.new(:@items);
}

sub parse-list(@lines, Int $index is rw --> List) {
  my $base    = marker-of(@lines[$index]);
  my $ordered = $base<ordered>;
  my $start   = $ordered ?? +($base<marker>.subst(/<[.)]> $/, '')) !! 1;

  my @items;

  while $index < @lines.elems {
    my $marker = marker-of(@lines[$index]);

    last unless $marker;
    last if $marker<ordered> != $ordered;

    my $content-col = $marker<content-col>;
    my @item-lines;

    my $first = @lines[$index];
    @item-lines.push($first.chars > $content-col ?? $first.substr($content-col) !! '');
    $index++;

    while $index < @lines.elems {
      my $line = @lines[$index];

      if is-blank($line) {
        my $next = $index + 1 < @lines.elems ?? @lines[$index + 1] !! Str;

        if $next.defined && ($next ~~ /^ \h ** {$content-col} \S/ || marker-of($next)) {
          @item-lines.push('');
          $index++;
          next;
        }

        last;
      }

      if $line ~~ /^ \h ** {$content-col} / {
        @item-lines.push($line.substr($content-col));
        $index++;
        next;
      }

      last if marker-of($line);
      last if is-block-start($line);

      @item-lines.push($line);
      $index++;
    }

    @item-lines.pop while @item-lines.elems && @item-lines[*-1] eq '';

    my $task    = False;
    my $checked = False;

    if @item-lines.elems && @item-lines[0] ~~ /^ '[' $<box>=[ <[xX]> | ' ' ] ']' \h+ / {
      $task    = True;
      $checked = so $<box>.Str.lc eq 'x';
      @item-lines[0] = @item-lines[0].subst(/^ '[' [ <[xX]> | ' ' ] ']' \h+ /, '');
    }

    @items.push(ListItem.new(:$task, :$checked, children => parse-blocks(@item-lines)));
  }

  List.new(:$ordered, :$start, :@items);
}

sub parse-blocks(@lines --> Array) {
  my @blocks;
  my $index = 0;

  while $index < @lines.elems {
    my $line = @lines[$index];

    if is-blank($line) {
      $index++;
      next;
    }

    if is-thematic-break($line) {
      @blocks.push(ThematicBreak.new);
      $index++;
      next;
    }

    if is-atx($line) {
      $line ~~ /^ \h* $<hashes>=[ '#' ** 1..6 ] [ \h+ $<content>=[ \N*? ] ]? [ \h+ '#'+ ]? \h* $/;
      my $level   = $<hashes>.Str.chars;
      my $content = $<content>.defined ?? ~$<content> !! '';

      @blocks.push(Heading.new(:$level, children => parse-inline($content)));
      $index++;
      next;
    }

    with fence-of($line) -> $fence-match {
      my $fence      = $fence-match<fence>.Str;
      my $fence-char = $fence.substr(0, 1);
      my $info       = $fence-match<info>.Str.trim;
      my @body;

      $index++;

      while $index < @lines.elems && !is-close-fence(@lines[$index], $fence-char, $fence.chars) {
        @body.push(@lines[$index]);
        $index++;
      }

      $index++ if $index < @lines.elems;

      my $text = @body.elems ?? @body.join("\n") ~ "\n" !! '';

      @blocks.push(CodeBlock.new(:$info, :$text));
      next;
    }

    if $line ~~ /^ \h* '>' / {
      my @inner;

      while $index < @lines.elems && @lines[$index] ~~ /^ \h* '>' / {
        @inner.push(@lines[$index].subst(/^ \h* '>' \h? /, ''));
        $index++;
      }

      @blocks.push(BlockQuote.new(children => parse-blocks(@inner)));
      next;
    }

    if $line.contains('|') && $index + 1 < @lines.elems && is-delimiter-row(@lines[$index + 1]) {
      my @header = split-row($line).map({ parse-inline($_) });
      my @aligns = split-row(@lines[$index + 1]).map({ cell-align($_) });

      $index += 2;

      my @rows;

      while $index < @lines.elems && !is-blank(@lines[$index]) && @lines[$index].contains('|') {
        @rows.push(split-row(@lines[$index]).map({ parse-inline($_) }).Array);
        $index++;
      }

      @blocks.push(Table.new(:@aligns, :@header, :@rows));
      next;
    }

    if !is-definition($line) && !marker-of($line)
       && $index + 1 < @lines.elems && is-definition(@lines[$index + 1]) {
      @blocks.push(parse-definition-list(@lines, $index));
      next;
    }

    if marker-of($line) {
      @blocks.push(parse-list(@lines, $index));
      next;
    }

    my @paragraph;

    while $index < @lines.elems {
      my $current = @lines[$index];

      last if is-blank($current);

      if @paragraph.elems && $current ~~ /^ \h* $<underline>=[ '=' + | '-' + ] \h* $/ {
        my $level = $<underline>.Str.substr(0, 1) eq '=' ?? 1 !! 2;

        @blocks.push(Heading.new(:$level, children => parse-inline(@paragraph.join("\n"))));
        $index++;
        @paragraph = ();
        last;
      }

      last if @paragraph.elems && is-block-start($current);

      @paragraph.push($current);
      $index++;
    }

    if @paragraph.elems {
      @blocks.push(Paragraph.new(children => parse-inline(@paragraph.join("\n"))));
    }
  }

  @blocks;
}

our sub parse(Str $source --> Document) is export {
  my $normalized = $source.subst(/\r\n/, "\n", :g).subst(/\r/, "\n", :g);
  my @lines = $normalized.split("\n");

  @lines.pop if @lines.elems && @lines[*-1] eq '';

  Document.new(children => parse-blocks(@lines));
}
