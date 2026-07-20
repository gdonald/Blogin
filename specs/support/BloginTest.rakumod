use v6.d;

use Blogin::Site;

unit module BloginTest;

my $seq = 0;

our sub nuke(IO::Path:D $dir) is export {
  return unless $dir.e;

  for $dir.dir -> $entry {
    $entry.d ?? nuke($entry) !! $entry.unlink;
  }

  $dir.rmdir;
}

our sub temp-dir(Str $tag --> IO::Path) is export {
  $*TMPDIR.add("blogin-$tag-{ $*PID }-{ $seq++ }");
}

our sub temp-made(Str $tag --> IO::Path) is export {
  my $dir = temp-dir($tag);
  $dir.mkdir;
  $dir;
}

our sub build-fixture(IO::Path:D $fixture, IO::Path:D $out, *%options) is export {
  Blogin::Site::build(
    content => $fixture.add('content'),
    out     => $out,
    layouts => $fixture.add('layouts'),
    static  => $fixture.add('static'),
    |%options,
  );
}
