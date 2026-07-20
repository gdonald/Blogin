use lib 'lib';
use BDD::Behave;
use Blogin::Markdown;
use Blogin::Markdown::Node;

sub blocks-of(Str $markdown) {
  Blogin::Markdown::parse($markdown).children;
}

describe 'a table', {
  let(:node, { blocks-of("| A | B |\n| :- | -: |\n| 1 | 2 |\n")[0] });

  it 'is a Table node', {
    expect(node() ~~ Table).to.be-truthy;
  }

  it 'captures the header cells', {
    expect(node().header.elems).to.eq(2);
  }

  it 'reads the left alignment from the delimiter', {
    expect(node().aligns[0]).to.eq('left');
  }

  it 'reads the right alignment from the delimiter', {
    expect(node().aligns[1]).to.eq('right');
  }

  it 'captures the body rows', {
    expect(node().rows.elems).to.eq(1);
  }
}

describe 'a task list', {
  let(:node, { blocks-of("- [ ] todo\n- [x] done\n")[0] });

  it 'marks items as tasks', {
    expect(node().items[0].task).to.be-truthy;
  }

  it 'leaves an unchecked box unchecked', {
    expect(node().items[0].checked).to.be-falsy;
  }

  it 'reads a checked box', {
    expect(node().items[1].checked).to.be-truthy;
  }

  it 'strips the checkbox from the item text', {
    expect(node().items[0].children[0].children[0].text).to.eq('todo');
  }
}
