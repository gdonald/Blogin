use lib 'lib';
use BDD::Behave;
use Blogin;
use Blogin::Config;
use Blogin::Log;

my $DOCS = 'docs-src'.IO;
my $seq  = 0;

sub nuke(IO::Path:D $dir) {
  return unless $dir.e;
  for $dir.dir -> $entry {
    $entry.d ?? nuke($entry) !! $entry.unlink;
  }
  $dir.rmdir;
}

sub build-docs(IO::Path:D $out) {
  build(
    src    => $DOCS.add('content'),
    config => Blogin::Config.load($DOCS.add('blogin.json')),
    out    => $out,
    log    => Blogin::Log.new(:level('quiet')),
  );
}

describe 'the documentation site builds with Blogin', {
  let(:out, { $*TMPDIR.add("blogin-docs-{$*PID}-{$seq++}") });

  before-each { build-docs(out()) }
  after-each  { nuke(out()) }

  it 'produces a home page from the guide section', {
    expect(out().add('index.html').e).to.be-truthy;
  }

  it 'produces a page per guide document', {
    expect(out().add('guide/getting-started.html').e).to.be-truthy;
  }

  it 'renders the document body', {
    expect(out().add('guide/overview.html').slurp.contains('static blog generator')).to.be-truthy;
  }

  it 'highlights fenced code in the docs', {
    expect(out().add('guide/overview.html').slurp.contains('hl-keyword')).to.be-truthy;
  }

  it 'lists the guide in the nav', {
    expect(out().add('guide/overview.html').slurp.contains('href=\'/guide\'')).to.be-truthy;
  }

  it 'emits a search index for the docs', {
    expect(out().add('search-index.json').e).to.be-truthy;
  }

  it 'emits a feed for the docs', {
    expect(out().add('feed.xml').e).to.be-truthy;
  }
}
