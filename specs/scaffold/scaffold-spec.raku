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

  after-each { nuke(dir()) }

  it 'records the chosen framework in the config', {
    Blogin::Scaffold::init(dir(), framework => 'bootstrap5', date => '2026-07-20');
    expect(Blogin::Config.load(dir().add('blogin.json')).css-framework).to.eq('bootstrap5');
  }

  it 'wires the framework stylesheet into base.haml', {
    Blogin::Scaffold::init(dir(), framework => 'bootstrap5', date => '2026-07-20');
    expect(dir().add('layouts/base.haml').slurp.contains('bootstrap')).to.be-truthy;
  }

  it 'rejects an unknown framework', {
    try Blogin::Scaffold::init(dir(), framework => 'nope');
    expect($!.message.contains('nope')).to.be-truthy;
  }
}
