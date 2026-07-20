use lib 'lib';
use BDD::Behave;
use Blogin::Site;
use XML;

my $PAGINATED = 'specs/fixtures/paginated'.IO;
my $seq       = 0;

sub nuke(IO::Path:D $dir) {
  return unless $dir.e;
  for $dir.dir -> $entry {
    $entry.d ?? nuke($entry) !! $entry.unlink;
  }
  $dir.rmdir;
}

sub build(IO::Path:D $out) {
  Blogin::Site::build(
    content => $PAGINATED.add('content'),
    out     => $out,
    layouts => $PAGINATED.add('layouts'),
    site    => %( title => 'Test Blog', base-url => 'https://example.com' ),
  );
}

describe 'the site-wide atom feed', {
  let(:out, { $*TMPDIR.add("blogin-feed-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  it 'writes public/feed.xml', {
    build(out());
    expect(out().add('feed.xml').e).to.be-truthy;
  }

  it 'is well-formed atom', {
    build(out());
    expect(from-xml(out().add('feed.xml').slurp).root.name).to.eq('feed');
  }

  it 'has an entry for every post across sections', {
    build(out());
    my $doc = from-xml(out().add('feed.xml').slurp);
    expect($doc.root.elements(:TAG<entry>).elems).to.eq(4);
  }

  it 'uses absolute urls from the base url', {
    build(out());
    expect(out().add('feed.xml').slurp.contains('https://example.com/posts/alpha')).to.be-truthy;
  }
}

describe 'per-section atom feeds', {
  let(:out, { $*TMPDIR.add("blogin-secfeed-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  it 'writes a feed under each section', {
    build(out());
    expect(out().add('posts/feed.xml').e && out().add('essays/feed.xml').e).to.be-truthy;
  }

  it 'limits a section feed to that section', {
    build(out());
    my $doc = from-xml(out().add('essays/feed.xml').slurp);
    expect($doc.root.elements(:TAG<entry>).elems).to.eq(1);
  }
}

describe 'the sitemap', {
  let(:out, { $*TMPDIR.add("blogin-sitemap-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  it 'writes a well-formed sitemap', {
    build(out());
    expect(from-xml(out().add('sitemap.xml').slurp).root.name).to.eq('urlset');
  }

  it 'lists post pages', {
    build(out());
    expect(out().add('sitemap.xml').slurp.contains('https://example.com/posts/alpha')).to.be-truthy;
  }

  it 'lists section listing pages', {
    build(out());
    expect(out().add('sitemap.xml').slurp.contains('https://example.com/posts</loc>')).to.be-truthy;
  }
}
