use lib 'lib';
use BDD::Behave;
use Blogin::Post;
use Blogin::Layout;

my $LAYOUTS = 'specs/fixtures/layouts'.IO;

sub post-of(Str $title, Str $body, Str :$filename = 'p.md') {
  Blogin::Post.parse("---\ntitle: $title\ndate: 2026-07-19\n---\n$body\n", :$filename);
}

sub render(Str $title, Str $body, *%options) {
  Blogin::Layout::render-post(
    post => post-of($title, $body),
    layouts => $LAYOUTS,
    site => %( title => 'My Site' ),
    |%options,
  );
}

describe 'a build without debug', {
  it 'emits no html comments at all', {
    expect(render('Hi', 'body').contains('<!--')).to.be-falsy;
  }
}

describe 'a build with debug', {
  let(:html, { render('Hi', 'the body', debug => True) });

  it 'wraps the header partial in boundary comments', {
    expect(html().contains('<!-- begin partial: header -->')).to.be-truthy;
  }

  it 'closes the header partial boundary', {
    expect(html().contains('<!-- end partial: header -->')).to.be-truthy;
  }

  it 'wraps the show template in boundary comments', {
    expect(html().contains('<!-- begin template: show -->')).to.be-truthy;
  }

  it 'precedes the post body with a provenance comment', {
    expect(html().contains('<!-- source: p.md slug=hi title=Hi -->')).to.be-truthy;
  }
}

describe 'comment sanitization', {
  let(:html, { render('Evil --> injection', 'body', debug => True) });

  it 'does not leak an early comment terminator from the title', {
    expect(html().contains('Evil --> injection -->')).to.be-falsy;
  }

  it 'still emits a provenance comment', {
    expect(html().contains('<!-- source:')).to.be-truthy;
  }
}

describe 'debug determinism', {
  it 'produces identical debug output on repeated renders', {
    my $first  = render('Same', 'body', debug => True);
    my $second = render('Same', 'body', debug => True);
    expect($first eq $second).to.be-truthy;
  }
}
