use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Markdown;
use Blogin::Markdown::Html;

sub html-of(Str $markdown) {
  Blogin::Markdown::Html.new.render(Blogin::Markdown::parse($markdown)).html;
}

describe 'inline math', {
  it 'wraps inline math in an inline math span', {
    expect(html-of('The value $x^2$ here.' ~ "\n").contains('<span class="math math-inline">x^2</span>')).to.be-truthy;
  }

  it 'escapes html inside the math', {
    expect(html-of('$a < b$' ~ "\n").contains('<span class="math math-inline">a &lt; b</span>')).to.be-truthy;
  }

  it 'does not treat a prose dollar amount as math', {
    my $html = html-of('I paid $5 for $10 today.' ~ "\n");
    expect($html.contains('math-inline')).to.be-falsy;
  }

  it 'keeps a prose dollar amount as literal text', {
    expect(html-of('It cost $5 total.' ~ "\n").contains('$5')).to.be-truthy;
  }
}

describe 'display math', {
  it 'wraps double-dollar math in a display math span', {
    expect(html-of('$$a + b = c$$' ~ "\n").contains('<span class="math math-display">a + b = c</span>')).to.be-truthy;
  }

  it 'renders a fenced math block as display math', {
    expect(html-of("```math\n\\int x\\,dx\n```\n").contains('<div class="math math-display">')).to.be-truthy;
  }
}

describe 'mermaid diagrams', {
  it 'renders a fenced mermaid block as a mermaid pre', {
    expect(html-of("```mermaid\ngraph TD; A-->B;\n```\n").contains('<pre class="mermaid">')).to.be-truthy;
  }

  it 'keeps the diagram source in the pre', {
    expect(html-of("```mermaid\ngraph TD; A-->B;\n```\n").contains('graph TD; A--&gt;B;')).to.be-truthy;
  }

  it 'still highlights a normal code fence', {
    expect(html-of("```raku\nsay 1;\n```\n").contains('<code class="language-raku">')).to.be-truthy;
  }
}

describe 'math and diagrams through a build', {
  my $FIXTURE = 'specs/fixtures/math'.IO;

  let(:out, {
    my $dir = temp-dir('math');
    build-fixture($FIXTURE, $dir);
    $dir
  });

  after-each { nuke(out()) }

  it 'renders inline math in a built post', {
    expect(out().add('posts/paper.html').slurp.contains('class="math math-inline"')).to.be-truthy;
  }

  it 'renders a mermaid diagram in a built post', {
    expect(out().add('posts/paper.html').slurp.contains('<pre class="mermaid">')).to.be-truthy;
  }
}
