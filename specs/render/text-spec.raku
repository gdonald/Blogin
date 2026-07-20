use lib 'lib';
use BDD::Behave;
use Blogin::Markdown;
use Blogin::Markdown::Html;

sub text-of(Str $markdown) {
  my $doc = Blogin::Markdown::parse($markdown);
  Blogin::Markdown::Html.new.render($doc).text;
}

describe 'plain text extraction', {
  it 'strips inline marks to their text', {
    expect(text-of("**bold** and *em*\n").contains('bold and em')).to.be-truthy;
  }

  it 'keeps heading text', {
    expect(text-of("# A Heading\n").contains('A Heading')).to.be-truthy;
  }

  it 'includes code block content for search', {
    expect(text-of("```\nsay 1;\n```\n").contains('say 1;')).to.be-truthy;
  }

  it 'keeps link text', {
    expect(text-of("[click here](http://x.com)\n").contains('click here')).to.be-truthy;
  }

  it 'drops the link url', {
    expect(text-of("[click here](http://x.com)\n").contains('http')).to.be-falsy;
  }

  it 'keeps image alt text', {
    expect(text-of("![a cat](cat.png)\n").contains('a cat')).to.be-truthy;
  }

  it 'does not html-escape the plain text', {
    expect(text-of("a < b\n").contains('a < b')).to.be-truthy;
  }
}
