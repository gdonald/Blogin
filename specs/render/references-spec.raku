use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Markdown;
use Blogin::Markdown::Html;

sub html-of(Str $markdown) {
  Blogin::Markdown::Html.new.render(Blogin::Markdown::parse($markdown)).html;
}

describe 'reference-style links', {
  it 'resolves a full reference to its definition', {
    my $html = html-of("See [the site][home].\n\n[home]: https://example.com\n");
    expect($html.contains('<a href="https://example.com">the site</a>')).to.be-truthy;
  }

  it 'resolves a collapsed reference using its own text as the label', {
    my $html = html-of("[Example][]\n\n[example]: https://e.com\n");
    expect($html.contains('<a href="https://e.com">Example</a>')).to.be-truthy;
  }

  it 'applies a definition title to the link', {
    my $html = html-of("[x][id]\n\n[id]: /u \"A Title\"\n");
    expect($html.contains('title="A Title"')).to.be-truthy;
  }

  it 'matches a label case-insensitively', {
    my $html = html-of("[x][ID]\n\n[id]: /u\n");
    expect($html.contains('href="/u"')).to.be-truthy;
  }

  it 'leaves an unresolved reference as literal text', {
    my $html = html-of("[x][missing]\n");
    expect($html.contains('[x][missing]')).to.be-truthy;
  }

  it 'does not render a definition line as content', {
    my $html = html-of("[id]: /secret\n");
    expect($html.contains('/secret')).to.be-falsy;
  }
}

describe 'footnotes', {
  let(:html, { html-of("Here[^1].\n\n[^1]: The note.\n") });

  it 'renders a reference as a numbered superscript link', {
    expect(html().contains('<sup class="footnote-ref"><a href="#fn-1" id="fnref-1">1</a></sup>')).to.be-truthy;
  }

  it 'renders the footnotes section item', {
    expect(html().contains('<li id="fn-1">')).to.be-truthy;
  }

  it 'renders the definition content in the item', {
    expect(html().contains('The note.')).to.be-truthy;
  }

  it 'links the footnote back to its reference', {
    expect(html().contains('href="#fnref-1"')).to.be-truthy;
  }

  it 'wraps the notes in a footnotes section', {
    expect(html().contains('<section class="footnotes">')).to.be-truthy;
  }
}

describe 'footnote numbering', {
  it 'numbers footnotes in first-reference order', {
    my $html = html-of("A[^b] then B[^a].\n\n[^a]: aaa\n[^b]: bbb\n");
    expect($html.contains('id="fnref-b">1') && $html.contains('id="fnref-a">2')).to.be-truthy;
  }

  it 'reuses one number for a repeated reference', {
    my $html = html-of("X[^n] and Y[^n].\n\n[^n]: note\n");
    expect($html.comb('<li id="fn-n">').elems).to.eq(1);
  }

  it 'gives a repeated reference a distinct anchor id', {
    my $html = html-of("X[^n] and Y[^n].\n\n[^n]: note\n");
    expect($html.contains('id="fnref-n-2"')).to.be-truthy;
  }

  it 'leaves a reference with no definition as literal text', {
    my $html = html-of("See[^nope].\n");
    expect($html.contains('[^nope]')).to.be-truthy;
  }
}

describe 'references through a build', {
  my $FIXTURE = 'specs/fixtures/references'.IO;

  let(:out,  { temp-dir('references') });
  let(:page, { build-fixture($FIXTURE, out()); out().add('posts/notes.html').slurp });

  after-each { nuke(out()) }

  it 'resolves a reference link in a built post', {
    expect(page().contains('<a href="https://example.com">reference link</a>')).to.be-truthy;
  }

  it 'renders the footnote reference in a built post', {
    expect(page().contains('class="footnote-ref"')).to.be-truthy;
  }

  it 'renders the footnotes section in a built post', {
    expect(page().contains('The cited source.')).to.be-truthy;
  }
}
