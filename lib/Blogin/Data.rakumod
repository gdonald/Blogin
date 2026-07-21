use v6.d;

use JSON::Fast;
use YAMLish;

unit module Blogin::Data;

my sub parse-file(IO::Path:D $file) {
  given $file.extension.lc {
    when 'json'          { from-json($file.slurp) }
    when 'yaml' | 'yml'  { load-yaml($file.slurp) }
    default              { Nil }
  }
}

my sub data-file(IO::Path:D $file --> Bool) {
  so $file.extension.lc eq any(<json yaml yml>);
}

# Nested hashes merge recursively; any other value from %over replaces %base.
our sub deep-merge(%base, %over --> Hash) {
  my %result = %base;

  for %over.kv -> $key, $value {
    %result{$key} =
      (%result{$key}:exists && %result{$key} ~~ Associative && $value ~~ Associative)
        ?? deep-merge(%result{$key}, $value)
        !! $value;
  }

  %result;
}

# The data/ tree keyed by file and subdirectory name, extensions stripped.
our sub load(IO() $dir --> Hash) {
  return %() unless $dir.d;

  my %tree;

  for $dir.dir.sort(*.basename) -> $entry {
    if $entry.d {
      %tree{ $entry.basename } = load($entry);
    }
    elsif data-file($entry) {
      %tree{ $entry.extension('').basename } = parse-file($entry);
    }
  }

  %tree;
}

my sub read-dir-data(IO::Path:D $dir --> Hash) {
  my %merged;

  for <_data.json _data.yaml _data.yml> -> $name {
    my $file = $dir.add($name);

    next unless $file.e;

    my $parsed = parse-file($file);
    %merged = deep-merge(%merged, $parsed) if $parsed ~~ Associative;
  }

  %merged;
}

# Global data merged with each content directory's _data.* along the section
# path, deeper directories overriding shallower ones and both overriding global.
our sub resolve(%global, IO() $content, Str $section --> Hash) {
  my %result = %global;

  my @dirs = $content;
  my $acc  = $content;

  for $section.split('/').grep(*.chars) -> $segment {
    $acc = $acc.add($segment);
    @dirs.push($acc);
  }

  %result = deep-merge(%result, read-dir-data($_)) for @dirs;

  %result;
}
