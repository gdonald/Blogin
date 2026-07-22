use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;

my $FIXTURE = 'specs/fixtures/customization'.IO;

my $OUT = temp-made('customization');

build-fixture(
  $FIXTURE,
  $OUT,
  clean-urls => False,
  page-size  => 1,
  site       => %( title => 'Custom Site' ),
  sections   => %(
    blog  => %( label => 'Blog',  index-dates => True ),
    notes => %( label => 'Notes', index-dates => False ),
  ),
);

END { nuke($OUT) }

sub page(Str $rel --> Str) {
  $OUT.add($rel).slurp;
}

describe 'the post context in show.haml', {
  it 'renders the post title as the heading', {
    expect(page('blog/alpha/index.html').contains('<h1>Alpha Post</h1>')).to.be-truthy;
  }

  it 'shows the reading time in the meta line', {
    expect(page('blog/alpha/index.html').contains('min read')).to.be-truthy;
  }

  it 'renders the table of contents when the post asks for one', {
    expect(page('blog/alpha/index.html').contains("class='toc'") && page('blog/alpha/index.html').contains('#first-heading')).to.be-truthy;
  }

  it 'renders the post body', {
    expect(page('blog/alpha/index.html').contains('measurable reading time')).to.be-truthy;
  }

  it 'links each of the post tags to its term page', {
    expect(page('blog/bravo/index.html').contains("href='/tags/raku/'") && page('blog/bravo/index.html').contains("href='/tags/web/'")).to.be-truthy;
  }

  it 'links to the adjacent post', {
    my $html = page('blog/alpha/index.html');
    expect($html.contains('class="post-nav"') && $html.contains('/blog/bravo/')).to.be-truthy;
  }

  it 'lists related posts sharing a tag', {
    expect(page('blog/alpha/index.html').contains("class='related'")).to.be-truthy;
  }
}

describe 'the listing context in index.haml', {
  it 'heads the listing with the section label', {
    expect(page('blog/index.html').contains('<h1>Blog</h1>')).to.be-truthy;
  }

  it 'links each post title to its url', {
    expect(page('blog/index.html').contains("href='/blog/bravo/'>Bravo Post")).to.be-truthy;
  }

  it 'shows each post description', {
    expect(page('blog/index.html').contains('The bravo description.')).to.be-truthy;
  }

  it 'shows the date when the section lists dates', {
    expect(page('blog/index.html').contains("class='date'>2026-01-02")).to.be-truthy;
  }

  it 'omits the date when the section does not list dates', {
    expect(page('notes/index.html').contains("class='date'")).to.be-falsy;
  }

  it 'pages the listing with an older link', {
    expect(page('blog/index.html').contains('>older</a>')).to.be-truthy;
  }
}

describe 'the taxonomy index in tags.haml', {
  it 'heads the index with the humanized taxonomy name', {
    expect(page('tags/index.html').contains('<h1>Tags</h1>')).to.be-truthy;
  }

  it 'renders each term as a cloud link with its count', {
    expect(page('tags/index.html').contains("class='tag' href='/tags/raku/'>raku (2)")).to.be-truthy;
  }
}

describe 'a term page in tag.haml', {
  it 'heads the term page with the term name', {
    expect(page('tags/raku/index.html').contains('<h1>raku</h1>')).to.be-truthy;
  }

  it 'lists every post carrying the term', {
    my $html = page('tags/raku/index.html');
    expect($html.contains('Alpha Post') && $html.contains('Bravo Post')).to.be-truthy;
  }
}

describe 'partials and locals', {
  it 'passes an explicit local to the header partial', {
    expect(page('blog/index.html').contains("class='brand' href='/'>Custom Site")).to.be-truthy;
  }

  it 'binds each collection item under the :as name', {
    my $html = page('blog/index.html');
    expect($html.contains("class='site-nav'") && $html.contains('>Blog</a>') && $html.contains('>Notes</a>')).to.be-truthy;
  }
}

describe 'the site context shared by every template', {
  it 'exposes the site title', {
    expect(page('blog/index.html').contains('<title>Custom Site</title>')).to.be-truthy;
  }

  it 'marks the current section in the nav', {
    expect(page('blog/index.html').contains("class='current' href='/blog/'>Blog")).to.be-truthy;
  }

  it 'exposes a value from a data file', {
    expect(page('blog/index.html').contains('Hand-built with Blogin')).to.be-truthy;
  }
}
