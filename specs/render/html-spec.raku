use lib 'lib';
use BDD::Behave;
use Blogin::Markdown;
use Blogin::Markdown::Html;
use Blogin::Framework;

sub html-of(Str $markdown, Str :$framework = 'none') {
  my $doc = Blogin::Markdown::parse($markdown);
  Blogin::Markdown::Html.new(framework => Blogin::Framework::profile($framework)).render($doc).html;
}

describe 'a paragraph', {
  it 'wraps inline content in a p tag', {
    expect(html-of("hello\n").contains('<p>hello</p>')).to.be-truthy;
  }

  it 'escapes html metacharacters', {
    expect(html-of("a < b & c\n").contains('a &lt; b &amp; c')).to.be-truthy;
  }
}

describe 'a heading', {
  let(:html, { html-of("## The Title\n") });

  it 'renders the level tag with a slug id', {
    expect(html().contains('<h2 id="the-title"')).to.be-truthy;
  }

  it 'renders a self link to its own anchor', {
    expect(html().contains('href="#the-title"')).to.be-truthy;
  }
}

describe 'inline marks', {
  it 'renders emphasis', {
    expect(html-of("*hi*\n").contains('<em>hi</em>')).to.be-truthy;
  }

  it 'renders strong', {
    expect(html-of("**bold**\n").contains('<strong>bold</strong>')).to.be-truthy;
  }

  it 'renders strikethrough', {
    expect(html-of("~~gone~~\n").contains('<del>gone</del>')).to.be-truthy;
  }

  it 'renders a code span', {
    expect(html-of("`x`\n").contains('<code>x</code>')).to.be-truthy;
  }
}

describe 'a link', {
  it 'renders an anchor with the url', {
    expect(html-of("[text](http://x.com)\n").contains('<a href="http://x.com"')).to.be-truthy;
  }

  it 'renders the title attribute', {
    expect(html-of("[text](http://x.com \"Home\")\n").contains('title="Home"')).to.be-truthy;
  }

  it 'renders attribute-list attributes', {
    expect(html-of("[o](http://x.com)\{target=_blank\}\n").contains('target="_blank"')).to.be-truthy;
  }
}

describe 'an image', {
  it 'renders an img tag with src and alt', {
    expect(html-of("![a cat](cat.png)\n").contains('<img src="cat.png" alt="a cat"')).to.be-truthy;
  }
}

describe 'a fenced code block', {
  let(:html, { html-of("```raku\nsay 1;\n```\n") });

  it 'emits a language class from the info string', {
    expect(html().contains('<code class="language-raku">')).to.be-truthy;
  }

  it 'keeps the escaped body', {
    expect(html().contains('say 1;')).to.be-truthy;
  }
}

describe 'a blockquote', {
  it 'wraps its content in a blockquote tag', {
    expect(html-of("> quoted\n").contains('<blockquote>')).to.be-truthy;
  }
}

describe 'a thematic break', {
  it 'renders an hr', {
    expect(html-of("---\n").contains('<hr />')).to.be-truthy;
  }
}

describe 'lists', {
  it 'renders an unordered list', {
    expect(html-of("- one\n- two\n").contains('<li>one</li>')).to.be-truthy;
  }

  it 'renders an ordered list with a start number', {
    expect(html-of("3. three\n4. four\n").contains('<ol start="3">')).to.be-truthy;
  }
}

describe 'a task list', {
  let(:html, { html-of("- [x] done\n") });

  it 'renders a disabled checkbox', {
    expect(html().contains('<input type="checkbox" disabled')).to.be-truthy;
  }

  it 'marks a checked item', {
    expect(html().contains('checked')).to.be-truthy;
  }
}

describe 'a table', {
  let(:html, { html-of("| A | B |\n| :- | -: |\n| 1 | 2 |\n") });

  it 'renders a table tag', {
    expect(html().contains('<table>')).to.be-truthy;
  }

  it 'renders a left-aligned header cell', {
    expect(html().contains('<th style="text-align:left">')).to.be-truthy;
  }

  it 'renders body cells', {
    expect(html().contains('<td')).to.be-truthy;
  }
}

describe 'css-framework classes', {
  let(:table-md, { "| A | B |\n| - | - |\n| 1 | 2 |\n" });

  it 'adds no element classes under the none profile', {
    expect(html-of(table-md(), framework => 'none').contains("<table>\n")).to.be-truthy;
  }

  it 'adds the framework table class under bootstrap5', {
    expect(html-of(table-md(), framework => 'bootstrap5').contains('<table class="table">')).to.be-truthy;
  }

  it 'adds the framework blockquote class under bootstrap5', {
    expect(html-of("> hi\n", framework => 'bootstrap5').contains('<blockquote class="blockquote">')).to.be-truthy;
  }

  it 'adds the framework image class under bootstrap5', {
    expect(html-of("![c](c.png)\n", framework => 'bootstrap5').contains('class="img-fluid"')).to.be-truthy;
  }
}

describe 'a definition list', {
  let(:html, { html-of("Term\n: A definition\n") });

  it 'wraps the block in a dl', {
    expect(html().contains('<dl>')).to.be-truthy;
  }

  it 'renders the term as a dt', {
    expect(html().contains('<dt>Term</dt>')).to.be-truthy;
  }

  it 'renders the definition as a dd', {
    expect(html().contains('<dd>A definition</dd>')).to.be-truthy;
  }

  it 'emits no bullet list markup', {
    expect(html().contains('<ul>') || html().contains('<li>')).to.be-falsy;
  }

  it 'supports several definitions for one term', {
    my $out = html-of("Term\n: First\n: Second\n");
    expect($out.contains('<dd>First</dd>') && $out.contains('<dd>Second</dd>')).to.be-truthy;
  }

  it 'renders inline markup inside a definition', {
    expect(html-of("Term\n: has **bold** text\n").contains('<strong>bold</strong>')).to.be-truthy;
  }
}
