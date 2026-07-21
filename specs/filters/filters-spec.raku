use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Filters;

describe 'Blogin::Filters::truncate', {
  it 'truncates at a word boundary with an ellipsis', {
    expect(Blogin::Filters::truncate('one two three four five', 12)).to.eq('one two…');
  }

  it 'leaves short text untouched', {
    expect(Blogin::Filters::truncate('short', 20)).to.eq('short');
  }
}

describe 'Blogin::Filters::format-date', {
  it 'formats a long month and year', {
    expect(Blogin::Filters::format-date('2026-07-19', '%B %e, %Y')).to.eq('July 19, 2026');
  }

  it 'formats an abbreviated month with a padded day', {
    expect(Blogin::Filters::format-date('2026-07-05', '%b %d')).to.eq('Jul 05');
  }

  it 'returns the input when it is not an iso date', {
    expect(Blogin::Filters::format-date('not-a-date', '%Y')).to.eq('not-a-date');
  }
}

describe 'Blogin::Filters::group-by', {
  let(:groups, {
    my @items;
    @items.push: %( year => '2026', title => 'a' );
    @items.push: %( year => '2025', title => 'b' );
    @items.push: %( year => '2026', title => 'c' );
    Blogin::Filters::group-by(@items, 'year')
  });

  it 'groups items by a field value', {
    expect(groups()[0]<items>.elems).to.eq(2);
  }

  it 'orders the groups by key descending', {
    expect(groups()[0]<key>).to.eq('2026');
  }

  it 'keeps a separate group for another key', {
    expect(groups()[1]<key>).to.eq('2025');
  }
}

describe 'a filter used in a layout', {
  my $FIXTURE = 'specs/fixtures/helpers'.IO;

  let(:out, {
    my $dir = temp-dir('filters');
    build-fixture($FIXTURE, $dir);
    $dir
  });

  after-each { nuke(out()) }

  it 'formats the post date through the format-date filter', {
    expect(out().add('posts/hello.html').slurp.contains('July 19, 2026')).to.be-truthy;
  }
}
