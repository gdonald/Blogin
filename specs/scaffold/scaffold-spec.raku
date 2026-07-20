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
    my @layouts = <base show index _header _sidebar _footer _nav _search>;
    expect(@layouts.map({ dir().add("layouts/$_.haml").e }).all.so).to.be-truthy;
  }

  it 'writes a stylesheet for the none framework', {
    expect(dir().add('static/style.css').e).to.be-truthy;
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
    expect(out().add('posts/hello-world.html').e).to.be-truthy;
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
      expect(out().add('index.html').slurp.contains('/search.css')).to.be-truthy;
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
    expect(out().add('posts/hello-world.html').slurp.contains('2026-07-20')).to.be-falsy;
  }
}
