use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Site;

my $PAGINATED = 'specs/fixtures/paginated'.IO;


sub build(IO::Path:D $out) {
  Blogin::Site::build(
    content => $PAGINATED.add('content'),
    out     => $out,
    layouts => $PAGINATED.add('layouts'),
  );
}

describe 'per-tag pages', {
  let(:out, { temp-dir('tags') });

  after-each { nuke(out()) }

  it 'writes a page per tag under tags/', {
    build(out());
    expect(out().add('tags/raku.html').e).to.be-truthy;
  }

  it 'aggregates posts across sections on a tag page', {
    build(out());
    my $html = out().add('tags/raku.html').slurp;
    expect($html.contains("href='/posts/alpha'")
        && $html.contains("href='/posts/bravo'")
        && $html.contains("href='/essays/first-essay'")).to.be-truthy;
  }

  it 'excludes posts that lack the tag', {
    build(out());
    expect(out().add('tags/raku.html').slurp.contains("href='/posts/charlie'")).to.be-falsy;
  }

  it 'uses the tag.haml layout when present', {
    build(out());
    expect(out().add('tags/raku.html').slurp.contains("class='tag'")).to.be-truthy;
  }
}

describe 'the tag index', {
  let(:out, { temp-dir('tagindex') });

  after-each { nuke(out()) }

  it 'writes a tag index at tags.html', {
    build(out());
    expect(out().add('tags.html').e).to.be-truthy;
  }

  it 'lists each tag with its post count', {
    build(out());
    my $html = out().add('tags.html').slurp;
    expect($html.contains('raku (3)') && $html.contains('web (2)')).to.be-truthy;
  }

  it 'links each tag to its page', {
    build(out());
    expect(out().add('tags.html').slurp.contains("href='/tags/raku'")).to.be-truthy;
  }
}
