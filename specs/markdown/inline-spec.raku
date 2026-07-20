use lib 'lib';
use BDD::Behave;
use Blogin::Markdown;
use Blogin::Markdown::Node;

sub inline-of(Str $markdown) {
  Blogin::Markdown::parse($markdown).children[0].children;
}

describe 'inline text', {
  let(:nodes, { inline-of("plain words\n") });

  it 'is a single Text node', {
    expect(nodes()[0] ~~ Text).to.be-truthy;
  }

  it 'holds the literal text', {
    expect(nodes()[0].text).to.eq('plain words');
  }
}

describe 'emphasis', {
  let(:node, { inline-of("*hi*\n")[0] });

  it 'is an Emphasis node', {
    expect(node() ~~ Emphasis).to.be-truthy;
  }

  it 'wraps the inner text', {
    expect(node().children[0].text).to.eq('hi');
  }
}

describe 'strong', {
  let(:node, { inline-of("**bold**\n")[0] });

  it 'is a Strong node', {
    expect(node() ~~ Strong).to.be-truthy;
  }

  it 'wraps the inner text', {
    expect(node().children[0].text).to.eq('bold');
  }
}

describe 'strikethrough', {
  let(:node, { inline-of("~~gone~~\n")[0] });

  it 'is a Strikethrough node', {
    expect(node() ~~ Strikethrough).to.be-truthy;
  }
}

describe 'code span', {
  let(:node, { inline-of("`x = 1`\n")[0] });

  it 'is a CodeSpan node', {
    expect(node() ~~ CodeSpan).to.be-truthy;
  }

  it 'keeps the literal code', {
    expect(node().text).to.eq('x = 1');
  }
}

describe 'a link', {
  let(:node, { inline-of("[here](http://example.com \"Home\")\n")[0] });

  it 'is a Link node', {
    expect(node() ~~ Link).to.be-truthy;
  }

  it 'captures the url', {
    expect(node().url).to.eq('http://example.com');
  }

  it 'captures the title', {
    expect(node().title).to.eq('Home');
  }

  it 'parses the link text as inline children', {
    expect(node().children[0].text).to.eq('here');
  }
}

describe 'a link with an attribute list', {
  let(:node, { inline-of("[out](http://x.com)\{target=_blank rel=noopener .ext\}\n")[0] });

  it 'sets the target attribute', {
    expect(node().attrs<target>).to.eq('_blank');
  }

  it 'sets the rel attribute', {
    expect(node().attrs<rel>).to.eq('noopener');
  }

  it 'collects classes', {
    expect(node().attrs<class>).to.eq('ext');
  }
}

describe 'an image', {
  let(:node, { inline-of("![a cat](cat.png \"Cat\")\n")[0] });

  it 'is an Image node', {
    expect(node() ~~ Image).to.be-truthy;
  }

  it 'captures the url', {
    expect(node().url).to.eq('cat.png');
  }

  it 'captures the alt text', {
    expect(node().alt).to.eq('a cat');
  }
}

describe 'an autolink', {
  let(:node, { inline-of("<https://example.com>\n")[0] });

  it 'is a Link node', {
    expect(node() ~~ Link).to.be-truthy;
  }

  it 'uses the url as its own text', {
    expect(node().children[0].text).to.eq('https://example.com');
  }
}

describe 'a backslash escape', {
  let(:nodes, { inline-of("\\*not emphasis\\*\n") });

  it 'produces no Emphasis node', {
    expect(nodes().grep(Emphasis).elems).to.eq(0);
  }

  it 'keeps the asterisks as text', {
    expect(nodes().map(*.text).join).to.eq('*not emphasis*');
  }
}

describe 'a soft break', {
  let(:nodes, { inline-of("one\ntwo\n") });

  it 'separates the lines with a SoftBreak', {
    expect(nodes()[1] ~~ SoftBreak).to.be-truthy;
  }
}

describe 'a hard break', {
  let(:nodes, { inline-of("one  \ntwo\n") });

  it 'separates the lines with a LineBreak', {
    expect(nodes()[1] ~~ LineBreak).to.be-truthy;
  }
}
