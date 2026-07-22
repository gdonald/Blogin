use v6.d;

use Blogin::Log;
use Blogin::Site;
use Blogin::Config;

unit module Blogin;

our constant VERSION = '0.0.1';

our sub build(
  IO()           :$src!,
  Blogin::Config :$config = Blogin::Config.new,
  IO()           :$out,
  Bool           :$drafts = False,
  Bool           :$future = False,
  Int            :$jobs = ($*KERNEL.cpu-cores // 1),
  Bool           :$debug,
  Bool           :$force = False,
  Blogin::Log    :$log = Blogin::Log.new,
  --> Bool
) is export {
  my $out-dir = $out // $config.output-dir.IO;

  if $config.languages.elems {
    build-languages($src, $out-dir, $config, :$drafts, :$future, :$jobs, :$force, :$debug, :$log);
    return True;
  }

  my ($theme-layouts, $theme-static, $theme-assets) = theme-dirs($src, $config);

  my $result = Blogin::Site::build(
    content => $src,
    out     => $out-dir,
    debug   => ($debug // $config.debug),
    |$config.build-options,
    :$theme-layouts, :$theme-static, :$theme-assets,
    :$drafts,
    :$future,
    :$jobs,
    :$force,
  );

  run-plugins($config, $out-dir, $result);

  $log.verbose("wrote { $result.written.elems } pages to { $out-dir }");

  True;
}

# A theme lives under themes/<name>/ with its own layouts, static, and assets,
# used as fallbacks behind the site's own files.
sub theme-dirs(IO() $src, Blogin::Config $config) {
  return (Nil, Nil, Nil) unless $config.theme.chars;

  my $root = $src.parent.add('themes').add($config.theme);

  ($root.add('layouts'), $root.add('static'), $root.add('assets'));
}

# Each plugin module exposes `our sub blogin-emit(%context)` and may write extra
# output. %context has the output dir, the rendered pages, and the config.
sub run-plugins(Blogin::Config $config, IO() $out, $result) {
  for $config.plugins -> $name {
    require ::($name);

    my &hook = ::($name).WHO<&blogin-emit>;

    hook(%( out => $out, pages => $result.rendered, config => $config )) if &hook;
  }
}

sub root-redirect-html(Str $target --> Str) {
  qq:to/HTML/;
  <!doctype html>
  <html lang="en">
  <head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="0; url=$target">
  <link rel="canonical" href="$target">
  <title>Redirecting</title>
  </head>
  <body>
  <p>Redirecting to <a href="$target">{ $target }</a>.</p>
  </body>
  </html>
  HTML
}

# Build each language into its own /<code>/ subtree, sharing layouts and assets,
# with a cross-language switcher and a root redirect to the default language.
sub build-languages(
  IO() $src, IO() $out-dir, Blogin::Config $config,
  Bool :$drafts, Bool :$future, Int :$jobs, Bool :$force, Bool :$debug, Blogin::Log :$log,
) {
  my @langs = $config.languages;
  my $root  = $src.parent;

  my ($theme-layouts, $theme-static, $theme-assets) = theme-dirs($src, $config);

  my %lang-paths = @langs.map(-> $code {
    $code => Blogin::Site::url-paths($src.add($code), :$drafts, :$future)
  }).hash;

  for @langs -> $code {
    my %site = $config.build-options<site>.clone;
    my %overrides = ($config.language-config{$code} // %()).hash;
    %site<title> = %overrides<title> if %overrides<title>:exists;

    my $result = Blogin::Site::build(
      content => $src.add($code),
      out     => $out-dir.add($code),
      layouts => $root.add('layouts'),
      static  => $root.add('static'),
      assets  => $root.add('assets'),
      data    => $root.add('data'),
      shortcodes => $root.add('shortcodes'),
      debug   => ($debug // $config.debug),
      |$config.build-options,
      :%site,
      :$theme-layouts, :$theme-static, :$theme-assets,
      url-prefix       => "/$code",
      languages        => @langs,
      current-language => $code,
      lang-paths       => %lang-paths,
      :$drafts, :$future, :$jobs, :$force,
    );

    run-plugins($config, $out-dir.add($code), $result);
  }

  $out-dir.mkdir;
  $out-dir.add('index.html').spurt(root-redirect-html("/{ @langs[0] }/"));

  $log.verbose("wrote { @langs.elems } language trees to { $out-dir }");
}

our sub clean(
  IO()        :$out!,
  IO()        :$root = $*CWD,
  Blogin::Log :$log = Blogin::Log.new,
  --> Int
) is export {
  my $removed = Blogin::Site::clean(:$out, :$root);

  $log.verbose("cleaned { $out } ($removed files)");

  $removed;
}
