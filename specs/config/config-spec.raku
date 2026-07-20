use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Config;
use Blogin;
use Blogin::Log;

my $CONFIGURED = 'specs/fixtures/configured'.IO;


sub build-configured(IO::Path:D $target, *%options) {
  my $config = Blogin::Config.load($CONFIGURED.add('blogin.json'));

  build(
    src => $CONFIGURED.add('content'),
    config => $config,
    out => $target,
    log => Blogin::Log.new(:level('quiet')),
    |%options,
  );
}

describe 'reading blogin.json', {
  let(:config, { Blogin::Config.load($CONFIGURED.add('blogin.json')) });

  it 'reads the site title', {
    expect(config().title).to.eq('Configured Site');
  }

  it 'reads the page size', {
    expect(config().page-size).to.eq(2);
  }

  it 'reads the home section', {
    expect(config().home-section).to.eq('posts');
  }

  it 'reads the debug flag', {
    expect(config().debug).to.be-truthy;
  }

  it 'reads a per-section override', {
    expect(config().sections<essays><page-size>).to.eq(1);
  }
}

describe 'config defaults', {
  let(:config, { Blogin::Config.new });

  it 'defaults the output dir', {
    expect(config().output-dir).to.eq('public');
  }

  it 'defaults clean-urls to true', {
    expect(config().clean-urls).to.be-truthy;
  }

  it 'defaults the css framework to none', {
    expect(config().css-framework).to.eq('none');
  }
}

describe 'malformed config', {
  it 'reports the offending key on a bad type', {
    try Blogin::Config.from-data(%( page-size => 'ten' ));
    expect($!.message.contains('page-size')).to.be-truthy;
  }

  it 'reports the offending nested section key', {
    try Blogin::Config.from-data(%( sections => %( essays => %( order => 'high' ) ) ));
    expect($!.message.contains('sections.essays.order')).to.be-truthy;
  }
}

describe 'config driving the build', {
  let(:out, { temp-dir('config') });

  after-each { nuke(out()) }

  it 'passes the site title into the layouts', {
    build-configured(out());
    expect(out().add('index.html').slurp.contains('Configured Site')).to.be-truthy;
  }

  it 'renders the home section at the root', {
    build-configured(out());
    expect(out().add('index.html').e).to.be-truthy;
  }

  it 'paginates at the configured page size', {
    build-configured(out());
    expect(out().add('posts/page/2.html').e).to.be-truthy;
  }

  it 'applies a per-section page-size override', {
    build-configured(out());
    expect(out().add('essays/page/2.html').e).to.be-truthy;
  }

  it 'enables debug from config', {
    build-configured(out());
    expect(out().add('index.html').slurp.contains('<!--')).to.be-truthy;
  }

  it 'lets a flag override win over config debug', {
    build-configured(out(), debug => False);
    expect(out().add('index.html').slurp.contains('<!--')).to.be-falsy;
  }
}
