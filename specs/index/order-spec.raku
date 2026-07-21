use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;

my $ORDERED = 'specs/fixtures/ordered'.IO;

describe 'a section listing with explicit order keys', {
  let(:out,  { temp-dir('ordered') });
  let(:html, { build-fixture($ORDERED, out()); out().add('posts.html').slurp });

  after-each { nuke(out()) }

  it 'places a lower order before a higher order regardless of date', {
    expect(html().index('Bravo') < html().index('Alpha')).to.be-truthy;
  }

  it 'places every ordered post before an unordered one', {
    expect(html().index('Alpha') < html().index('Delta')).to.be-truthy;
  }

  it 'falls back to newest-date-first among unordered posts', {
    expect(html().index('Delta') < html().index('Charlie')).to.be-truthy;
  }
}
