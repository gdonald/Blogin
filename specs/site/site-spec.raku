use lib 'lib';
use BDD::Behave;
use Blogin::Site;

my $BASIC = 'specs/fixtures/basic'.IO;
my $seq   = 0;

sub nuke(IO::Path:D $dir) {
  return unless $dir.e;
  for $dir.dir -> $entry {
    $entry.d ?? nuke($entry) !! $entry.unlink;
  }
  $dir.rmdir;
}

sub read-tree(IO::Path:D $dir) {
  my %files;

  sub walk(IO::Path:D $node, Str $prefix) {
    for $node.dir.sort(*.basename) -> $entry {
      my $rel = $prefix eq '' ?? $entry.basename !! "$prefix/{ $entry.basename }";
      $entry.d ?? walk($entry, $rel) !! (%files{$rel} = $entry.slurp);
    }
  }

  walk($dir, '');
  %files;
}

sub build-basic(IO::Path:D $out, *%options) {
  Blogin::Site::build(
    content => $BASIC.add('content'),
    out     => $out,
    layouts => $BASIC.add('layouts'),
    static  => $BASIC.add('static'),
    |%options,
  );
}

describe 'building the basic fixture site', {
  let(:out, { $*TMPDIR.add("blogin-site-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  it 'renders a section post to its section path', {
    build-basic(out());
    expect(out().add('posts/hello.html').e).to.be-truthy;
  }

  it 'renders a root page at the top level', {
    build-basic(out());
    expect(out().add('about.html').e).to.be-truthy;
  }

  it 'renders the post title into the page', {
    build-basic(out());
    expect(out().add('posts/hello.html').slurp.contains('<h1>Hello</h1>')).to.be-truthy;
  }

  it 'injects the rendered markdown body', {
    build-basic(out());
    expect(out().add('posts/hello.html').slurp.contains('<strong>world</strong>')).to.be-truthy;
  }

  it 'copies static assets verbatim', {
    build-basic(out());
    expect(out().add('style.css').slurp.contains('font-family')).to.be-truthy;
  }

  it 'reports the rendered non-draft pages', {
    my $result = build-basic(out());
    expect($result.written.elems).to.eq(3);
  }
}

describe 'drafts', {
  let(:out, { $*TMPDIR.add("blogin-draft-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  it 'excludes drafts by default', {
    build-basic(out());
    expect(out().add('posts/draft-post.html').e).to.be-falsy;
  }

  it 'includes drafts with the drafts flag', {
    build-basic(out(), drafts => True);
    expect(out().add('posts/draft-post.html').e).to.be-truthy;
  }
}

describe 'rebuilds', {
  let(:out, { $*TMPDIR.add("blogin-rebuild-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  it 'wipes stale output that the current build does not produce', {
    build-basic(out());
    out().add('orphan.html').spurt('stale');

    build-basic(out());

    expect(out().add('orphan.html').e).to.be-falsy;
  }
}

describe 'deterministic output', {
  let(:one, { $*TMPDIR.add("blogin-j1-{$*PID}-{$seq++}") });
  let(:many, { $*TMPDIR.add("blogin-jn-{$*PID}-{$seq++}") });

  after-each {
    nuke(one());
    nuke(many());
  }

  it 'produces byte-identical files regardless of job count', {
    build-basic(one(), jobs => 1);
    build-basic(many(), jobs => 4);

    expect(read-tree(one()) eqv read-tree(many())).to.be-truthy;
  }
}

describe 'a url collision', {
  let(:out, { $*TMPDIR.add("blogin-collision-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  it 'raises naming both source files', {
    try Blogin::Site::build(
      content => 'specs/fixtures/collision/content'.IO,
      out     => out(),
      layouts => $BASIC.add('layouts'),
    );

    expect($!.message.contains('a.md') && $!.message.contains('b.md')).to.be-truthy;
  }
}
