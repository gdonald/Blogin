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

sub make-app(IO() $root, Bool :$clean-urls = True) {
  route {
    get -> *@segments {
      my %result = serve-content('/' ~ @segments.join('/'), $root, :$clean-urls);

      if %result<status> == 200 {
        content %result<content-type>, %result<file>.slurp(:bin);
      }
      else {
        response.status = 404;
        content 'text/plain', 'Not Found';
      }
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

our sub serve(
  IO()  :$content!,
  IO()  :$out!,
  IO()  :$layouts = $content.parent.add('layouts'),
  IO()  :$static  = $content.parent.add('static'),
  Blogin::Config :$config = Blogin::Config.new,
  Int   :$port = 3000,
  Blogin::Log :$log = Blogin::Log.new,
) is export {
  my $config-file = $content.parent.add('blogin.json');

  my &rebuild = {
    my $current = Blogin::Config.load($config-file);

    Blogin::Site::build(
      :$content, :$out, :$layouts, :$static,
      debug => $current.debug,
      |$current.build-options,
    );
  };

  rebuild();

  for ($content, $layouts, $static).grep(*.defined) -> $dir {
    watch-recursive($dir, {
      $log.info('change detected, rebuilding');
      rebuild();
      CATCH { default { $log.error("rebuild failed: { .message }") } }
    });
  }

  my $server = Cro::HTTP::Server.new(
    host        => 'localhost',
    :$port,
    application => make-app($out, clean-urls => $config.clean-urls),
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
