use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Summary;

describe 'Blogin::Summary::first-block', {
  it 'returns the first block of stripped text', {
    expect(Blogin::Summary::first-block("First block.\nSecond block.", 200)).to.eq('First block.');
  }

  it 'truncates a block longer than the limit at a word boundary', {
    expect(Blogin::Summary::first-block('one two three four five', 12)).to.eq('one two…');
  }

  it 'leaves a block within the limit untouched', {
    expect(Blogin::Summary::first-block('short enough', 200)).to.eq('short enough');
  }
}

describe 'Blogin::Summary::choose', {
  it 'prefers an explicit summary over everything else', {
    expect(Blogin::Summary::choose(explicit => 'Chosen', excerpt => 'Marker', text => 'Body')).to.eq('Chosen');
  }

  it 'uses the marker excerpt when there is no explicit summary', {
    expect(Blogin::Summary::choose(excerpt => 'Marker', text => 'Body')).to.eq('Marker');
  }

  it 'falls back to the first block of the body text', {
    expect(Blogin::Summary::choose(text => "Body block.\nMore.")).to.eq('Body block.');
  }
}

describe 'summaries through a build', {
  my $FIXTURE = 'specs/fixtures/summaries'.IO;

  let(:out,     { temp-dir('summaries') });
  let(:listing, { build-fixture($FIXTURE, out()); out().add('posts.html').slurp });
  let(:feed,    { build-fixture($FIXTURE, out()); out().add('feed.xml').slurp });

  after-each { nuke(out()) }

  it 'shows the marker excerpt in the listing', {
    expect(listing().contains('This is the teaser part.')).to.be-truthy;
  }

  it 'keeps the post-marker body out of the listing summary', {
    expect(listing().contains('stays out of the summary')).to.be-falsy;
  }

  it 'shows an explicit front-matter summary in the listing', {
    expect(listing().contains('A hand-written summary.')).to.be-truthy;
  }

  it 'shows the first-block fallback in the listing', {
    expect(listing().contains('The first paragraph becomes the teaser')).to.be-truthy;
  }

  it 'keeps the second paragraph out of the fallback summary', {
    expect(listing().contains('stays out of the first block')).to.be-falsy;
  }

  it 'emits the marker excerpt as an Atom summary', {
    expect(feed().contains('<summary>This is the teaser part.</summary>')).to.be-truthy;
  }

  it 'emits the explicit summary as an Atom summary', {
    expect(feed().contains('<summary>A hand-written summary.</summary>')).to.be-truthy;
  }
}

describe 'the more marker in the rendered post', {
  my $FIXTURE = 'specs/fixtures/summaries'.IO;

  let(:out,  { temp-dir('summaries-post') });
  let(:page, { build-fixture($FIXTURE, out()); out().add('posts/marker-post.html').slurp });

  after-each { nuke(out()) }

  it 'does not leave the marker comment in the output', {
    expect(page().contains('<!--more-->')).to.be-falsy;
  }

  it 'still renders the full body of the post', {
    expect(page().contains('stays out of the summary')).to.be-truthy;
  }
}
