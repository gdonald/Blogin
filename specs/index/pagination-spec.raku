use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Site;

my $PAGINATED = 'specs/fixtures/paginated'.IO;


sub build-paginated(IO::Path:D $out, *%options) {
  Blogin::Site::build(
    content => $PAGINATED.add('content'),
    out     => $out,
    layouts => $PAGINATED.add('layouts'),
    page-size => 2,
    |%options,
  );
}

describe 'per-section paginated listings', {
  let(:out, { temp-dir('page') });

  after-each { nuke(out()) }

  it 'writes the first listing page at the section path', {
    build-paginated(out());
    expect(out().add('posts.html').e).to.be-truthy;
  }

  it 'writes the second listing page under page/2', {
    build-paginated(out());
    expect(out().add('posts/page/2.html').e).to.be-truthy;
  }

  it 'lists the newest posts first on page one', {
    build-paginated(out());
    my $html = out().add('posts.html').slurp;
    expect($html.contains("href='/posts/alpha'") && $html.contains("href='/posts/bravo'")).to.be-truthy;
  }

  it 'keeps the overflow post off page one', {
    build-paginated(out());
    expect(out().add('posts.html').slurp.contains("href='/posts/charlie'")).to.be-falsy;
  }

  it 'includes each post description in the listing', {
    build-paginated(out());
    expect(out().add('posts.html').slurp.contains('The alpha summary.')).to.be-truthy;
  }

  it 'puts the overflow post on page two', {
    build-paginated(out());
    expect(out().add('posts/page/2.html').slurp.contains("href='/posts/charlie'")).to.be-truthy;
  }

  it 'links forward from page one to page two', {
    build-paginated(out());
    expect(out().add('posts.html').slurp.contains('href="/posts/page/2"')).to.be-truthy;
  }

  it 'links back from page two to page one', {
    build-paginated(out());
    expect(out().add('posts/page/2.html').slurp.contains('href="/posts"')).to.be-truthy;
  }

  it 'writes a single page for a short section', {
    build-paginated(out());
    expect(out().add('essays.html').e && !out().add('essays/page/2.html').e).to.be-truthy;
  }
}

describe 'adjacent post navigation within a section', {
  let(:out, { temp-dir('post-nav') });

  before-each { build-paginated(out()) }
  after-each  { nuke(out()) }

  it 'links a middle post to the newer neighbor', {
    expect(out().add('posts/bravo.html').slurp.contains("href=\"/posts/alpha\"")).to.be-truthy;
  }

  it 'links a middle post to the older neighbor', {
    expect(out().add('posts/bravo.html').slurp.contains("href=\"/posts/charlie\"")).to.be-truthy;
  }

  it 'omits the newer link on the newest post', {
    expect(out().add('posts/alpha.html').slurp.contains('class="prev"')).to.be-falsy;
  }

  it 'omits the older link on the oldest post', {
    expect(out().add('posts/charlie.html').slurp.contains('class="next"')).to.be-falsy;
  }

  it 'shows no navigation for a section with a single post', {
    expect(out().add('essays/first-essay.html').slurp.contains('post-nav')).to.be-falsy;
  }
}

describe 'the home section at the site root', {
  let(:out, { temp-dir('home') });

  after-each { nuke(out()) }

  it 'writes the home section listing to the root index', {
    build-paginated(out(), home-section => 'posts');
    expect(out().add('index.html').e).to.be-truthy;
  }

  it 'paginates the home section at the root', {
    build-paginated(out(), home-section => 'posts');
    expect(out().add('page/2.html').e).to.be-truthy;
  }

  it 'lists the newest posts on the root index', {
    build-paginated(out(), home-section => 'posts');
    expect(out().add('index.html').slurp.contains("href='/posts/alpha'")).to.be-truthy;
  }

  it 'renders the root through the home template when one exists', {
    build-paginated(out(), home-section => 'posts');
    expect(out().add('index.html').slurp.contains('Welcome')).to.be-truthy;
  }

  it 'leaves the section listing on the plain index template', {
    build-paginated(out(), home-section => 'posts');
    expect(out().add('posts.html').slurp.contains('Welcome')).to.be-falsy;
  }

  it 'links the root index forward to its second page', {
    build-paginated(out(), home-section => 'posts');
    expect(out().add('index.html').slurp.contains('href="/page/2"')).to.be-truthy;
  }
}
