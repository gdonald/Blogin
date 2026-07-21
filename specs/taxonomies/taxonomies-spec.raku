use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Post;
use Blogin::Config;
use Blogin::Site;

my $FIXTURE = 'specs/fixtures/taxonomies'.IO;

describe 'Blogin::Post::terms', {
  let(:post, {
    Blogin::Post.parse("---\ntitle: T\ntags: [raku]\ncategories: [news, tutorials]\n---\nx\n", filename => 'p.md')
  });

  it 'returns the tags for the tags taxonomy', {
    expect(post().terms('tags').join(',')).to.eq('raku');
  }

  it 'parses a bracketed list for another taxonomy', {
    expect(post().terms('categories').join(',')).to.eq('news,tutorials');
  }

  it 'returns nothing for an absent taxonomy', {
    expect(post().terms('series').elems).to.eq(0);
  }
}

describe 'building a categories taxonomy', {
  let(:out, {
    my $dir = temp-dir('taxonomies');
    build-fixture($FIXTURE, $dir, taxonomies => ['tags', 'categories']);
    $dir
  });

  after-each { nuke(out()) }

  it 'writes a page per category term', {
    expect(out().add('categories/news.html').e).to.be-truthy;
  }

  it 'aggregates every post that shares a term', {
    my $html = out().add('categories/news.html').slurp;
    expect($html.contains("href='/posts/alpha'") && $html.contains("href='/posts/bravo'")).to.be-truthy;
  }

  it 'excludes a post that lacks the term', {
    expect(out().add('categories/tutorials.html').slurp.contains("href='/posts/bravo'")).to.be-falsy;
  }

  it 'writes a category index', {
    expect(out().add('categories.html').e).to.be-truthy;
  }

  it 'lists each term with its post count on the index', {
    expect(out().add('categories.html').slurp.contains('news (2)')).to.be-truthy;
  }

  it 'links a term to its page from the index', {
    expect(out().add('categories.html').slurp.contains("href='/categories/news'")).to.be-truthy;
  }

  it 'resolves a taxonomy-named layout for the term page', {
    expect(out().add('categories/news.html').slurp.contains("class='category'")).to.be-truthy;
  }

  it 'still builds the tags taxonomy alongside', {
    expect(out().add('tags/raku.html').e).to.be-truthy;
  }
}

describe 'a taxonomy with no name-specific layout', {
  let(:out, {
    my $dir = temp-dir('taxonomies-fallback');
    build-fixture($FIXTURE, $dir, taxonomies => ['tags', 'categories', 'series']);
    $dir
  });

  after-each { nuke(out()) }

  it 'slugifies a multi-word term for the page path', {
    expect(out().add('series/part-one.html').e).to.be-truthy;
  }

  it 'falls back to the index layout for the term page', {
    expect(out().add('series/part-one.html').slurp.contains("class='listing'")).to.be-truthy;
  }

  it 'falls back to the index layout for the term index', {
    expect(out().add('series.html').slurp.contains("class='listing'")).to.be-truthy;
  }

  it 'does not use a name-specific layout it has none of', {
    expect(out().add('series/part-one.html').slurp.contains("class='category'")).to.be-falsy;
  }
}

describe 'taxonomies with directory-index urls', {
  let(:out, {
    my $dir = temp-dir('taxonomies-dirindex');
    build-fixture($FIXTURE, $dir, taxonomies => ['tags', 'categories'], clean-urls => False);
    $dir
  });

  after-each { nuke(out()) }

  it 'writes a term page as a directory index', {
    expect(out().add('categories/news/index.html').e).to.be-truthy;
  }

  it 'writes the taxonomy index as a directory index', {
    expect(out().add('categories/index.html').e).to.be-truthy;
  }

  it 'links terms with trailing-slash urls', {
    expect(out().add('categories/index.html').slurp.contains("href='/categories/news/'")).to.be-truthy;
  }
}

describe 'a taxonomy no post uses', {
  let(:out, {
    my $dir = temp-dir('taxonomies-empty');
    build-fixture($FIXTURE, $dir, taxonomies => ['tags', 'authors']);
    $dir
  });

  after-each { nuke(out()) }

  it 'builds the site without error', {
    expect(out().add('posts/alpha.html').e).to.be-truthy;
  }

  it 'writes no pages for the empty taxonomy', {
    expect(out().add('authors.html').e || out().add('authors').e).to.be-falsy;
  }
}

describe 'taxonomies flowing from config build options', {
  let(:out, {
    my $dir   = temp-dir('taxonomies-config');
    my $config = Blogin::Config.from-data(%( taxonomies => ['tags', 'categories'] ));

    Blogin::Site::build(
      content => $FIXTURE.add('content'),
      out     => $dir,
      layouts => $FIXTURE.add('layouts'),
      |$config.build-options,
    );

    $dir
  });

  after-each { nuke(out()) }

  it 'builds the categories taxonomy through the config options', {
    expect(out().add('categories/news.html').e).to.be-truthy;
  }
}
