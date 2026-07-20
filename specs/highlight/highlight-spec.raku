use lib 'lib';
use BDD::Behave;
use Blogin::Highlight;
use Blogin::Markdown;
use Blogin::Markdown::Html;

describe 'the syntax highlighter', {
  it 'wraps a keyword in a keyword span', {
    expect(Blogin::Highlight::highlight('my $x = 1', 'raku').contains('<span class="hl-keyword">my</span>')).to.be-truthy;
  }

  it 'highlights a number', {
    expect(Blogin::Highlight::highlight('my $x = 1', 'raku').contains('<span class="hl-number">1</span>')).to.be-truthy;
  }

  it 'highlights a line comment', {
    expect(Blogin::Highlight::highlight('code # a note', 'raku').contains('<span class="hl-comment"># a note</span>')).to.be-truthy;
  }

  it 'highlights a string', {
    expect(Blogin::Highlight::highlight('say "hi"', 'raku').contains('<span class="hl-string">"hi"</span>')).to.be-truthy;
  }

  it 'escapes html inside a string', {
    expect(Blogin::Highlight::highlight('say "<b>"', 'raku').contains('&lt;b&gt;')).to.be-truthy;
  }

  it 'leaves an unknown language escaped without spans', {
    my $out = Blogin::Highlight::highlight('class X', 'martian');
    expect($out.contains('hl-')).to.be-falsy;
  }

  it 'still escapes html for an unknown language', {
    expect(Blogin::Highlight::highlight('a < b', 'martian').contains('a &lt; b')).to.be-truthy;
  }
}

describe 'highlighting through the renderer', {
  sub code-html(Bool :$highlight) {
    my $doc = Blogin::Markdown::parse("```raku\nmy \$x = 1;\n```\n");
    Blogin::Markdown::Html.new(:$highlight).render($doc).html;
  }

  it 'highlights fenced code when enabled', {
    expect(code-html(highlight => True).contains('hl-keyword')).to.be-truthy;
  }

  it 'leaves fenced code plain when disabled', {
    expect(code-html(highlight => False).contains('hl-keyword')).to.be-falsy;
  }
}
