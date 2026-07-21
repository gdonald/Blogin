use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin;
use Blogin::Site;

sub seed(IO::Path:D $dir) {
  $dir.add('posts').mkdir;

  $dir.add('index.html').spurt('home');
  $dir.add('posts/hello.html').spurt('hello');
}

describe 'Blogin::Site::clean', {
  let(:root, { temp-made('clean') });
  let(:out,  { root().add('public') });

  before-each {
    out().mkdir;
    seed(out());
  }

  after-each {
    nuke(root()) if root().e;
  }

  it 'removes the output directory', {
    Blogin::Site::clean(:out(out()), :root(root()));

    expect(out().e).to.be-falsy;
  }

  it 'returns the count of files removed', {
    expect(Blogin::Site::clean(:out(out()), :root(root()))).to.eq(2);
  }

  it 'returns zero for an output directory that does not exist', {
    nuke(out());

    expect(Blogin::Site::clean(:out(out()), :root(root()))).to.eq(0);
  }
}

describe 'Blogin::Site::clean guarding the target', {
  let(:root,    { temp-made('clean-root') });
  let(:outside, { temp-made('clean-outside') });

  before-each {
    outside().add('keep.txt').spurt('keep');
  }

  after-each {
    nuke(root()) if root().e;
    nuke(outside()) if outside().e;
  }

  it 'refuses a target that is not inside the root', {
    try Blogin::Site::clean(:out(outside()), :root(root()));

    expect($!.message.contains('refusing')).to.be-truthy;
  }

  it 'leaves an outside target untouched when it refuses', {
    try Blogin::Site::clean(:out(outside()), :root(root()));

    expect(outside().add('keep.txt').e).to.be-truthy;
  }

  it 'refuses the root itself', {
    try Blogin::Site::clean(:out(root()), :root(root()));

    expect($!.message.contains('refusing')).to.be-truthy;
  }
}

describe 'Blogin::clean facade', {
  let(:root, { temp-made('clean-facade') });
  let(:out,  { root().add('public') });

  before-each {
    out().mkdir;
    seed(out());
  }

  after-each {
    nuke(root()) if root().e;
  }

  it 'removes the output directory through the facade', {
    Blogin::clean(:out(out()), :root(root()));

    expect(out().e).to.be-falsy;
  }

  it 'reports the number of files removed', {
    expect(Blogin::clean(:out(out()), :root(root()))).to.eq(2);
  }
}
