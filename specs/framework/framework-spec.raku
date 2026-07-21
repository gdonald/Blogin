use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Framework;
use Blogin::Markdown;
use Blogin::Markdown::Html;
use Blogin::Site;
use Blogin::Scaffold;
use Blogin;
use Blogin::Config;
use Blogin::Log;

my $PAGINATED = 'specs/fixtures/paginated'.IO;


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

  it 'maps the post-nav button to a primary button for bootstrap5', {
    expect(Blogin::Framework::profile('bootstrap5').class-for('post-nav-button')).to.eq('btn btn-primary');
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

  it 'exposes the bootstrap5 stylesheet url', {
    expect(Blogin::Framework::profile('bootstrap5').stylesheet.contains('bootstrap')).to.be-truthy;
  }

  it 'exposes the pico stylesheet url', {
    expect(Blogin::Framework::profile('pico').stylesheet.contains('pico')).to.be-truthy;
  }

  it 'exposes the bulma stylesheet url', {
    expect(Blogin::Framework::profile('bulma').stylesheet.contains('bulma')).to.be-truthy;
  }

  it 'exposes no stylesheet for none', {
    expect(Blogin::Framework::profile('none').stylesheet).to.eq('');
  }

  it 'exposes the bootstrap5 script bundle', {
    expect(Blogin::Framework::profile('bootstrap5').script.contains('bootstrap.bundle')).to.be-truthy;
  }

  it 'exposes no script for a css-only framework', {
    expect(Blogin::Framework::profile('bulma').script).to.eq('');
  }

  it 'exposes no script for none', {
    expect(Blogin::Framework::profile('none').script).to.eq('');
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
  let(:out, { temp-dir('fw') });

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

describe 'the built layout links the configured framework stylesheet', {
  let(:dir, { temp-made('fwinit') });
  let(:out, { temp-made('fwinit-out') });

  after-each {
    nuke(dir());
    nuke(out());
  }

  sub build-init {
    build(
      src    => dir().add('content'),
      config => Blogin::Config.load(dir().add('blogin.json')),
      out    => out(),
      log    => Blogin::Log.new(:level('quiet')),
    );
  }

  it 'links pico for the pico framework', {
    Blogin::Scaffold::init(dir(), framework => 'pico', date => '2026-07-20');
    build-init;
    expect(out().add('index.html').slurp.contains('pico')).to.be-truthy;
  }

  it 'links bulma for the bulma framework', {
    Blogin::Scaffold::init(dir(), framework => 'bulma', date => '2026-07-20');
    build-init;
    expect(out().add('index.html').slurp.contains('bulma')).to.be-truthy;
  }
}
