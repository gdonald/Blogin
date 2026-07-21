use v6.d;

unit module Blogin::Shortcode;

sub html-escape(Str $text --> Str) {
  $text.trans([ '&', '<', '>' ] => [ '&amp;', '&lt;', '&gt;' ]);
}

sub attr-escape(Str $text --> Str) {
  $text.trans([ '&', '<', '>', '"' ] => [ '&amp;', '&lt;', '&gt;', '&quot;' ]);
}

# Load user shortcode templates from a directory: shortcodes/<name>.html, keyed
# by name.
our sub load(IO() $dir --> Hash) {
  return %() unless $dir.d;

  my %templates;

  for $dir.dir.grep({ .extension.lc eq 'html' }) -> $file {
    %templates{ $file.extension('').basename } = $file.slurp;
  }

  %templates;
}

# Substitute {{ key }} placeholders in a user template with escaped argument
# values.
our sub render-template(Str $template, %args --> Str) {
  $template.subst(/ '{{' \h* $<key>=(<[\w-]>+) \h* '}}' /, { attr-escape(%args{ ~$/<key> } // '') }, :g);
}

our sub parse-args(Str $raw --> Hash) {
  my %args;

  for $raw ~~ m:g/ $<key>=(<[\w-]>+) '=' '"' $<val>=(<-["]>*) '"' / -> $match {
    %args{ ~$match<key> } = ~$match<val>;
  }

  %args;
}

sub youtube(%args --> Str) {
  my $id = attr-escape(%args<id> // '');

  qq{<div class="video"><iframe src="https://www.youtube.com/embed/$id" allowfullscreen></iframe></div>};
}

sub figure(%args --> Str) {
  my $src     = attr-escape(%args<src> // '');
  my $alt     = attr-escape(%args<alt> // '');
  my $caption = %args<caption> // '';

  my $out = qq{<figure><img src="$src" alt="$alt" />};
  $out ~= '<figcaption>' ~ html-escape($caption) ~ '</figcaption>' if $caption.chars;

  $out ~ '</figure>';
}

our sub known(Str $name --> Bool) {
  so $name eq any(<youtube figure>);
}

our sub expand(Str $name, %args --> Str) {
  given $name {
    when 'youtube' { youtube(%args) }
    when 'figure'  { figure(%args) }
    default        { '' }
  }
}
