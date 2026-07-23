use v6.d;

use Cro::HTTP::Router;
use Cro::HTTP::Server;
use Blogin::Site;
use Blogin::Config;
use Blogin::Log;

unit module Blogin::Server;

my %CONTENT-TYPES =
  html => 'text/html; charset=utf-8',
  css  => 'text/css',
  js   => 'application/javascript',
  json => 'application/json',
  xml  => 'application/xml',
  svg  => 'image/svg+xml',
  png  => 'image/png',
  jpg  => 'image/jpeg',
  jpeg => 'image/jpeg',
  gif  => 'image/gif',
  ico  => 'image/x-icon',
  txt  => 'text/plain',
  ;

our sub content-type-for(IO::Path:D $file --> Str) is export {
  %CONTENT-TYPES{ $file.extension.lc } // 'application/octet-stream';
}

constant RELOAD-PATH = '/__blogin-reload';

# A build version bumped on each rebuild. The preview page long-polls it: the
# request parks until the version changes or the wait times out, so the held
# connection sits idle rather than being written to. A browser that navigates
# away closes an idle socket, which the server is never mid-write on, so no
# "closed socket" noise.
class ReloadChannel is export {
  has Int  $.version = 0;
  has Lock $!lock = Lock.new;
  has      $!next-change = Promise.new;
  has      $!next-vow;

  submethod TWEAK { $!next-vow = $!next-change.vow; }

  method notify(--> Nil) {
    $!lock.protect({
      $!version++;
      my $vow = $!next-vow;
      $!next-change = Promise.new;
      $!next-vow    = $!next-change.vow;
      $vow.keep($!version);
    });
  }

  # Return the current version once it differs from what the caller last saw, or
  # after $timeout seconds, whichever comes first.
  method wait-for-change(Int() $known, Real :$timeout = 25 --> Int) {
    my $change;

    $!lock.protect({
      return $!version if $!version != $known;
      $change = $!next-change;
    });

    await Promise.anyof($change, Promise.in($timeout));
    $!version;
  }
}

our sub reload-script(Str $path = RELOAD-PATH --> Str) is export {
  '<script>(function () {' ~
    'var known = null;' ~
    'function poll() {' ~
      'var url = ' ~ "\"$path\"" ~ ' + (known === null ? "" : "?v=" + encodeURIComponent(known));' ~
      'fetch(url).then(function (r) { return r.text(); }).then(function (v) {' ~
        'if (known === null) { known = v; poll(); }' ~
        'else if (v !== known) { location.reload(); }' ~
        'else { poll(); }' ~
      '}).catch(function () { setTimeout(poll, 1000); });' ~
    '}' ~
    'poll();' ~
  '})();</script>';
}

# Insert the reload client before the closing body tag, or append it when the
# page has none.
our sub inject-reload(Str $html, Str $script = reload-script() --> Str) is export {
  return $html ~ $script unless $html.contains('</body>');

  $html.subst('</body>', $script ~ '</body>');
}

our sub rebuild-and-reload(ReloadChannel $channel, &rebuild --> Nil) is export {
  rebuild();
  $channel.notify;
}

# Map a request path to a file on disk, mirroring how a static host rewrites
# extensionless URLs to their `.html` files.
our sub resolve-file(Str $url-path, IO() $root, Bool :$clean-urls = True --> IO::Path) is export {
  my $rel = $url-path;
  $rel ~~ s/ ^ '/' //;
  $rel ~~ s/ '?' .* $ //;

  $rel = 'index.html'      if $rel eq '';
  $rel = $rel ~ 'index.html' if $rel.ends-with('/');

  my $direct = $root.add($rel);
  return $direct if $direct.e && $direct.f;

  unless $rel.contains('.') {
    my $html = $root.add("$rel.html");
    return $html if $html.e && $html.f;

    my $index = $root.add($rel).add('index.html');
    return $index if $index.e && $index.f;
  }

  IO::Path;
}

our sub serve-content(Str $url-path, IO() $root, Bool :$clean-urls = True --> Hash) is export {
  my $file = resolve-file($url-path, $root, :$clean-urls);

  return %( status => 404, content-type => 'text/plain', file => IO::Path )
    unless $file.defined;

  %( status => 200, content-type => content-type-for($file), file => $file );
}

# Resolve a request to what should be served: status, content-type, and body.
# HTML bodies get the reload client injected when $inject is set; other files
# are served as raw bytes.
our sub render-response(Str $url-path, IO() $root, Bool :$clean-urls = True, Bool :$inject = False --> Hash) is export {
  my %result = serve-content($url-path, $root, :$clean-urls);

  return %( status => 404, content-type => 'text/plain', body => 'Not Found' )
    if %result<status> != 200;

  return %( status => 200, content-type => %result<content-type>, body => inject-reload(%result<file>.slurp) )
    if $inject && %result<content-type>.starts-with('text/html');

  %( status => 200, content-type => %result<content-type>, body => %result<file>.slurp(:bin) );
}

sub make-app(IO() $root, Bool :$clean-urls = True, ReloadChannel :$reload) {
  route {
    if $reload.defined {
      get -> '__blogin-reload' {
        my $known = (request.query-value('v') // -1).Int;
        my $current = $reload.wait-for-change($known);
        header 'Cache-Control', 'no-cache';
        content 'text/plain', $current.Str;
      }
    }

    get -> *@segments {
      my %response = render-response('/' ~ @segments.join('/'), $root, :$clean-urls, inject => $reload.defined);

      response.status = %response<status>;
      content %response<content-type>, %response<body>;
    }
  }
}

sub watch-recursive(IO::Path:D $dir, &on-change) {
  return unless $dir.d;

  my @dirs = $dir, |$dir.dir(:r).grep(*.d);

  for @dirs -> $target {
    start react {
      whenever IO::Notification.watch-path($target.absolute) {
        on-change();
      }
    }
  }
}

# Watch a single file by watching its directory and filtering to the file's
# basename, so an editor's write-and-rename save still fires.
sub watch-file(IO::Path:D $file, &on-change) {
  return unless $file.parent.d;

  start react {
    whenever IO::Notification.watch-path($file.parent.absolute) -> $change {
      on-change() if $change.path.IO.basename eq $file.basename;
    }
  }
}

our sub serve(
  IO()  :$content!,
  IO()  :$out!,
  IO()  :$layouts = $content.parent.add('layouts'),
  IO()  :$static  = $content.parent.add('static'),
  IO()  :$assets  = $content.parent.add('assets'),
  IO()  :$data    = $content.parent.add('data'),
  IO()  :$shortcodes = $content.parent.add('shortcodes'),
  Blogin::Config :$config = Blogin::Config.new,
  Int   :$port = 3000,
  Blogin::Log :$log = Blogin::Log.new,
) is export {
  my $config-file  = $content.parent.add('blogin.json');
  my $reload       = ReloadChannel.new;
  my $rebuild-lock = Lock.new;

  my &rebuild = {
    my $current = Blogin::Config.load($config-file);

    Blogin::Site::build(
      :$content, :$out, :$layouts, :$static, :$assets, :$data, :$shortcodes,
      debug => $current.debug,
      |$current.build-options,
    );
  };

  # Serialize rebuilds so overlapping file-watch events cannot race on the
  # output (for example the fingerprint rename pass).
  my &on-change = {
    $rebuild-lock.protect({
      $log.info('change detected, rebuilding');
      rebuild-and-reload($reload, &rebuild);
      CATCH { default { $log.error("rebuild failed: { .message }") } }
    });
  };

  rebuild();

  for ($content, $layouts, $static, $assets, $data, $shortcodes).grep({ .defined && .d }) -> $dir {
    watch-recursive($dir, &on-change);
  }

  watch-file($config-file, &on-change);

  my $server = Cro::HTTP::Server.new(
    host        => 'localhost',
    :$port,
    application => make-app($out, clean-urls => $config.clean-urls, :$reload),
  );

  $server.start;
  $log.info("serving { $out } on http://localhost:$port (Ctrl-C to stop)");

  react {
    whenever signal(SIGINT) {
      $log.info('stopping');
      $server.stop;
      done;
    }
  }
}
