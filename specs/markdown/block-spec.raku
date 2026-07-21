use lib 'lib';
use BDD::Behave;
use Blogin::Markdown;
use Blogin::Markdown::Node;

sub blocks-of(Str $markdown) {
  Blogin::Markdown::parse($markdown).children;
}

describe 'a paragraph', {
  let(:node, { blocks-of("hello world\n")[0] });

  it 'is a Paragraph node', {
    expect(node() ~~ Paragraph).to.be-truthy;
  }

  it 'holds its inline text', {
    expect(node().children[0].text).to.eq('hello world');
  }
}

describe 'an ATX heading', {
  let(:node, { blocks-of("### Title here\n")[0] });

  it 'is a Heading node', {
    expect(node() ~~ Heading).to.be-truthy;
  }

  it 'captures the level from the hash count', {
    expect(node().level).to.eq(3);
  }

  it 'captures the heading text', {
    expect(node().children[0].text).to.eq('Title here');
  }
}

describe 'an ATX heading with closing hashes', {
  let(:node, { blocks-of("## Middle ##\n")[0] });

  it 'strips the trailing hashes from the text', {
    expect(node().children[0].text).to.eq('Middle');
  }
}

describe 'a setext heading', {
  let(:node, { blocks-of("The Title\n=========\n")[0] });

  it 'is a Heading node', {
    expect(node() ~~ Heading).to.be-truthy;
  }

  it 'is level one for an equals underline', {
    expect(node().level).to.eq(1);
  }
}

describe 'a thematic break', {
  let(:node, { blocks-of("---\n")[0] });

  it 'is a ThematicBreak node', {
    expect(node() ~~ ThematicBreak).to.be-truthy;
  }
}

describe 'a fenced code block', {
  let(:node, { blocks-of("```raku\nsay 1;\n```\n")[0] });

  it 'is a CodeBlock node', {
    expect(node() ~~ CodeBlock).to.be-truthy;
  }

  it 'captures the info string', {
    expect(node().info).to.eq('raku');
  }

  it 'keeps the literal body', {
    expect(node().text).to.eq("say 1;\n");
  }
}

describe 'a blockquote', {
  let(:node, { blocks-of("> quoted line\n")[0] });

  it 'is a BlockQuote node', {
    expect(node() ~~ BlockQuote).to.be-truthy;
  }

  it 'parses its content as blocks', {
    expect(node().children[0] ~~ Paragraph).to.be-truthy;
  }
}

describe 'a nested blockquote', {
  let(:node, { blocks-of("> outer\n> > inner\n")[0] });

  it 'contains a nested BlockQuote', {
    expect(node().children.grep(BlockQuote).elems).to.eq(1);
  }
}

describe 'an unordered list', {
  let(:node, { blocks-of("- one\n- two\n")[0] });

  it 'is a List node', {
    expect(node() ~~ List).to.be-truthy;
  }

  it 'is not ordered', {
    expect(node().ordered).to.be-falsy;
  }

  it 'has one item per marker', {
    expect(node().items.elems).to.eq(2);
  }
}

describe 'an ordered list with a start number', {
  let(:node, { blocks-of("3. three\n4. four\n")[0] });

  it 'is ordered', {
    expect(node().ordered).to.be-truthy;
  }

  it 'captures the start number', {
    expect(node().start).to.eq(3);
  }
}

describe 'a nested list', {
  let(:node, { blocks-of("- outer\n    - inner\n")[0] });

  it 'nests a List inside the item', {
    expect(node().items[0].children.grep(List).elems).to.eq(1);
  }
}

describe 'a definition list', {
  let(:node, { blocks-of("Term\n: A definition\n")[0] });

  it 'is a DefinitionList node', {
    expect(node() ~~ DefinitionList).to.be-truthy;
  }

  it 'captures the term text', {
    expect(node().items[0].term[0].text).to.eq('Term');
  }

  it 'captures the definition text', {
    expect(node().items[0].definitions[0][0].text).to.eq('A definition');
  }
}
