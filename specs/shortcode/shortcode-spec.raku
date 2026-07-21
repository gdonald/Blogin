use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Shortcode;
use Blogin::Markdown;
use Blogin::Markdown::Html;

sub html-of(Str $markdown) {
  Blogin::Markdown::Html.new.render(Blogin::Markdown::parse($markdown)).html;
}

describe 'Blogin::Shortcode::parse-args', {
  it 'parses quoted key-value pairs', {
    my %args = Blogin::Shortcode::parse-args('id="abc" width="640"');
    expect(%args<id> eq 'abc' && %args<width> eq '640').to.be-truthy;
  }
}

describe 'Blogin::Shortcode::known', {
  it 'knows a built-in shortcode', {
    expect(Blogin::Shortcode::known('youtube')).to.be-truthy;
  }

  it 'does not know an arbitrary name', {
    expect(Blogin::Shortcode::known('bogus')).to.be-falsy;
  }
}

describe 'shortcodes in Markdown', {
  it 'expands a youtube shortcode to an embed', {
    expect(html-of('{{< youtube id="abc123" >}}' ~ "\n").contains('youtube.com/embed/abc123')).to.be-truthy;
  }

  it 'expands a figure shortcode with a caption', {
    expect(html-of('{{< figure src="/p.png" alt="a" caption="A cat" >}}' ~ "\n").contains('<figcaption>A cat</figcaption>')).to.be-truthy;
  }

  it 'escapes html in a figure caption', {
    expect(html-of('{{< figure src="/p.png" caption="a < b" >}}' ~ "\n").contains('a &lt; b')).to.be-truthy;
  }

  it 'leaves an unknown shortcode as escaped text', {
    expect(html-of('{{< bogus x="1" >}}' ~ "\n").contains('&lt; bogus')).to.be-truthy;
  }

  it 'does not treat inline braces as a shortcode', {
    expect(html-of('text with {{< inline >}} braces mid sentence' ~ "\n").contains('<p>')).to.be-truthy;
  }
}

describe 'a shortcode through a build', {
  my $FIXTURE = 'specs/fixtures/helpers'.IO;

  let(:out, {
    my $dir = temp-dir('shortcode');
    build-fixture($FIXTURE, $dir);
    $dir
  });

  after-each { nuke(out()) }

  it 'expands a shortcode in the rendered post', {
    expect(out().add('posts/hello.html').slurp.contains('https://www.youtube.com/embed/abc123')).to.be-truthy;
  }
}

describe 'Blogin::Shortcode::render-template', {
  it 'substitutes a placeholder with an argument', {
    expect(Blogin::Shortcode::render-template('<p>{{ msg }}</p>', %( msg => 'Hi' ))).to.eq('<p>Hi</p>');
  }

  it 'escapes html in a substituted value', {
    expect(Blogin::Shortcode::render-template('{{ x }}', %( x => 'a<b' ))).to.eq('a&lt;b');
  }

  it 'drops a placeholder with no matching argument', {
    expect(Blogin::Shortcode::render-template('[{{ y }}]', %())).to.eq('[]');
  }
}

describe 'Blogin::Shortcode::load', {
  it 'loads a template file keyed by name', {
    my %templates = Blogin::Shortcode::load('specs/fixtures/shortcodes/shortcodes'.IO);
    expect(%templates<note>.contains('class="note"')).to.be-truthy;
  }

  it 'returns empty for a missing directory', {
    expect(Blogin::Shortcode::load('specs/fixtures/shortcodes/nope'.IO).elems).to.eq(0);
  }
}

describe 'a user-defined shortcode through a build', {
  my $FIXTURE = 'specs/fixtures/shortcodes'.IO;

  let(:out, {
    my $dir = temp-dir('user-shortcode');
    build-fixture($FIXTURE, $dir, shortcodes => $FIXTURE.add('shortcodes'));
    $dir
  });

  after-each { nuke(out()) }

  it 'expands a shortcode defined by a template file', {
    expect(out().add('posts/hi.html').slurp.contains('<aside class="note">Heads up</aside>')).to.be-truthy;
  }
}
