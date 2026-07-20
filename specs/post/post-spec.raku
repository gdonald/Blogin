use lib 'lib';
use BDD::Behave;
use Blogin::Post;

describe 'a post with full front matter', {
  let(:source, {
    "---\ntitle: Hello World\ndate: 2026-07-19\nslug: custom-slug\ntags: [raku, blog]\ndraft: true\ndescription: A first post\n---\nBody text here.\n"
  });

  let(:post, { Blogin::Post.parse(source(), filename => '2026-07-19-hello.md') });

  it 'reads the title', {
    expect(post().title).to.eq('Hello World');
  }

  it 'reads the date', {
    expect(post().date.Str).to.eq('2026-07-19');
  }

  it 'reads the explicit slug', {
    expect(post().slug).to.eq('custom-slug');
  }

  it 'reads the tags', {
    expect(post().tags.join(',')).to.eq('raku,blog');
  }

  it 'reads the draft flag', {
    expect(post().draft).to.be-truthy;
  }

  it 'reads the description', {
    expect(post().description).to.eq('A first post');
  }

  it 'separates the body', {
    expect(post().body.contains('Body text here.')).to.be-truthy;
  }

  it 'keeps the front matter out of the body', {
    expect(post().body.contains('title:')).to.be-falsy;
  }
}

describe 'a post with partial front matter', {
  let(:post, { Blogin::Post.parse("---\ntitle: My Post\ndate: 2026-01-02\n---\nHi\n", filename => 'p.md') });

  it 'derives the slug from the title', {
    expect(post().slug).to.eq('my-post');
  }

  it 'defaults draft to false', {
    expect(post().draft).to.be-falsy;
  }

  it 'defaults the description to empty', {
    expect(post().description).to.eq('');
  }
}

describe 'a post with no front matter', {
  let(:post, { Blogin::Post.parse("Just body content\n", filename => '2026-07-19-hello-world.md') });

  it 'derives the title from the filename', {
    expect(post().title).to.eq('Hello World');
  }

  it 'derives the date from the filename prefix', {
    expect(post().date.Str).to.eq('2026-07-19');
  }

  it 'derives the slug from the derived title', {
    expect(post().slug).to.eq('hello-world');
  }

  it 'treats the whole file as the body', {
    expect(post().body.contains('Just body content')).to.be-truthy;
  }
}

describe 'front matter values', {
  it 'reads comma-separated tags without brackets', {
    my $post = Blogin::Post.parse("---\ntitle: T\ntags: a, b, c\n---\nx\n", filename => 'f.md');
    expect($post.tags.join(',')).to.eq('a,b,c');
  }

  it 'keeps unknown keys as raw metadata', {
    my $post = Blogin::Post.parse("---\ntitle: T\nauthor: Greg\n---\nx\n", filename => 'f.md');
    expect($post.meta<author>).to.eq('Greg');
  }
}

describe 'the error path', {
  it 'raises when the title cannot be determined', {
    expect({ Blogin::Post.parse("body\n", filename => '2026-07-19-.md') }).to.throw;
  }

  it 'names the file in a missing-title error', {
    try Blogin::Post.parse("body\n", filename => '2026-07-19-.md');
    expect($!.message.contains('2026-07-19-.md')).to.be-truthy;
  }

  it 'raises on an unparseable date', {
    expect({ Blogin::Post.parse("---\ntitle: T\ndate: nope\n---\nx\n", filename => 'f.md') }).to.throw;
  }

  it 'names the file in an unparseable-date error', {
    try Blogin::Post.parse("---\ntitle: T\ndate: nope\n---\nx\n", filename => 'bad.md');
    expect($!.message.contains('bad.md')).to.be-truthy;
  }
}
