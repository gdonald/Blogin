use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Scaffold;
use Blogin::Site;

my $BASIC = 'specs/fixtures/basic'.IO;


describe 'creating a new post', {
  let(:content, { temp-made('new') });

  after-each { nuke(content()) }

  it 'writes the post under the section with a dated slug', {
    Blogin::Scaffold::new-post('My First Post', content => content(), section => 'posts', date => '2026-07-20');
    expect(content().add('posts/2026-07-20-my-first-post.md').e).to.be-truthy;
  }

  it 'fills a front matter title stub', {
    my $file = Blogin::Scaffold::new-post('My First Post', content => content(), section => 'posts', date => '2026-07-20');
    expect($file.slurp.contains('title: "My First Post"')).to.be-truthy;
  }

  it 'places a section-less post at the content root', {
    Blogin::Scaffold::new-post('Loose', content => content(), date => '2026-07-20');
    expect(content().add('2026-07-20-loose.md').e).to.be-truthy;
  }

  it 'refuses to overwrite an existing post', {
    Blogin::Scaffold::new-post('Dup', content => content(), section => 'posts', date => '2026-07-20');
    try Blogin::Scaffold::new-post('Dup', content => content(), section => 'posts', date => '2026-07-20');
    expect($!.message.contains('already exists')).to.be-truthy;
  }
}

describe 'the new post is buildable', {
  let(:content, { temp-made('newbuild') });
  let(:out,     { temp-dir('newout') });

  after-each {
    nuke(content());
    nuke(out());
  }

  it 'builds into a real page', {
    Blogin::Scaffold::new-post('Fresh Idea', content => content(), section => 'posts', date => '2026-07-20');
    Blogin::Site::build(content => content(), out => out(), layouts => $BASIC.add('layouts'));
    expect(out().add('posts/fresh-idea.html').e).to.be-truthy;
  }
}
