use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Data;

my $FIXTURE = 'specs/fixtures/data'.IO;
my $CONTENT = $FIXTURE.add('content');
my $DATADIR = $FIXTURE.add('data');

describe 'Blogin::Data::load', {
  let(:tree, { Blogin::Data::load($DATADIR) });

  it 'reads a json file into a keyed entry', {
    expect(tree()<authors><name>).to.eq('Greg Donald');
  }

  it 'reads a yaml file into a keyed entry', {
    expect(tree()<meta><tagline>).to.eq('data files work');
  }

  it 'keys an entry by its filename without the extension', {
    expect(tree()<banner>).to.eq('Global Banner');
  }

  it 'returns an empty hash for a missing directory', {
    expect(Blogin::Data::load($FIXTURE.add('nope')).elems).to.eq(0);
  }
}

describe 'Blogin::Data::deep-merge', {
  it 'replaces a scalar key from the overriding hash', {
    expect(Blogin::Data::deep-merge(%( a => 1, b => 2 ), %( b => 3 ))<b>).to.eq(3);
  }

  it 'keeps a base key the override does not touch', {
    expect(Blogin::Data::deep-merge(%( a => 1, b => 2 ), %( b => 3 ))<a>).to.eq(1);
  }

  it 'merges nested hashes rather than replacing them', {
    my %merged = Blogin::Data::deep-merge(%( x => %( p => 1, q => 2 ) ), %( x => %( q => 9 ) ));
    expect(%merged<x><p>).to.eq(1);
  }

  it 'lets the override win on a nested key', {
    my %merged = Blogin::Data::deep-merge(%( x => %( p => 1, q => 2 ) ), %( x => %( q => 9 ) ));
    expect(%merged<x><q>).to.eq(9);
  }
}

describe 'Blogin::Data::resolve', {
  let(:global, { Blogin::Data::load($DATADIR) });

  it 'keeps a global value for a section with no directory data', {
    expect(Blogin::Data::resolve(global(), $CONTENT, '')<banner>).to.eq('Global Banner');
  }

  it 'lets a directory-scoped file override a global key beneath it', {
    expect(Blogin::Data::resolve(global(), $CONTENT, 'posts')<banner>).to.eq('Posts Banner');
  }

  it 'still exposes untouched global keys under a scoped section', {
    expect(Blogin::Data::resolve(global(), $CONTENT, 'posts')<authors><name>).to.eq('Greg Donald');
  }
}

describe 'data files through a build', {
  let(:out, { temp-dir('data') });

  after-each { nuke(out()) }

  it 'surfaces a global data file in a rendered post', {
    build-fixture($FIXTURE, out());
    expect(out().add('posts/hello.html').slurp.contains('Greg Donald')).to.be-truthy;
  }

  it 'surfaces a yaml data file in a rendered post', {
    build-fixture($FIXTURE, out());
    expect(out().add('posts/hello.html').slurp.contains('data files work')).to.be-truthy;
  }

  it 'applies a directory-scoped override to a post beneath it', {
    build-fixture($FIXTURE, out());
    expect(out().add('posts/hello.html').slurp.contains('Posts Banner')).to.be-truthy;
  }

  it 'leaves a root page on the global value', {
    build-fixture($FIXTURE, out());
    expect(out().add('about.html').slurp.contains('Global Banner')).to.be-truthy;
  }

  it 'applies the directory-scoped override to the section listing', {
    build-fixture($FIXTURE, out());
    expect(out().add('posts.html').slurp.contains('Posts Banner')).to.be-truthy;
  }
}
