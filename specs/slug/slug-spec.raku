use lib 'lib';
use BDD::Behave;
use Blogin::Slug;

describe 'slugify', {
  it 'lowercases and hyphenates words', {
    expect(Blogin::Slug::slugify('Hello World')).to.eq('hello-world');
  }

  it 'collapses runs of punctuation to a single hyphen', {
    expect(Blogin::Slug::slugify('a, b & c')).to.eq('a-b-c');
  }

  it 'strips leading and trailing separators', {
    expect(Blogin::Slug::slugify('  Trim Me!  ')).to.eq('trim-me');
  }

  it 'drops non-ascii characters', {
    expect(Blogin::Slug::slugify('Café Crème')).to.eq('caf-cr-me');
  }

  it 'keeps digits', {
    expect(Blogin::Slug::slugify('Post 42')).to.eq('post-42');
  }
}

describe 'humanize', {
  it 'title-cases hyphen-separated words', {
    expect(Blogin::Slug::humanize('getting-started')).to.eq('Getting Started');
  }

  it 'splits on underscores too', {
    expect(Blogin::Slug::humanize('my_first_post')).to.eq('My First Post');
  }

  it 'is empty for an empty stem', {
    expect(Blogin::Slug::humanize('')).to.eq('');
  }
}
