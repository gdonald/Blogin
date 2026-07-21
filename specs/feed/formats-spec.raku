use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use JSON::Fast;
use Blogin::Feed;
use Blogin::Config;

my $ENTRY = %( title => 'Post', url => 'https://e.com/p', date => '2026-07-19', summary => 'A teaser' );

describe 'Blogin::Feed::rss', {
  let(:xml, {
    Blogin::Feed::rss(
      title    => 'My Site',
      site-url => 'https://e.com/',
      feed-url => 'https://e.com/rss.xml',
      updated  => '2026-07-19',
      entries  => [ $ENTRY ],
    )
  });

  it 'declares rss 2.0', {
    expect(xml().contains('<rss version="2.0">')).to.be-truthy;
  }

  it 'wraps the items in a channel', {
    expect(xml().contains('<channel>')).to.be-truthy;
  }

  it 'renders the item title', {
    expect(xml().contains('<title>Post</title>')).to.be-truthy;
  }

  it 'links the item', {
    expect(xml().contains('<link>https://e.com/p</link>')).to.be-truthy;
  }

  it 'formats the publication date as rfc-822', {
    expect(xml().contains('19 Jul 2026 00:00:00 +0000')).to.be-truthy;
  }

  it 'uses the summary as the item description', {
    expect(xml().contains('<description>A teaser</description>')).to.be-truthy;
  }
}

describe 'Blogin::Feed::json-feed', {
  let(:feed, {
    from-json(Blogin::Feed::json-feed(
      title    => 'My Site',
      site-url => 'https://e.com/',
      feed-url => 'https://e.com/feed.json',
      updated  => '2026-07-19',
      entries  => [ $ENTRY ],
    ))
  });

  it 'declares the json feed version', {
    expect(feed()<version>).to.eq('https://jsonfeed.org/version/1.1');
  }

  it 'sets the feed title', {
    expect(feed()<title>).to.eq('My Site');
  }

  it 'sets the home page url', {
    expect(feed()<home_page_url>).to.eq('https://e.com/');
  }

  it 'includes the item title', {
    expect(feed()<items>[0]<title>).to.eq('Post');
  }

  it 'formats the item date as rfc-3339', {
    expect(feed()<items>[0]<date_published>).to.eq('2026-07-19T00:00:00Z');
  }

  it 'uses the summary as the item content text', {
    expect(feed()<items>[0]<content_text>).to.eq('A teaser');
  }
}

describe 'feed formats through a build', {
  my $BASIC = 'specs/fixtures/basic'.IO;

  let(:out, {
    my $dir = temp-dir('feeds');
    build-fixture($BASIC, $dir, site => %( base-url => 'https://e.com', title => 'T' ), feed-formats => ['atom', 'rss', 'json']);
    $dir
  });

  after-each { nuke(out()) }

  it 'writes the site atom feed', {
    expect(out().add('feed.xml').e).to.be-truthy;
  }

  it 'writes the site rss feed', {
    expect(out().add('rss.xml').slurp.contains('<rss version="2.0">')).to.be-truthy;
  }

  it 'writes the site json feed', {
    expect(from-json(out().add('feed.json').slurp)<version>).to.eq('https://jsonfeed.org/version/1.1');
  }

  it 'writes a per-section rss feed', {
    expect(out().add('posts/rss.xml').e).to.be-truthy;
  }

  it 'writes a per-section json feed', {
    expect(out().add('posts/feed.json').e).to.be-truthy;
  }
}

describe 'feed formats defaulting to atom only', {
  my $BASIC = 'specs/fixtures/basic'.IO;

  let(:out, {
    my $dir = temp-dir('feeds-default');
    build-fixture($BASIC, $dir, site => %( base-url => 'https://e.com' ));
    $dir
  });

  after-each { nuke(out()) }

  it 'writes the atom feed by default', {
    expect(out().add('feed.xml').e).to.be-truthy;
  }

  it 'does not write an rss feed by default', {
    expect(out().add('rss.xml').e).to.be-falsy;
  }
}

describe 'feed formats from config options', {
  my $BASIC = 'specs/fixtures/basic'.IO;

  let(:out, {
    my $dir    = temp-dir('feeds-config');
    my $config = Blogin::Config.from-data(%( base-url => 'https://e.com', feed-formats => ['atom', 'json'] ));
    build-fixture($BASIC, $dir, |$config.build-options);
    $dir
  });

  after-each { nuke(out()) }

  it 'writes the json feed selected in config', {
    expect(out().add('feed.json').e).to.be-truthy;
  }
}
