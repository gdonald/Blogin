use lib 'lib';
use BDD::Behave;
use Blogin::Framework;
use Blogin::Markdown;
use Blogin::Markdown::Html;
use Blogin::Site;
use Blogin::Scaffold;

my $PAGINATED = 'specs/fixtures/paginated'.IO;
my $seq       = 0;

sub nuke(IO::Path:D $dir) {
  return unless $dir.e;
  for $dir.dir -> $entry {
    $entry.d ?? nuke($entry) !! $entry.unlink;
  }
  $dir.rmdir;
}

sub element-html(Str $markdown, Str $framework) {
  my $doc = Blogin::Markdown::parse($markdown);
  Blogin::Markdown::Html.new(framework => Blogin::Framework::profile($framework)).render($doc).html;
}

describe 'framework profiles', {
  it 'knows the four profiles', {
    expect(Blogin::Framework::known.join(',')).to.eq('bootstrap5,bulma,none,pico');
  }

  it 'maps a slot for bootstrap5', {
    expect(Blogin::Framework::profile('bootstrap5').class-for('table')).to.eq('table');
  }

  it 'maps a slot for bulma', {
    expect(Blogin::Framework::profile('bulma').class-for('image')).to.eq('image');
  }

  it 'is classless for pico', {
    expect(Blogin::Framework::profile('pico').class-for('table')).to.eq('');
  }

  it 'emits nothing for none', {
    expect(Blogin::Framework::profile('none').class-for('table')).to.eq('');
  }

  it 'rejects an unknown profile', {
    try Blogin::Framework::profile('tailwind');
    expect($!.message.contains('tailwind')).to.be-truthy;
  }
}

describe 'element classes on the same semantic html', {
  let(:table-md, { "| A | B |\n| - | - |\n| 1 | 2 |\n" });

  it 'adds none under the none profile', {
    expect(element-html(table-md(), 'none').contains("<table>\n")).to.be-truthy;
  }

  it 'adds the bootstrap5 table class', {
    expect(element-html(table-md(), 'bootstrap5').contains('<table class="table">')).to.be-truthy;
  }

  it 'adds the bulma table class', {
    expect(element-html(table-md(), 'bulma').contains('<table class="table">')).to.be-truthy;
  }

  it 'stays classless under pico', {
    expect(element-html(table-md(), 'pico').contains("<table>\n")).to.be-truthy;
  }

  it 'adds the bulma image class', {
    expect(element-html("![a](x.png)\n", 'bulma').contains('class="image"')).to.be-truthy;
  }
}

describe 'chrome reads from the profile', {
  let(:out, { $*TMPDIR.add("blogin-fw-{$*PID}-{$seq++}") });

  after-each { nuke(out()) }

  sub build-fw(Str $framework) {
    Blogin::Site::build(
      content => $PAGINATED.add('content'),
      out     => out(),
      layouts => $PAGINATED.add('layouts'),
      page-size => 2,
      :$framework,
    );
  }

  it 'adds the pagination class under bootstrap5', {
    build-fw('bootstrap5');
    expect(out().add('posts.html').slurp.contains('<nav class="pagination">')).to.be-truthy;
  }

  it 'leaves pagination unclassed under none', {
    build-fw('none');
    expect(out().add('posts.html').slurp.contains('<nav class="pagination">')).to.be-falsy;
  }
}

describe 'init wires the framework stylesheet', {
  let(:dir, { my $d = $*TMPDIR.add("blogin-fwinit-{$*PID}-{$seq++}"); $d.mkdir; $d });

  after-each { nuke(dir()) }

  it 'links pico for the pico framework', {
    Blogin::Scaffold::init(dir(), framework => 'pico', date => '2026-07-20');
    expect(dir().add('layouts/base.haml').slurp.contains('pico')).to.be-truthy;
  }

  it 'links bulma for the bulma framework', {
    Blogin::Scaffold::init(dir(), framework => 'bulma', date => '2026-07-20');
    expect(dir().add('layouts/base.haml').slurp.contains('bulma')).to.be-truthy;
  }
}
