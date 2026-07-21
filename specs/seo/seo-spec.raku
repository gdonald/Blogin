use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Post;
use Blogin::Layout;

my %SITE = title => 'My Site', base-url => 'https://example.com';

sub post-of(*%fields) {
  my $front = %fields.map({ "{ .key }: { .value }" }).join("\n");
  Blogin::Post.parse("---\n$front\n---\nBody text.\n", filename => 'p.md');
}

describe 'a post view head-meta', {
  let(:view, {
    Blogin::Layout::View.new(
      post => post-of(title => 'Hello', date => '2026-07-19', description => 'A short description.'),
      site => %SITE,
      url  => '/posts/hello',
    )
  });

  it 'emits a canonical link at the absolute url', {
    expect(view().head-meta.contains('<link rel="canonical" href="https://example.com/posts/hello"/>')).to.be-truthy;
  }

  it 'marks the page as an article', {
    expect(view().head-meta.contains('<meta property="og:type" content="article"/>')).to.be-truthy;
  }

  it 'sets the open graph title to the post title', {
    expect(view().head-meta.contains('<meta property="og:title" content="Hello"/>')).to.be-truthy;
  }

  it 'sets the open graph description to the post description', {
    expect(view().head-meta.contains('<meta property="og:description" content="A short description."/>')).to.be-truthy;
  }

  it 'sets the open graph site name to the site title', {
    expect(view().head-meta.contains('<meta property="og:site_name" content="My Site"/>')).to.be-truthy;
  }

  it 'emits a twitter summary card', {
    expect(view().head-meta.contains('<meta name="twitter:card" content="summary"/>')).to.be-truthy;
  }

  it 'emits a plain meta description', {
    expect(view().head-meta.contains('<meta name="description" content="A short description."/>')).to.be-truthy;
  }
}

describe 'a post with no description', {
  let(:view, {
    Blogin::Layout::View.new(
      post    => post-of(title => 'Hello', date => '2026-07-19'),
      site    => %SITE,
      url     => '/posts/hello',
      summary => 'The derived summary.',
    )
  });

  it 'falls back to the derived summary for the description', {
    expect(view().head-meta.contains('<meta property="og:description" content="The derived summary."/>')).to.be-truthy;
  }
}

describe 'escaping in head-meta', {
  it 'escapes a quote in the title', {
    my $view = Blogin::Layout::View.new(
      post => post-of(title => 'A "quoted" title', date => '2026-07-19'),
      site => %SITE,
      url  => '/posts/q',
    );
    expect($view.head-meta.contains('content="A &quot;quoted&quot; title"')).to.be-truthy;
  }
}

describe 'a listing view head-meta', {
  let(:view, {
    Blogin::Layout::ListView.new(site => %SITE, section => 'posts', url => '/posts')
  });

  it 'marks the listing as a website', {
    expect(view().head-meta.contains('<meta property="og:type" content="website"/>')).to.be-truthy;
  }

  it 'titles the listing with the section label', {
    expect(view().head-meta.contains('<meta property="og:title" content="Posts"/>')).to.be-truthy;
  }

  it 'sets the canonical link to the listing url', {
    expect(view().head-meta.contains('<link rel="canonical" href="https://example.com/posts"/>')).to.be-truthy;
  }
}

describe 'seo tags and robots through a build', {
  my $FIXTURE = 'specs/fixtures/seo'.IO;

  let(:out, { temp-dir('seo') });

  after-each { nuke(out()) }

  it 'writes the open graph tags into a built post page', {
    build-fixture($FIXTURE, out(), site => %SITE);
    expect(out().add('posts/hello.html').slurp.contains('property="og:title"')).to.be-truthy;
  }

  it 'writes a robots.txt', {
    build-fixture($FIXTURE, out(), site => %SITE);
    expect(out().add('robots.txt').e).to.be-truthy;
  }

  it 'points robots.txt at the sitemap using the base url', {
    build-fixture($FIXTURE, out(), site => %SITE);
    expect(out().add('robots.txt').slurp.contains('Sitemap: https://example.com/sitemap.xml')).to.be-truthy;
  }

  it 'omits robots.txt when robots is off', {
    build-fixture($FIXTURE, out(), site => %SITE, robots => False);
    expect(out().add('robots.txt').e).to.be-falsy;
  }
}
