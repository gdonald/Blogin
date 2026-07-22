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

  it 'heads a tag page with the term name', {
    build(out());
    expect(out().add('tags/raku.html').slurp.contains('<h1>raku</h1>')).to.be-truthy;
  }

  it 'renders a term page through the singular layout, not the index layout', {
    build(out());
    my $html = out().add('tags/raku.html').slurp;
    expect($html.contains("class='tag'") && !$html.contains('tag-cloud')).to.be-truthy;
  }
}

describe 'tags on a post page', {
  let(:out, { temp-dir('posttags') });

  after-each { nuke(out()) }

  it 'renders each of the post tags as a link', {
    build(out());
    my $html = out().add('posts/alpha.html').slurp;
    expect($html.contains("href='/tags/raku'") && $html.contains("href='/tags/web'")).to.be-truthy;
  }

  it 'labels each tag link with the tag name', {
    build(out());
    expect(out().add('posts/alpha.html').slurp.contains('>raku<')).to.be-truthy;
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

  it 'heads the index with the humanized taxonomy name', {
    build(out());
    expect(out().add('tags.html').slurp.contains('<h1>Tags</h1>')).to.be-truthy;
  }

  it 'renders the index through the plural layout', {
    build(out());
    expect(out().add('tags.html').slurp.contains('tag-cloud')).to.be-truthy;
  }
}
