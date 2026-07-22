use lib 'lib';
use BDD::Behave;
use Blogin::Post;
use Blogin::Layout;
use Blogin::Nav;
use Blogin::Framework;

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

describe 'adjacent post links on a post view', {
  it 'renders both neighbor links when present', {
    my $view = Blogin::Layout::View.new(
      prev-url => '/posts/newer', prev-title => 'Newer',
      next-url => '/posts/older', next-title => 'Older',
    );
    expect($view.post-nav-html.contains('/posts/newer') && $view.post-nav-html.contains('/posts/older')).to.be-truthy;
  }

  it 'renders nothing when a post has no neighbors', {
    expect(Blogin::Layout::View.new.post-nav-html).to.eq('');
  }

  it 'escapes a neighbor title', {
    my $view = Blogin::Layout::View.new(next-url => '/x', next-title => 'A & B');
    expect($view.post-nav-html.contains('A &amp; B')).to.be-truthy;
  }

  it 'styles the links as buttons by default', {
    my $view = Blogin::Layout::View.new(next-url => '/x', next-title => 'Next');
    expect($view.post-nav-html.contains('blogin-btn')).to.be-truthy;
  }

  it 'uses the framework button class under bootstrap5', {
    my $view = Blogin::Layout::View.new(
      framework => Blogin::Framework::profile('bootstrap5'),
      next-url => '/x', next-title => 'Next',
    );
    expect($view.post-nav-html.contains('btn btn-primary')).to.be-truthy;
  }

  it 'gives the links directional arrows', {
    my $view = Blogin::Layout::View.new(
      prev-url => '/p', prev-title => 'P', next-url => '/n', next-title => 'N',
    );
    expect($view.post-nav-html.contains('&larr;') && $view.post-nav-html.contains('&rarr;')).to.be-truthy;
  }
}

describe 'the tags of a post view', {
  let(:tagged, {
    Blogin::Layout::View.new(tags => [ %( name => 'raku', url => '/tags/raku/' ), %( name => 'web', url => '/tags/web/' ) ])
  });

  it 'reports that it has tags when some are present', {
    expect(tagged().has-tags).to.be-truthy;
  }

  it 'exposes each tag link for the layout to render', {
    expect(tagged().tags.map(*<name>).join(',')).to.eq('raku,web');
  }

  it 'reports no tags when none are present', {
    expect(Blogin::Layout::View.new.has-tags).to.be-falsy;
  }
}

describe 'the section heading', {
  it 'title-cases a single-segment section', {
    expect(Blogin::Layout::ListView.new(section => 'guide').section-label).to.eq('Guide');
  }

  it 'humanizes the last segment of a nested section', {
    expect(Blogin::Layout::ListView.new(section => 'guide/getting-started').section-label).to.eq('Getting Started');
  }

  it 'prefers the configured nav label over humanizing', {
    my $node = Blogin::Nav::NavNode.new(name => 'cli', label => 'CLI', path => 'cli', url => '/cli');
    my $view = Blogin::Layout::ListView.new(section => 'cli', nav => [$node]);
    expect($view.section-label).to.eq('CLI');
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

describe 'a named layout override', {
  it 'renders a post through the named layout when present', {
    expect(render('J', 'b', templates => ['journal', 'show']).contains("<article class='journal'>")).to.be-truthy;
  }

  it 'falls back to the next template when the named layout is absent', {
    expect(render('J', 'b', templates => ['missing', 'show']).contains('<article>')).to.be-truthy;
  }

  it 'errors naming every candidate when none resolve', {
    try Blogin::Layout::render-post(
      post => post-of('T', 'b'),
      layouts => $LAYOUTS,
      site => %( title => 'S' ),
      templates => ['nope'],
    );
    expect($!.message.contains('nope.haml')).to.be-truthy;
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
