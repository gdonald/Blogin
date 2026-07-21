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

describe 'the supported languages', {
  sub keyword-of(Str $code, Str $language, Str $keyword) {
    Blogin::Highlight::highlight($code, $language).contains('<span class="hl-keyword">' ~ $keyword ~ '</span>');
  }

  it 'highlights a c keyword', {
    expect(keyword-of('int x = 1;', 'c', 'int')).to.be-truthy;
  }

  it 'highlights a cpp keyword', {
    expect(keyword-of('namespace app {}', 'cpp', 'namespace')).to.be-truthy;
  }

  it 'highlights a java keyword', {
    expect(keyword-of('public class X {}', 'java', 'public')).to.be-truthy;
  }

  it 'highlights a go keyword', {
    expect(keyword-of('func main() {}', 'go', 'func')).to.be-truthy;
  }

  it 'highlights a rust keyword', {
    expect(keyword-of('fn main() {}', 'rust', 'fn')).to.be-truthy;
  }

  it 'highlights a typescript keyword', {
    expect(keyword-of('interface P {}', 'typescript', 'interface')).to.be-truthy;
  }

  it 'highlights a double-slash line comment', {
    expect(Blogin::Highlight::highlight('code // a note', 'go').contains('<span class="hl-comment">// a note</span>')).to.be-truthy;
  }

  it 'reports a supported language', {
    expect(Blogin::Highlight::supports('rust')).to.be-truthy;
  }

  it 'reports an unsupported language', {
    expect(Blogin::Highlight::supports('martian')).to.be-falsy;
  }
}

describe 'labeling an unhighlighted block', {
  sub code-html(Str $info, Bool :$highlight) {
    my $doc = Blogin::Markdown::parse("```$info\ncode here\n```\n");
    Blogin::Markdown::Html.new(:$highlight).render($doc).html;
  }

  it 'labels an unsupported language as plain when highlighting', {
    expect(code-html('martian', highlight => True).contains('hl-plain')).to.be-truthy;
  }

  it 'does not label a supported language as plain', {
    expect(code-html('rust', highlight => True).contains('hl-plain')).to.be-falsy;
  }

  it 'does not label anything when highlighting is off', {
    expect(code-html('martian', highlight => False).contains('hl-plain')).to.be-falsy;
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
