use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Scaffold;
use Blogin::Config;
use Blogin;
use Blogin::Log;

describe 'scaffolding a site', {
  let(:dir, { temp-made('init') });

  before-each { Blogin::Scaffold::init(dir(), date => '2026-07-20') }
  after-each  { nuke(dir()) }

  it 'writes blogin.json', {
    expect(dir().add('blogin.json').e).to.be-truthy;
  }

  it 'writes a starter post', {
    expect(dir().add('content/posts/2026-07-20-hello-world.md').e).to.be-truthy;
  }

  it 'writes the canonical layouts', {
    my @layouts = <base show index _header _footer _nav _search>;
    expect(@layouts.map({ dir().add("layouts/$_.haml").e }).all.so).to.be-truthy;
  }

  it 'writes a stylesheet under assets/css', {
    expect(dir().add('assets/css/style.css').e).to.be-truthy;
  }

  it 'creates the assets js and img directories', {
    expect(dir().add('assets/js').d && dir().add('assets/img').d).to.be-truthy;
  }

  it 'defaults the home section in the config', {
    my $config = Blogin::Config.load(dir().add('blogin.json'));
    expect($config.home-section).to.eq('posts');
  }
}

describe 'the scaffold builds', {
  let(:dir, { temp-made('build') });
  let(:out, { temp-made('out') });

  before-each { Blogin::Scaffold::init(dir(), date => '2026-07-20') }

  after-each {
    nuke(dir());
    nuke(out());
  }

  sub build-scaffold {
    build(
      src => dir().add('content'),
      config => Blogin::Config.load(dir().add('blogin.json')),
      out => out(),
      log => Blogin::Log.new(:level('quiet')),
    );
  }

  it 'produces a home index page', {
    build(
      src => dir().add('content'),
      config => Blogin::Config.load(dir().add('blogin.json')),
      out => out(),
      log => Blogin::Log.new(:level('quiet')),
    );
    expect(out().add('index.html').e).to.be-truthy;
  }

  it 'produces the starter post page', {
    build(
      src => dir().add('content'),
      config => Blogin::Config.load(dir().add('blogin.json')),
      out => out(),
      log => Blogin::Log.new(:level('quiet')),
    );
    expect(out().add('posts/hello-world/index.html').e).to.be-truthy;
  }

  it 'defaults the generated config to directory-index urls', {
    expect(Blogin::Config.load(dir().add('blogin.json')).clean-urls).to.be-falsy;
  }

  it 'lists every option in the generated config', {
    my $json = dir().add('blogin.json').slurp;
    my @keys = <title base-url author output-dir home-section clean-urls css-framework
                page-size highlight summary-length reading-wpm related-count taxonomies
                feed-formats robots minify fingerprint image-widths search
                search-text-length search-cap languages language-config theme plugins
                debug sections>;
    expect(@keys.grep({ !$json.contains("\"$_\"") }).elems).to.eq(0);
  }

  it 'links to blogin.dev in the footer', {
    build-scaffold;
    expect(out().add('index.html').slurp.contains("href='https://blogin.dev'")).to.be-truthy;
  }

  it 'links the atom and rss feeds in the footer', {
    build-scaffold;
    my $page = out().add('index.html').slurp;
    expect($page.contains("href='/feed.xml'") && $page.contains("href='/rss.xml'")).to.be-truthy;
  }

  it 'emits the feeds the footer links to', {
    build-scaffold;
    expect(out().add('feed.xml').e && out().add('rss.xml').e).to.be-truthy;
  }

  it 'renders a theme toggle in the header', {
    build-scaffold;
    expect(out().add('index.html').slurp.contains('class="blogin-theme-toggle"')).to.be-truthy;
  }

  it 'inlines the early theme script', {
    build-scaffold;
    expect(out().add('index.html').slurp.contains('window.bloginToggleTheme')).to.be-truthy;
  }

  it 'generates dark-mode styles in blogin.css', {
    build-scaffold;
    expect(out().add('assets/css/blogin.css').slurp.contains('[data-theme="dark"]')).to.be-truthy;
  }

  it 'puts the search in the header, not a sidebar', {
    build-scaffold;
    my $html = out().add('index.html').slurp;
    expect($html.contains('navbar-search') && !$html.contains('<aside')).to.be-truthy;
  }

  it 'stacks the navbar search and toggle through blogin.css', {
    build-scaffold;
    expect(out().add('assets/css/blogin.css').slurp.contains('.navbar-tools')).to.be-truthy;
  }

  it 'themes the body of a plain site through style.css', {
    build-scaffold;
    expect(out().add('assets/css/style.css').slurp.contains('[data-theme="dark"]')).to.be-truthy;
  }

  it 'lets the plain header wrap the toggle on small screens', {
    build-scaffold;
    expect(out().add('assets/css/style.css').slurp.contains('flex-wrap')).to.be-truthy;
  }

  it 'links a stylesheet that resolves to an emitted file', {
    build(
      src => dir().add('content'),
      config => Blogin::Config.load(dir().add('blogin.json')),
      out => out(),
      log => Blogin::Log.new(:level('quiet')),
    );

    my $page = out().add('index.html').slurp;
    $page ~~ /'href=' <["']> $<href>=[ '/' <-["']>+ '.css' ] <["']>/;
    my $linked = out().add($<href>.Str.subst(/^ '/' /, ''));

    expect($linked.e).to.be-truthy;
  }
}

describe 'a non-empty target', {
  let(:dir, { temp-made('nonempty') });

  after-each { nuke(dir()) }

  it 'refuses without force and names a conflict', {
    dir().add('keep.txt').spurt('existing');
    try Blogin::Scaffold::init(dir());
    expect($!.message.contains('keep.txt')).to.be-truthy;
  }

  it 'overwrites with force', {
    dir().add('keep.txt').spurt('existing');
    Blogin::Scaffold::init(dir(), force => True, date => '2026-07-20');
    expect(dir().add('blogin.json').e).to.be-truthy;
  }
}

describe 'framework selection', {
  let(:dir, { temp-made('fw') });
  let(:out, { temp-made('fwout') });

  after-each {
    nuke(dir());
    nuke(out());
  }

  sub build-scaffold {
    build(
      src => dir().add('content'),
      config => Blogin::Config.load(dir().add('blogin.json')),
      out => out(),
      log => Blogin::Log.new(:level('quiet')),
    );
  }

  it 'records the chosen framework in the config', {
    Blogin::Scaffold::init(dir(), framework => 'bootstrap5', date => '2026-07-20');
    expect(Blogin::Config.load(dir().add('blogin.json')).css-framework).to.eq('bootstrap5');
  }

  it 'links the framework stylesheet in the built pages', {
    Blogin::Scaffold::init(dir(), framework => 'bootstrap5', date => '2026-07-20');
    build-scaffold;
    expect(out().add('index.html').slurp.contains('bootstrap@5.3.3/dist/css')).to.be-truthy;
  }

  it 'includes the framework script bundle in the built pages', {
    Blogin::Scaffold::init(dir(), framework => 'bootstrap5', date => '2026-07-20');
    build-scaffold;
    expect(out().add('index.html').slurp.contains('bootstrap.bundle.min.js')).to.be-truthy;
  }

  it 'links no framework stylesheet under the none framework', {
    Blogin::Scaffold::init(dir(), framework => 'none', date => '2026-07-20');
    build-scaffold;
    expect(out().add('index.html').slurp.contains('cdn.jsdelivr.net')).to.be-falsy;
  }

  context 'the bootstrap5 scaffold layout', {
    before-each {
      Blogin::Scaffold::init(dir(), framework => 'bootstrap5', date => '2026-07-20');
      build-scaffold;
    }

    it 'renders a responsive navbar with the toggler', {
      expect(out().add('index.html').slurp.contains('navbar-toggler')).to.be-truthy;
    }

    it 'wraps the page in a bootstrap container', {
      expect(out().add('index.html').slurp.contains("class='container")).to.be-truthy;
    }

    it 'renders the listing as a list-group', {
      expect(out().add('index.html').slurp.contains("class='list-group'")).to.be-truthy;
    }

    it 'links the emitted search stylesheet from the widget', {
      expect(out().add('index.html').slurp.contains('/assets/css/search.css')).to.be-truthy;
    }

    it 'links the feeds from the footer', {
      expect(out().add('index.html').slurp.contains("href='/feed.xml'")).to.be-truthy;
    }

    it 'renders a theme toggle in the navbar', {
      expect(out().add('index.html').slurp.contains('class="blogin-theme-toggle"')).to.be-truthy;
    }

    it 'puts the search in the navbar', {
      my $html = out().add('index.html').slurp;
      expect($html.contains('navbar-search') && $html.contains('data-blogin-search')).to.be-truthy;
    }

    it 'groups the search and toggle so they stack on small screens', {
      my $html = out().add('index.html').slurp;
      expect($html.contains('navbar-tools') && $html.contains('navbar-toggle-slot')).to.be-truthy;
    }

    it 'adapts the navbar to the color theme instead of forcing dark', {
      expect(out().add('index.html').slurp.contains('bg-body-tertiary')).to.be-truthy;
    }
  }

  it 'rejects an unknown framework', {
    try Blogin::Scaffold::init(dir(), framework => 'nope');
    expect($!.message.contains('nope')).to.be-truthy;
  }
}

describe 'per-section date visibility', {
  let(:dir, { temp-made('dates') });
  let(:out, { temp-made('dates-out') });

  before-each { Blogin::Scaffold::init(dir(), date => '2026-07-20') }

  after-each {
    nuke(dir());
    nuke(out());
  }

  sub build-with($config) {
    build(
      src    => dir().add('content'),
      config => $config,
      out    => out(),
      log    => Blogin::Log.new(:level('quiet')),
    );
  }

  it 'shows dates by default', {
    build-with(Blogin::Config.load(dir().add('blogin.json')));
    expect(out().add('index.html').slurp.contains('2026-07-20')).to.be-truthy;
  }

  it 'hides listing dates when the section disables them', {
    build-with(Blogin::Config.from-data(%(
      home-section => 'posts',
      sections     => %( posts => %( index-dates => False ) ),
    )));
    expect(out().add('index.html').slurp.contains('2026-07-20')).to.be-falsy;
  }

  it 'hides post dates when the section disables them', {
    build-with(Blogin::Config.from-data(%(
      home-section => 'posts',
      sections     => %( posts => %( show-dates => False ) ),
    )));
    expect(out().add('posts/hello-world/index.html').slurp.contains('2026-07-20')).to.be-falsy;
  }
}
