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

  it 'links the root index forward to its second page', {
    build-paginated(out(), home-section => 'posts');
    expect(out().add('index.html').slurp.contains('href="/page/2"')).to.be-truthy;
  }
}
