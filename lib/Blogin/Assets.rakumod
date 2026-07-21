use v6.d;

unit module Blogin::Assets;

our sub minify-css(Str $css --> Str) {
  my $out = $css.subst(/ '/*' .*? '*/' /, '', :g);

  $out = $out.subst(/ \s+ /, ' ', :g);
  $out = $out.subst(/ \s* (<[ { } : ; , > ]>) \s* /, { ~.[0] }, :g);
  $out = $out.subst(/ ';}' /, '}', :g);

  $out.trim;
}

# Line-oriented and ASI-safe: drops blank lines, whole-line comments, and
# indentation without touching content inside a line.
our sub minify-js(Str $js --> Str) {
  $js.lines.map(*.trim).grep({ .chars && !.starts-with('//') }).join("\n");
}

our sub content-hash($content --> Str) {
  my $bytes = $content ~~ Blob ?? $content !! $content.encode('utf-8');

  my $hash = 0x811c9dc5;
  for $bytes.list -> $byte {
    $hash = (($hash +^ $byte) * 0x01000193) +& 0xFFFFFFFF;
  }

  sprintf('%08x', $hash);
}

our sub fingerprint-name(Str $filename, Str $hash --> Str) {
  my $ext = $filename.IO.extension;

  return "$filename.$hash" unless $ext.chars;

  my $stem = $filename.substr(0, $filename.chars - $ext.chars - 1);

  "$stem.$hash.$ext";
}

our sub is-fingerprintable(IO::Path:D $file --> Bool) {
  so $file.extension.lc eq any(<css js png jpg jpeg gif webp svg ico>);
}

# Replace each asset URL with its fingerprinted URL wherever it appears as a
# delimited token: quoted, inside a CSS url(), or as a srcset entry.
our sub rewrite-refs(Str $text, %manifest --> Str) {
  my $out = $text;

  for %manifest.kv -> $original, $fingerprinted {
    $out = $out.subst(
      / $<pre>=(<[ \" \' \( \, \s ]>) "$original" $<post>=(<[ \" \' \) \, \s ]>) /,
      -> $match { $match<pre> ~ $fingerprinted ~ $match<post> },
      :g,
    );
  }

  $out;
}

our sub is-raster(IO::Path:D $file --> Bool) {
  so $file.extension.lc eq any(<png jpg jpeg gif webp>);
}

our sub variant-name(Str $filename, Int $width --> Str) {
  my $ext = $filename.IO.extension;

  return "$filename-$width" unless $ext.chars;

  my $stem = $filename.substr(0, $filename.chars - $ext.chars - 1);

  "$stem-$width.$ext";
}

sub tool-available(Str $name --> Bool) {
  so (%*ENV<PATH> // '').split(':').first({ .chars && .IO.add($name).x });
}

# The first available image resizer, preferring ImageMagick for cross-platform
# consistency, then macOS sips. Empty string when none is found.
our sub resizer(--> Str) {
  for <magick convert sips> -> $tool {
    return $tool if tool-available($tool);
  }

  '';
}

sub run-quiet(*@command) {
  my $proc = run(|@command, :out, :err);
  my $out  = $proc.out.slurp(:close);

  $proc.err.slurp(:close);

  %( ok => $proc.exitcode == 0, out => $out );
}

our sub image-width(IO::Path:D $file, Str $tool --> Int) {
  my %result = do given $tool {
    when 'sips'   { run-quiet('sips', '-g', 'pixelWidth', $file.absolute) }
    when 'magick' { run-quiet('magick', 'identify', '-format', '%w', $file.absolute) }
    default       { run-quiet('identify', '-format', '%w', $file.absolute) }
  };

  return 0 unless %result<ok>;

  %result<out> ~~ / (\d+) / ?? +$0 !! 0;
}

our sub resize(IO::Path:D $src, IO::Path:D $dest, Int $width, Str $tool --> Bool) {
  my %result = do given $tool {
    when 'sips'   { run-quiet('sips', '--resampleWidth', $width.Str, $src.absolute, '--out', $dest.absolute) }
    when 'magick' { run-quiet('magick', $src.absolute, '-resize', "{ $width }x", $dest.absolute) }
    default       { run-quiet('convert', $src.absolute, '-resize', "{ $width }x", $dest.absolute) }
  };

  %result<ok>;
}

# A srcset listing each variant by width plus the original at its full width.
our sub srcset-value(Str $original-url, Int $original-width, @variants --> Str) {
  my @entries = @variants.map({ "{ .<url> } { .<width> }w" });

  @entries.push("$original-url { $original-width }w");
  @entries.join(', ');
}

our sub add-srcset(Str $html, %srcsets --> Str) {
  my $out = $html;

  for %srcsets.kv -> $url, $srcset {
    $out = $out.subst('src="' ~ $url ~ '"', 'src="' ~ $url ~ '" srcset="' ~ $srcset ~ '"', :g);
  }

  $out;
}
