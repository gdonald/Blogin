use lib 'lib';
use BDD::Behave;
use Blogin::Server;
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

sub build-basic(IO::Path:D $out) {
  Blogin::Site::build(
    content => $BASIC.add('content'),
    out     => $out,
    layouts => $BASIC.add('layouts'),
    static  => $BASIC.add('static'),
    home-section => 'posts',
  );
}

describe 'resolving a request path to a file', {
  let(:out, { $*TMPDIR.add("blogin-serve-{$*PID}-{$seq++}") });

  before-each { build-basic(out()) }
  after-each  { nuke(out()) }

  it 'serves index.html for the root', {
    expect(Blogin::Server::resolve-file('/', out()).basename).to.eq('index.html');
  }

  it 'resolves an extensionless post url to its html file', {
    expect(Blogin::Server::resolve-file('/posts/hello', out()).basename).to.eq('hello.html');
  }

  it 'serves a static asset directly', {
    expect(Blogin::Server::resolve-file('/style.css', out()).basename).to.eq('style.css');
  }

  it 'ignores a query string', {
    expect(Blogin::Server::resolve-file('/posts/hello?x=1', out()).basename).to.eq('hello.html');
  }

  it 'returns an undefined path for a missing url', {
    expect(Blogin::Server::resolve-file('/nope', out()).defined).to.be-falsy;
  }
}

describe 'serving content', {
  let(:out, { $*TMPDIR.add("blogin-content-{$*PID}-{$seq++}") });

  before-each { build-basic(out()) }
  after-each  { nuke(out()) }

  it 'serves the built index with a 200', {
    expect(Blogin::Server::serve-content('/', out())<status>).to.eq(200);
  }

  it 'serves an extensionless post page', {
    my %result = Blogin::Server::serve-content('/posts/hello', out());
    expect(%result<file>.slurp.contains('<h1>Hello</h1>')).to.be-truthy;
  }

  it 'uses the html content type for a page', {
    expect(Blogin::Server::serve-content('/posts/hello', out())<content-type>.contains('text/html')).to.be-truthy;
  }

  it 'returns a 404 for a missing url', {
    expect(Blogin::Server::serve-content('/nope', out())<status>).to.eq(404);
  }
}

describe 'the rebuild-and-serve seam', {
  let(:src, { $*TMPDIR.add("blogin-src-{$*PID}-{$seq++}") });
  let(:out, { $*TMPDIR.add("blogin-rebuilt-{$*PID}-{$seq++}") });

  before-each {
    src().add('posts').mkdir;
    src().add('posts/2026-07-19-note.md').spurt("---\ntitle: Note\ndate: 2026-07-19\n---\noriginal body\n");
  }

  after-each {
    nuke(src());
    nuke(out());
  }

  it 'serves the original content after the first build', {
    Blogin::Site::build(content => src(), out => out(), layouts => $BASIC.add('layouts'));
    expect(Blogin::Server::serve-content('/posts/note', out())<file>.slurp.contains('original body')).to.be-truthy;
  }

  it 'serves the edited content after a rebuild', {
    Blogin::Site::build(content => src(), out => out(), layouts => $BASIC.add('layouts'));

    src().add('posts/2026-07-19-note.md').spurt("---\ntitle: Note\ndate: 2026-07-19\n---\nchanged body\n");

    Blogin::Site::build(content => src(), out => out(), layouts => $BASIC.add('layouts'));

    expect(Blogin::Server::serve-content('/posts/note', out())<file>.slurp.contains('changed body')).to.be-truthy;
  }
}
