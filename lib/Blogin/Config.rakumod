use v6.d;

use JSON::Fast;

unit class Blogin::Config;

has Str  $.title         = '';
has Str  $.base-url      = '';
has Str  $.output-dir    = 'public';
has Int  $.page-size     = 10;
has Str  $.author        = '';
has Str  $.home-section  = '';
has Bool $.clean-urls    = True;
has Str  $.css-framework = 'none';
has Bool $.debug         = False;
has Bool $.search             = True;
has Int  $.search-text-length = 2000;
has Int  $.search-cap         = 10;
has Bool $.highlight          = False;
has Int  $.summary-length     = 200;
has Bool $.robots             = True;
has Bool $.minify             = False;
has Bool $.fingerprint        = False;
has      @.image-widths       = [];
has Int  $.reading-wpm        = 200;
has Int  $.related-count      = 5;
has      @.taxonomies         = ['tags'];
has      @.feed-formats       = ['atom'];
has      %.sections;

my sub want-str($value, Str $key) {
  die "config key '$key' must be a string" unless $value ~~ Str;
  $value;
}

my sub want-int($value, Str $key) {
  die "config key '$key' must be an integer" unless $value ~~ Int;
  $value;
}

my sub want-bool($value, Str $key) {
  die "config key '$key' must be a boolean" unless $value ~~ Bool;
  $value;
}

my sub want-str-list($value, Str $key) {
  die "config key '$key' must be a list of strings" unless $value ~~ Positional;

  my @strings;
  for @($value) -> $item {
    die "config key '$key' must be a list of strings" unless $item ~~ Str;
    @strings.push($item);
  }

  @strings;
}

my sub want-int-list($value, Str $key) {
  die "config key '$key' must be a list of integers" unless $value ~~ Positional;

  my @ints;
  for @($value) -> $item {
    die "config key '$key' must be a list of integers" unless $item ~~ Int;
    @ints.push($item);
  }

  @ints;
}

my sub want-feed-formats($value) {
  my @formats = want-str-list($value, 'feed-formats');

  for @formats -> $format {
    die "config key 'feed-formats' has an unknown format '$format' (use atom, rss, or json)"
      unless $format eq any(<atom rss json>);
  }

  @formats;
}

my sub validate-sections(%sections) {
  for %sections.kv -> $name, $entry {
    die "config section '$name' must be a map" unless $entry ~~ Associative;

    want-int($_,  "sections.$name.page-size")   with $entry<page-size>;
    want-str($_,  "sections.$name.label")       with $entry<label>;
    want-int($_,  "sections.$name.order")       with $entry<order>;
    want-bool($_, "sections.$name.nav")         with $entry<nav>;
    want-str($_,  "sections.$name.layout")      with $entry<layout>;
    want-bool($_, "sections.$name.index-dates") with $entry<index-dates>;
    want-bool($_, "sections.$name.show-dates")  with $entry<show-dates>;
  }

  %sections;
}

method from-data(Blogin::Config:U: %data --> Blogin::Config) {
  my %args;

  %args<title>         = want-str($_,  'title')         with %data<title>;
  %args<base-url>      = want-str($_,  'base-url')      with %data<base-url>;
  %args<output-dir>    = want-str($_,  'output-dir')    with %data<output-dir>;
  %args<page-size>     = want-int($_,  'page-size')     with %data<page-size>;
  %args<author>        = want-str($_,  'author')        with %data<author>;
  %args<home-section>  = want-str($_,  'home-section')  with %data<home-section>;
  %args<clean-urls>    = want-bool($_, 'clean-urls')    with %data<clean-urls>;
  %args<css-framework> = want-str($_,  'css-framework') with %data<css-framework>;
  %args<debug>         = want-bool($_, 'debug')         with %data<debug>;
  %args<search>             = want-bool($_, 'search')             with %data<search>;
  %args<search-text-length> = want-int($_,  'search-text-length') with %data<search-text-length>;
  %args<search-cap>         = want-int($_,  'search-cap')         with %data<search-cap>;
  %args<highlight>          = want-bool($_, 'highlight')          with %data<highlight>;
  %args<summary-length>     = want-int($_,  'summary-length')     with %data<summary-length>;
  %args<robots>             = want-bool($_, 'robots')             with %data<robots>;
  %args<minify>             = want-bool($_, 'minify')             with %data<minify>;
  %args<fingerprint>        = want-bool($_, 'fingerprint')        with %data<fingerprint>;
  %args<image-widths>      := want-int-list($_, 'image-widths')    with %data<image-widths>;
  %args<reading-wpm>        = want-int($_,  'reading-wpm')         with %data<reading-wpm>;
  %args<related-count>      = want-int($_,  'related-count')       with %data<related-count>;
  %args<taxonomies>        := want-str-list($_, 'taxonomies')      with %data<taxonomies>;
  %args<feed-formats>      := want-feed-formats($_)                with %data<feed-formats>;
  %args<sections>      = validate-sections($_)          with %data<sections>;

  self.new(|%args);
}

method load(Blogin::Config:U: IO() $path --> Blogin::Config) {
  return self.new unless $path.e;

  my %data;

  {
    %data = from-json($path.slurp);
    CATCH { default { die "malformed config '{ $path }': { .message }" } }
  }

  self.from-data(%data);
}

# The config-derived named arguments for Blogin::Site::build (excludes debug,
# which callers resolve against CLI overrides).
method build-options(--> Hash) {
  %(
    site               => %( title => $!title, base-url => $!base-url, author => $!author ),
    clean-urls         => $!clean-urls,
    framework          => $!css-framework,
    page-size          => $!page-size,
    home-section       => $!home-section,
    sections           => %!sections,
    search             => $!search,
    search-text-length => $!search-text-length,
    search-cap         => $!search-cap,
    highlight          => $!highlight,
    summary-length     => $!summary-length,
    robots             => $!robots,
    minify             => $!minify,
    fingerprint        => $!fingerprint,
    image-widths       => @!image-widths,
    reading-wpm        => $!reading-wpm,
    related-count      => $!related-count,
    taxonomies         => @!taxonomies,
    feed-formats       => @!feed-formats,
  );
}
