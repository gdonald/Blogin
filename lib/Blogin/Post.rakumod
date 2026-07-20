use v6.d;

use Blogin::Slug;

unit class Blogin::Post;

has Str  $.title;
has Date $.date;
has Str  $.slug;
has      @.tags;
has Bool $.draft = False;
has Str  $.description = '';
has Str  $.body;
has      %.meta;
has Str  $.filename = '';

my @KNOWN = <title date slug tags draft description>;

my sub unquote(Str $value --> Str) {
  return $value.substr(1, $value.chars - 2)
    if $value.chars >= 2
    && ( ($value.starts-with('"') && $value.ends-with('"'))
      || ($value.starts-with("'") && $value.ends-with("'")) );

  $value;
}

my sub parse-date(Str $text --> Date) {
  return Date unless $text ~~ / ^ (\d ** 4) '-' (\d\d) '-' (\d\d) $ /;

  (try Date.new(+$0, +$1, +$2)) // Date;
}

my sub basename(Str $filename --> Str) {
  $filename.subst(/ ^ .* '/' /, '');
}

my sub filename-stem(Str $filename --> Str) {
  my $stem = basename($filename);

  $stem = $stem.subst(/ '.' <-[.]>+ $ /, '');
  $stem = $stem.subst(/ ^ \d ** 4 '-' \d\d '-' \d\d '-'? /, '');

  $stem;
}

my sub date-from-filename(Str $filename --> Date) {
  my $base = basename($filename);

  return parse-date("$0-$1-$2") if $base ~~ / ^ (\d ** 4) '-' (\d\d) '-' (\d\d) /;

  Date;
}

my sub parse-tags(Str $raw --> List) {
  my $value = $raw.trim;

  $value = $value.substr(1, $value.chars - 2)
    if $value.starts-with('[') && $value.ends-with(']');

  $value.split(',').map(*.trim).grep(*.chars).List;
}

my sub parse-front-matter(Str $source) {
  my @lines = $source.split("\n");
  my %fields;

  my $body-start = 0;

  if @lines.elems && @lines[0].trim eq '---' {
    my $index = 1;

    while $index < @lines.elems && @lines[$index].trim ne '---' {
      my $line = @lines[$index];

      if $line ~~ / ^ \h* $<key>=[ <[\w-]>+ ] \h* ':' [ \h+ $<val>=[ \N* ] ]? $ / {
        %fields{ ~$<key> } = $<val>.defined ?? $<val>.Str.trim !! '';
      }

      $index++;
    }

    $body-start = $index < @lines.elems ?? $index + 1 !! $index;
  }

  my $body = @lines[$body-start .. *].join("\n").subst(/ ^ [ \h* \n ]+ /, '');

  %( fields => %fields, body => $body );
}

method parse(Blogin::Post:U: Str $source, Str :$filename = '' --> Blogin::Post) {
  my %parsed = parse-front-matter($source);
  my %fields = %parsed<fields>;

  my $title = %fields<title>:exists && %fields<title>.chars
    ?? unquote(%fields<title>)
    !! Blogin::Slug::humanize(filename-stem($filename));

  die "missing title in '$filename'" unless $title.defined && $title.chars;

  my $date;

  if %fields<date>:exists && %fields<date>.chars {
    $date = parse-date(unquote(%fields<date>));
    die "unparseable date '{ %fields<date> }' in '$filename'" without $date;
  }
  else {
    $date = date-from-filename($filename);
  }

  my $slug = %fields<slug>:exists && %fields<slug>.chars
    ?? unquote(%fields<slug>)
    !! Blogin::Slug::slugify($title);

  my @tags        = parse-tags(%fields<tags> // '');
  my $draft       = (%fields<draft> // '').lc eq 'true';
  my $description = unquote(%fields<description> // '');

  my %meta;
  for %fields.kv -> $key, $value {
    %meta{$key} = $value unless $key eq any(@KNOWN);
  }

  self.new(
    :$title,
    date => ($date // Date),
    :$slug,
    :@tags,
    :$draft,
    :$description,
    body => %parsed<body>,
    meta => %meta,
    :$filename,
  );
}

method load(Blogin::Post:U: IO() $path --> Blogin::Post) {
  self.parse($path.slurp, filename => $path.basename);
}
