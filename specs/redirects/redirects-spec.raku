use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Post;

describe 'Blogin::Post aliases', {
  it 'parses a bracketed list of aliases', {
    my $post = Blogin::Post.parse("---\ntitle: T\naliases: [/old, /older]\n---\nx\n", filename => 'p.md');
    expect($post.aliases.join(',')).to.eq('/old,/older');
  }

  it 'defaults aliases to empty', {
    my $post = Blogin::Post.parse("---\ntitle: T\n---\nx\n", filename => 'p.md');
    expect($post.aliases.elems).to.eq(0);
  }
}

describe 'alias redirects through a build', {
  my $FIXTURE = 'specs/fixtures/redirects'.IO;

  let(:out, {
    my $dir = temp-dir('redirects');
    build-fixture($FIXTURE, $dir);
    $dir
  });

  after-each { nuke(out()) }

  it 'writes a redirect stub at the alias url', {
    expect(out().add('old-hello.html').e).to.be-truthy;
  }

  it 'writes a redirect stub at a nested alias url', {
    expect(out().add('2020/01/hello.html').e).to.be-truthy;
  }

  it 'refreshes to the canonical url', {
    expect(out().add('old-hello.html').slurp.contains('content="0; url=/posts/hello"')).to.be-truthy;
  }

  it 'sets a canonical link on the stub', {
    expect(out().add('old-hello.html').slurp.contains('<link rel="canonical" href="/posts/hello">')).to.be-truthy;
  }

  it 'does not overwrite a real page with a colliding alias', {
    expect(out().add('posts/hello.html').slurp.contains('Redirecting')).to.be-falsy;
  }
}

describe 'a generated 404 page', {
  let(:redirects-out, {
    my $dir = temp-dir('nf-layout');
    build-fixture('specs/fixtures/redirects'.IO, $dir);
    my $html = $dir.add('404.html').slurp;
    nuke($dir);
    $html
  });

  let(:basic-out, {
    my $dir = temp-dir('nf-default');
    build-fixture('specs/fixtures/basic'.IO, $dir);
    my $html = $dir.add('404.html').slurp;
    nuke($dir);
    $html
  });

  it 'renders through the 404 layout when present', {
    expect(redirects-out().contains("<section class='notfound'>")).to.be-truthy;
  }

  it 'wraps the 404 layout in the base chrome', {
    expect(redirects-out().contains('<html>')).to.be-truthy;
  }

  it 'falls back to a built-in 404 when no layout is present', {
    expect(basic-out().contains('was not found')).to.be-truthy;
  }
}
