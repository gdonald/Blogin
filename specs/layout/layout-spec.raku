use lib 'lib';
use BDD::Behave;
use Blogin::Post;
use Blogin::Layout;

my $LAYOUTS = 'specs/fixtures/layouts'.IO;

sub post-of(Str $title, Str $body) {
  Blogin::Post.parse("---\ntitle: $title\ndate: 2026-07-19\n---\n$body\n", filename => 'p.md');
}

sub render(Str $title, Str $body, *%options) {
  Blogin::Layout::render-post(post => post-of($title, $body), layouts => $LAYOUTS, site => %( title => 'My Site' ), |%options);
}

describe 'rendering a post through base and show', {
  let(:html, { render('Hello', '**bold** body') });

  it 'wraps the post in the base shell', {
    expect(html().contains('<html>')).to.be-truthy;
  }

  it 'renders the show template article', {
    expect(html().contains('<article>')).to.be-truthy;
  }

  it 'injects the rendered body without double-escaping', {
    expect(html().contains('<strong>bold</strong>')).to.be-truthy;
  }

  it 'includes the site title from config', {
    expect(html().contains('My Site')).to.be-truthy;
  }
}

describe 'escaping post fields', {
  it 'escapes the title field', {
    expect(render('A < B', 'x').contains('A &lt; B')).to.be-truthy;
  }
}

describe 'chrome partials', {
  let(:html, { render('T', 'b') });

  it 'pulls in the header partial with site data', {
    expect(html().contains("<span class='brand'>My Site")).to.be-truthy;
  }

  it 'pulls in the footer partial', {
    expect(html().contains('Built with Blogin')).to.be-truthy;
  }

  it 'omits an absent sidebar without erroring', {
    expect(html().contains('<aside>')).to.be-falsy;
  }
}

describe 'per-section layout resolution', {
  it 'uses the section override when present', {
    expect(render('E', 'b', section => 'essays').contains("<article class='essay'>")).to.be-truthy;
  }

  it 'falls back to the default show for a section without an override', {
    expect(render('O', 'b', section => 'other').contains('<article>')).to.be-truthy;
  }

  it 'uses the default show for a root-level page', {
    expect(render('R', 'b').contains('<article>')).to.be-truthy;
  }
}

describe 'missing required layouts', {
  it 'errors naming show.haml when show is missing', {
    try Blogin::Layout::render-post(
      post => post-of('T', 'b'),
      layouts => 'specs/fixtures/layouts-no-show'.IO,
      site => %( title => 'S' ),
    );
    expect($!.message.contains('show.haml')).to.be-truthy;
  }

  it 'errors naming base.haml when base is missing', {
    try Blogin::Layout::render-post(
      post => post-of('T', 'b'),
      layouts => 'specs/fixtures/layouts-no-base'.IO,
      site => %( title => 'S' ),
    );
    expect($!.message.contains('base.haml')).to.be-truthy;
  }
}
