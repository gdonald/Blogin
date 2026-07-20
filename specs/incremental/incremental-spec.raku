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

sub build(IO::Path:D $content, IO::Path:D $out, *%options) {
  Blogin::Site::build(
    content => $content,
    out     => $out,
    layouts => $BASIC.add('layouts'),
    home-section => 'posts',
    site => %( title => 'Test', base-url => 'https://example.com' ),
    |%options,
  );
}

sub seed-content(IO::Path:D $content) {
  $content.add('posts').mkdir;
  $content.add('posts/2026-07-19-alpha.md').spurt("---\ntitle: Alpha\ndate: 2026-07-19\ntags: [x]\n---\nAlpha body\n");
  $content.add('posts/2026-07-18-bravo.md').spurt("---\ntitle: Bravo\ndate: 2026-07-18\ntags: [y]\n---\nBravo body\n");
}

sub rel-log(@files, IO::Path:D $out) {
  @files.map({ .relative($out) }).sort;
}

describe 'incremental rebuilds', {
  let(:content, { my $d = $*TMPDIR.add("blogin-inc-src-{$*PID}-{$seq++}"); $d.mkdir; $d });
  let(:out,     { $*TMPDIR.add("blogin-inc-out-{$*PID}-{$seq++}") });

  before-each { seed-content(content()) }

  after-each {
    nuke(content());
    nuke(out());
  }

  it 'rewrites nothing when the source is unchanged', {
    build(content(), out());
    my $result = build(content(), out());
    expect($result.write-log.elems).to.eq(0);
  }

  it 'rewrites the changed post', {
    build(content(), out());
    content().add('posts/2026-07-19-alpha.md').spurt("---\ntitle: Alpha\ndate: 2026-07-19\ntags: [x]\n---\nAlpha body edited\n");

    my $result = build(content(), out());
    expect(rel-log($result.write-log, out()).grep('posts/alpha.html').elems).to.eq(1);
  }

  it 'leaves an unrelated post alone', {
    build(content(), out());
    content().add('posts/2026-07-19-alpha.md').spurt("---\ntitle: Alpha\ndate: 2026-07-19\ntags: [x]\n---\nAlpha body edited\n");

    my $result = build(content(), out());
    expect(rel-log($result.write-log, out()).grep('posts/bravo.html').elems).to.eq(0);
  }

  it 'rewrites the search index when a body changes', {
    build(content(), out());
    content().add('posts/2026-07-19-alpha.md').spurt("---\ntitle: Alpha\ndate: 2026-07-19\ntags: [x]\n---\nAlpha body edited\n");

    my $result = build(content(), out());
    expect(rel-log($result.write-log, out()).grep('search-index.json').elems).to.eq(1);
  }

  it 'rewrites the listing and feed when a title changes', {
    build(content(), out());
    content().add('posts/2026-07-19-alpha.md').spurt("---\ntitle: Alpha Renamed\ndate: 2026-07-19\ntags: [x]\n---\nAlpha body\n");

    my $result = build(content(), out());
    my @log = rel-log($result.write-log, out());
    expect(@log.grep('posts.html').elems == 1 && @log.grep('feed.xml').elems == 1).to.be-truthy;
  }

  it 'prunes the output of a deleted post', {
    build(content(), out());
    content().add('posts/2026-07-18-bravo.md').unlink;

    build(content(), out());
    expect(out().add('posts/bravo.html').e).to.be-falsy;
  }

  it 'rewrites everything under force', {
    build(content(), out());
    my $result = build(content(), out(), force => True);
    expect($result.write-log.elems).to.be-greater-than(0);
  }
}
