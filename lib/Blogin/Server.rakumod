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

# A rebuild pushes a reload to every connected preview page over this channel.
class ReloadChannel is export {
  has Supplier $!supplier = Supplier.new;

  method notify(--> Nil) {
    $!supplier.emit('reload');
  }

  method Supply(--> Supply) {
    $!supplier.Supply;
  }
}

our sub reload-script(Str $path = RELOAD-PATH --> Str) is export {
  # Close the stream before navigating so the persistent connection does not
  # hold one of the browser's limited per-host sockets across page loads.
  '<script>(function(){' ~
    "var source = new EventSource(\"$path\");" ~
    'source.onmessage = function () { location.reload(); };' ~
    'window.addEventListener("pagehide", function () { source.close(); });' ~
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

# Cro's streaming body serializer requires the event Supply to emit Blobs.
our sub reload-event(--> Blob) is export {
  "data: reload\n\n".encode('utf-8');
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

sub make-app(IO() $root, Bool :$clean-urls = True, ReloadChannel :$reload) {
  route {
    if $reload.defined {
      get -> '__blogin-reload' {
        header 'Cache-Control', 'no-cache';
        content 'text/event-stream', supply {
          whenever $reload.Supply {
            emit reload-event();
          }
        }
      }
    }

    get -> *@segments {
      my %result = serve-content('/' ~ @segments.join('/'), $root, :$clean-urls);

      if %result<status> != 200 {
        response.status = 404;
        content 'text/plain', 'Not Found';
      }
      elsif $reload.defined && %result<content-type>.starts-with('text/html') {
        content %result<content-type>, inject-reload(%result<file>.slurp);
      }
      else {
        content %result<content-type>, %result<file>.slurp(:bin);
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
  IO()  :$data    = $content.parent.add('data'),
  Blogin::Config :$config = Blogin::Config.new,
  Int   :$port = 3000,
  Blogin::Log :$log = Blogin::Log.new,
) is export {
  my $config-file = $content.parent.add('blogin.json');
  my $reload      = ReloadChannel.new;

  my &rebuild = {
    my $current = Blogin::Config.load($config-file);

    Blogin::Site::build(
      :$content, :$out, :$layouts, :$static, :$data,
      debug => $current.debug,
      |$current.build-options,
    );
  };

  rebuild();

  for ($content, $layouts, $static, $data).grep({ .defined && .d }) -> $dir {
    watch-recursive($dir, {
      $log.info('change detected, rebuilding');
      rebuild-and-reload($reload, &rebuild);
      CATCH { default { $log.error("rebuild failed: { .message }") } }
    });
  }

  watch-file($config-file, {
    $log.info('config change detected, rebuilding');
    rebuild-and-reload($reload, &rebuild);
    CATCH { default { $log.error("rebuild failed: { .message }") } }
  });

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
