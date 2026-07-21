use v6.d;

unit module Blogin::Highlight;

my %LANGS =
  raku => %(
    comment  => '#',
    keywords => set(<my our sub method class role grammar token rule regex has is
                     does return if elsif else unless for while loop given when
                     default use need unit module multi proto enum constant>),
  ),
  ruby => %(
    comment  => '#',
    keywords => set(<def end class module if elsif else unless case when while until
                     do return yield require attr_accessor new self nil true false>),
  ),
  python => %(
    comment  => '#',
    keywords => set(<def class if elif else for while return import from as with try
                     except finally lambda pass yield None True False and or not in is>),
  ),
  javascript => %(
    comment  => '//',
    keywords => set(<function var let const if else for while return class new this
                     import export from async await try catch throw typeof of>),
  ),
  bash => %(
    comment  => '#',
    keywords => set(<if then elif else fi for in do done while until case esac
                     function echo return export local>),
  ),
  json => %(
    comment  => '',
    keywords => set(),
  ),
  c => %(
    comment  => '//',
    keywords => set(<int char short long float double void unsigned signed const
                     static struct union enum typedef sizeof if else for while do
                     switch case default break continue return goto extern>),
  ),
  cpp => %(
    comment  => '//',
    keywords => set(<int char bool float double void auto const static struct class
                     namespace template typename public private protected virtual
                     if else for while do switch case default break continue return
                     new delete this using nullptr true false enum union>),
  ),
  java => %(
    comment  => '//',
    keywords => set(<public private protected class interface extends implements
                     static final void int long double boolean char new return if
                     else for while do switch case default break continue this super
                     import package try catch finally throw throws null true false>),
  ),
  go => %(
    comment  => '//',
    keywords => set(<func package import var const type struct interface map chan go
                     defer if else for range switch case default break continue
                     return nil true false new make>),
  ),
  rust => %(
    comment  => '//',
    keywords => set(<fn let mut const static struct enum impl trait pub use mod
                     match if else for while loop return self where as ref move dyn
                     true false None Some Ok Err>),
  ),
  typescript => %(
    comment  => '//',
    keywords => set(<function const let var interface type class enum public private
                     protected readonly extends implements import export from if else
                     for while return new this async await try catch throw typeof of
                     as null undefined true false>),
  ),
  ;

sub esc(Str $text --> Str) {
  $text.trans([ '&', '<', '>' ] => [ '&amp;', '&lt;', '&gt;' ]);
}

sub span(Str $class, Str $text --> Str) {
  "<span class=\"hl-$class\">{ esc($text) }</span>";
}

our sub languages(--> List) is export {
  %LANGS.keys.sort.List;
}

our sub supports(Str $language --> Bool) is export {
  %LANGS{ ($language.words.head // '').lc }:exists;
}

our sub highlight(Str $code, Str $language --> Str) is export {
  my $lang = ($language.words.head // '').lc;
  my $def  = %LANGS{$lang};

  return esc($code) without $def;

  my $comment  = $def<comment>;
  my $keywords = $def<keywords>;

  my $out = '';
  my $pos = 0;
  my $len = $code.chars;

  while $pos < $len {
    my $rest = $code.substr($pos);
    my $ch   = $rest.substr(0, 1);

    if $comment.chars && $rest.starts-with($comment) {
      my $newline = $rest.index("\n");
      my $token   = $newline.defined ?? $rest.substr(0, $newline) !! $rest;
      $out ~= span('comment', $token);
      $pos += $token.chars;
      next;
    }

    if $ch eq '"' || $ch eq "'" {
      my $close = $rest.index($ch, 1);
      if $close.defined {
        my $token = $rest.substr(0, $close + 1);
        $out ~= span('string', $token);
        $pos += $token.chars;
        next;
      }
    }

    if $ch ~~ / ^ \d $ / {
      my $token = ($rest ~~ / ^ \d+ [ '.' \d+ ]? /).Str;
      $out ~= span('number', $token);
      $pos += $token.chars;
      next;
    }

    if $ch ~~ / ^ <[A..Za..z_]> $ / {
      my $token = ($rest ~~ / ^ \w+ /).Str;
      $out ~= $keywords{$token} ?? span('keyword', $token) !! esc($token);
      $pos += $token.chars;
      next;
    }

    $out ~= esc($ch);
    $pos++;
  }

  $out;
}
