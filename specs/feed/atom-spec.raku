use lib 'lib';
use BDD::Behave;
use Blogin::Feed;
use XML;

describe 'the atom generator', {
  let(:feed, {
    Blogin::Feed::atom(
      title    => 'My Blog',
      site-url => 'https://example.com/',
      feed-url => 'https://example.com/feed.xml',
      updated  => '2026-07-20',
      entries  => [
        %( title => 'First', url => 'https://example.com/first', date => '2026-07-20' ),
      ],
    )
  });

  it 'is well-formed with a feed root', {
    expect(from-xml(feed()).root.name).to.eq('feed');
  }

  it 'formats the updated date as rfc3339', {
    expect(feed().contains('<updated>2026-07-20T00:00:00Z</updated>')).to.be-truthy;
  }

  it 'renders one entry per record', {
    expect(from-xml(feed()).root.elements(:TAG<entry>).elems).to.eq(1);
  }

  it 'escapes special characters in a title', {
    my $xml = Blogin::Feed::atom(title => 'A & B', entries => []);
    expect($xml.contains('A &amp; B')).to.be-truthy;
  }

  it 'falls back to the epoch when a date is empty', {
    my $xml = Blogin::Feed::atom(title => 'x', entries => [ %( title => 'y', url => '/y', date => '' ), ]);
    expect($xml.contains('1970-01-01T00:00:00Z')).to.be-truthy;
  }
}

describe 'the sitemap generator', {
  it 'is well-formed with a urlset root', {
    my $xml = Blogin::Feed::sitemap(locs => ['https://example.com/a', 'https://example.com/b']);
    expect(from-xml($xml).root.name).to.eq('urlset');
  }

  it 'emits a url per location', {
    my $xml = Blogin::Feed::sitemap(locs => ['https://example.com/a', 'https://example.com/b']);
    expect(from-xml($xml).root.elements(:TAG<url>).elems).to.eq(2);
  }

  it 'produces a well-formed empty sitemap', {
    expect(from-xml(Blogin::Feed::sitemap(locs => [])).root.name).to.eq('urlset');
  }
}
