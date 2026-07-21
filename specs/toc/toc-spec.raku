use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Toc;

sub headings(@triples) {
  @triples.map(-> @triple { %( level => @triple[0], text => @triple[1], id => @triple[2] ) }).List;
}

describe 'Blogin::Toc::build', {
  let(:tree, {
    Blogin::Toc::build(headings([
      [1, 'One',   'one'],
      [2, 'One A', 'one-a'],
      [2, 'One B', 'one-b'],
      [1, 'Two',   'two'],
    ]))
  });

  it 'roots the top-level headings', {
    expect(tree().elems).to.eq(2);
  }

  it 'nests a deeper heading under its parent', {
    expect(tree()[0]<children>.elems).to.eq(2);
  }

  it 'keeps sibling order within a parent', {
    expect(tree()[0]<children>[1]<id>).to.eq('one-b');
  }

  it 'leaves a childless heading with no children', {
    expect(tree()[1]<children>.elems).to.eq(0);
  }
}

describe 'Blogin::Toc::build with three levels', {
  let(:tree, {
    Blogin::Toc::build(headings([
      [1, 'A', 'a'],
      [2, 'B', 'b'],
      [3, 'C', 'c'],
      [1, 'D', 'd'],
    ]))
  });

  it 'nests a third level under the second', {
    expect(tree()[0]<children>[0]<children>[0]<title>).to.eq('C');
  }

  it 'closes back to the root on a shallower heading', {
    expect(tree()[1]<title>).to.eq('D');
  }
}

describe 'Blogin::Toc::render', {
  let(:html, {
    Blogin::Toc::render(Blogin::Toc::build(headings([
      [2, 'Intro',   'intro'],
      [2, 'Details', 'details'],
      [3, 'Deep',    'deep'],
    ])))
  });

  it 'links each heading to its anchor', {
    expect(html().contains('<a href="#details">Details</a>')).to.be-truthy;
  }

  it 'nests a child list under its parent item', {
    expect(html().contains('Details</a><ul><li><a href="#deep">Deep</a>')).to.be-truthy;
  }

  it 'renders nothing for no headings', {
    expect(Blogin::Toc::render([])).to.eq('');
  }

  it 'escapes a heading title', {
    my $out = Blogin::Toc::render(Blogin::Toc::build(headings([
      [2, 'A < B', 'a-b'],
      [2, 'Plain', 'plain'],
    ])));
    expect($out.contains('A &lt; B')).to.be-truthy;
  }
}

describe 'a table of contents through a build', {
  my $FIXTURE = 'specs/fixtures/toc'.IO;

  let(:out,   { temp-dir('toc') });
  let(:guide, { build-fixture($FIXTURE, out()); out().add('posts/guide.html').slurp });
  let(:plain, { build-fixture($FIXTURE, out()); out().add('posts/plain.html').slurp });

  after-each { nuke(out()) }

  it 'renders the toc nav when the post opts in', {
    expect(guide().contains("<nav class='toc'>")).to.be-truthy;
  }

  it 'links a toc entry to its heading anchor', {
    expect(guide().contains('<a href="#details">Details</a>')).to.be-truthy;
  }

  it 'nests a deeper heading in the toc', {
    expect(guide().contains('<a href="#deep">Deep</a>')).to.be-truthy;
  }

  it 'anchors the toc link to the rendered heading id', {
    expect(guide().contains('id="details"')).to.be-truthy;
  }

  it 'omits the toc for a post that does not opt in', {
    expect(plain().contains("<nav class='toc'>")).to.be-falsy;
  }
}
