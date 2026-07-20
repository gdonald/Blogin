use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin;

describe 'Blogin::build', {
  let(:root, { temp-dir('build') });
  let(:src,  { root().add('content') });
  let(:out,  { root().add('public') });

  before-each {
    src().mkdir;
  }

  after-each {
    nuke(root()) if root().e;
  }

  it 'creates the output directory', {
    Blogin::build(:src(src()), :out(out()));

    expect(out().d).to.be-truthy;
  }

  it 'leaves the output empty for an empty source', {
    Blogin::build(:src(src()), :out(out()));

    expect(out().dir.elems).to.eq(0);
  }

  it 'returns true', {
    expect(Blogin::build(:src(src()), :out(out()))).to.be-truthy;
  }
}

describe 'blogin build (cli)', {
  my $root;
  my $exit-code;
  my $output-exists;

  before-all {
    $root = temp-dir('cli');

    my $src = $root.add('content');
    my $out = $root.add('public');

    $src.mkdir;

    my $proc = run(
      $*EXECUTABLE, '-Ilib', 'bin/blogin', 'build',
      "--src=$src", "--out=$out", '--quiet',
      :out, :err,
    );

    $proc.out.slurp(:close);
    $proc.err.slurp(:close);

    $exit-code     = $proc.exitcode;
    $output-exists = $out.d;
  }

  after-all {
    nuke($root) if $root.defined && $root.e;
  }

  it 'exits 0 on an empty source directory', {
    expect($exit-code).to.eq(0);
  }

  it 'creates the output directory via the cli', {
    expect($output-exists).to.be-truthy;
  }
}
