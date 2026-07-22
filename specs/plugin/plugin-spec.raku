use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin;
use Blogin::Config;
use Blogin::Log;

my $FIXTURE = 'specs/fixtures/plugin'.IO;

describe 'Blogin::Config plugins', {
  it 'defaults plugins to empty', {
    expect(Blogin::Config.new.plugins.elems).to.eq(0);
  }

  it 'reads a plugins list', {
    expect(Blogin::Config.from-data(%( plugins => ['A', 'B'] )).plugins.join(',')).to.eq('A,B');
  }
}

describe 'a build plugin hook', {
  let(:out, {
    my $dir    = temp-dir('plugin');
    my $config = Blogin::Config.load($FIXTURE.add('blogin.json'));
    Blogin::build(src => $FIXTURE.add('content'), :$config, out => $dir, log => Blogin::Log.new(level => 'quiet'));
    $dir
  });

  after-each { nuke(out()) }

  it 'runs the plugin and lets it emit output', {
    expect(out().add('plugin-output.txt').e).to.be-truthy;
  }

  it 'hands the rendered pages to the plugin', {
    expect(out().add('plugin-output.txt').slurp.contains('pages: 2')).to.be-truthy;
  }
}
