use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Metrics;

describe 'Blogin::Metrics::word-count', {
  it 'counts the words in the text', {
    expect(Blogin::Metrics::word-count('one two three four five')).to.eq(5);
  }

  it 'is zero for empty text', {
    expect(Blogin::Metrics::word-count('')).to.eq(0);
  }
}

describe 'Blogin::Metrics::reading-time', {
  it 'rounds up to whole minutes at the default pace', {
    expect(Blogin::Metrics::reading-time(250)).to.eq(2);
  }

  it 'is at least one minute for any words', {
    expect(Blogin::Metrics::reading-time(5)).to.eq(1);
  }

  it 'is zero for no words', {
    expect(Blogin::Metrics::reading-time(0)).to.eq(0);
  }

  it 'honors a custom words-per-minute', {
    expect(Blogin::Metrics::reading-time(300, wpm => 100)).to.eq(3);
  }
}

describe 'post metrics through a build', {
  my $FIXTURE = 'specs/fixtures/metadata'.IO;

  let(:out, {
    my $dir = temp-dir('metadata');
    build-fixture($FIXTURE, $dir);
    $dir
  });

  after-each { nuke(out()) }

  it 'exposes the word count to the layout', {
    expect(out().add('posts/alpha.html').slurp.contains("<p class='words'>5</p>")).to.be-truthy;
  }

  it 'exposes the reading time to the layout', {
    expect(out().add('posts/alpha.html').slurp.contains("<p class='reading'>1</p>")).to.be-truthy;
  }
}

describe 'related posts through a build', {
  my $FIXTURE = 'specs/fixtures/metadata'.IO;

  let(:related, {
    my $dir = temp-dir('related');
    build-fixture($FIXTURE, $dir);
    my $html = $dir.add('posts/alpha.html').slurp;
    nuke($dir);
    $html
  });

  it 'lists a post that shares taxonomy terms', {
    expect(related().contains('Bravo')).to.be-truthy;
  }

  it 'orders the most-shared post first', {
    expect(related().index('Bravo') < related().index('Charlie')).to.be-truthy;
  }

  it 'excludes a post with no shared terms', {
    expect(related().contains('Echo')).to.be-falsy;
  }

  it 'excludes a future-dated post from related', {
    expect(related().contains('Future')).to.be-falsy;
  }
}

describe 'the related-count limit', {
  my $FIXTURE = 'specs/fixtures/metadata'.IO;

  let(:related, {
    my $dir = temp-dir('related-cap');
    build-fixture($FIXTURE, $dir, related-count => 1);
    my $html = $dir.add('posts/alpha.html').slurp;
    nuke($dir);
    $html
  });

  it 'shows the single most-related post', {
    expect(related().contains('Bravo')).to.be-truthy;
  }

  it 'drops related posts beyond the configured count', {
    expect(related().contains('Charlie') || related().contains('Delta')).to.be-falsy;
  }
}

describe 'future-dated posts', {
  my $FIXTURE = 'specs/fixtures/metadata'.IO;

  it 'excludes a future-dated post by default', {
    my $dir = temp-dir('future-off');
    build-fixture($FIXTURE, $dir);
    my $exists = $dir.add('posts/future.html').e;
    nuke($dir);
    expect($exists).to.be-falsy;
  }

  it 'includes a future-dated post with the future flag', {
    my $dir = temp-dir('future-on');
    build-fixture($FIXTURE, $dir, future => True);
    my $exists = $dir.add('posts/future.html').e;
    nuke($dir);
    expect($exists).to.be-truthy;
  }
}
