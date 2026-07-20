use lib 'lib';
use BDD::Behave;
use Blogin::Nav;
use Blogin::Post;
use Blogin::Layout;

my $CONTENT = 'specs/fixtures/nav/content'.IO;
my $LAYOUTS = 'specs/fixtures/layouts'.IO;

sub tree(*%options) {
  Blogin::Nav::build-tree($CONTENT, |%options);
}

describe 'the section nav tree', {
  let(:nodes, { tree() });

  it 'has one entry per top-level section directory', {
    expect(nodes().map(*.name).sort.join(',')).to.eq('hidden,posts,projects');
  }

  it 'excludes root-level standalone pages', {
    expect(nodes().grep(*.name eq 'about').elems).to.eq(0);
  }

  it 'humanizes the label by default', {
    expect(nodes().first(*.name eq 'projects').label).to.eq('Projects');
  }

  it 'links a section to its listing url', {
    expect(nodes().first(*.name eq 'projects').url).to.eq('/projects');
  }

  it 'nests child section directories', {
    expect(nodes().first(*.name eq 'projects').children.map(*.name).sort.join(',')).to.eq('cli,web');
  }

  it 'gives a nested child its full section url', {
    my $web = nodes().first(*.name eq 'projects').children.first(*.name eq 'web');
    expect($web.url).to.eq('/projects/web');
  }
}

describe 'per-section nav config', {
  let(:sections, {
    %(
      projects => %( label => 'Work', order => 1 ),
      posts    => %( order => 2 ),
      hidden   => %( nav => False ),
    )
  });

  it 'applies a label override', {
    expect(tree(sections => sections()).first(*.name eq 'projects').label).to.eq('Work');
  }

  it 'orders sections by the configured order', {
    expect(tree(sections => sections()).map(*.name).join(',')).to.eq('projects,posts');
  }

  it 'drops a section marked nav false', {
    expect(tree(sections => sections()).grep(*.name eq 'hidden').elems).to.eq(0);
  }
}

describe 'rendering the nav', {
  sub render(Str $section) {
    my $post = Blogin::Post.parse("---\ntitle: T\ndate: 2026-07-19\n---\nb\n", filename => 'p.md');
    Blogin::Layout::render-post(
      post => $post, layouts => $LAYOUTS, site => %( title => 'S' ),
      section => $section, nav => tree(),
    );
  }

  it 'renders links to sections', {
    expect(render('posts').contains("href='/projects'")).to.be-truthy;
  }

  it 'renders nested child links', {
    expect(render('posts').contains("href='/projects/web'")).to.be-truthy;
  }

  it 'marks the current section', {
    expect(render('projects').contains("class='current'")).to.be-truthy;
  }

  it 'does not mark an unrelated section as current', {
    expect(render('posts').contains("<a class='current' href='/projects'")).to.be-falsy;
  }
}
