use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin;
use Blogin::Config;
use Blogin::Log;

my $FIXTURE = 'specs/fixtures/theme'.IO;

describe 'Blogin::Config theme', {
  it 'defaults the theme to empty', {
    expect(Blogin::Config.new.theme).to.eq('');
  }

  it 'reads a theme name', {
    expect(Blogin::Config.from-data(%( theme => 'acme' )).theme).to.eq('acme');
  }
}

describe 'building against a theme', {
  let(:out, {
    my $dir    = temp-dir('theme');
    my $config = Blogin::Config.load($FIXTURE.add('blogin.json'));
    Blogin::build(src => $FIXTURE.add('content'), :$config, out => $dir, log => Blogin::Log.new(level => 'quiet'));
    $dir
  });

  after-each { nuke(out()) }

  it 'uses a theme layout the site does not provide', {
    expect(out().add('posts/hello.html').slurp.contains("class='theme-body'")).to.be-truthy;
  }

  it 'lets a local layout override the theme file', {
    expect(out().add('posts/hello.html').slurp.contains("class='local-show'")).to.be-truthy;
  }

  it 'does not use the theme file that was overridden locally', {
    expect(out().add('posts/hello.html').slurp.contains("class='theme-show'")).to.be-falsy;
  }

  it 'copies the theme static assets', {
    expect(out().add('theme.css').e).to.be-truthy;
  }
}
