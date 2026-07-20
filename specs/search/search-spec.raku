use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Site;
use Blogin::Search;
use Template::HAML;
use JSON::Fast;

my $PAGINATED = 'specs/fixtures/paginated'.IO;


sub build(IO::Path:D $out, *%options) {
  Blogin::Site::build(
    content => $PAGINATED.add('content'),
    out     => $out,
    layouts => $PAGINATED.add('layouts'),
    |%options,
  );
}

my @FIXTURE-INDEX =
  %( title => 'Raku Grammars', url => '/posts/grammars', tags => ['raku'],   text => 'grammars parse text' ),
  %( title => 'A Web Note',    url => '/posts/web',      tags => ['web'],    text => 'raku on the web' ),
  %( title => 'Cooking',       url => '/posts/cooking',  tags => ['food'],   text => 'a raku mention once' );

describe 'the ranking logic', {
  it 'weights a title hit above a body hit', {
    my @ranked = Blogin::Search::rank(@FIXTURE-INDEX, 'raku');
    expect(@ranked[0]<title>).to.eq('Raku Grammars');
  }

  it 'ranks a tag hit above a body-only hit', {
    my @ranked = Blogin::Search::rank(@FIXTURE-INDEX, 'raku');
    expect(@ranked[1]<title>).to.eq('A Web Note');
  }

  it 'excludes records with no hits', {
    my @ranked = Blogin::Search::rank(@FIXTURE-INDEX, 'grammars');
    expect(@ranked.elems).to.eq(1);
  }

  it 'caps the number of results', {
    my @ranked = Blogin::Search::rank(@FIXTURE-INDEX, 'raku', cap => 2);
    expect(@ranked.elems).to.eq(2);
  }

  it 'returns nothing for an empty query', {
    expect(Blogin::Search::rank(@FIXTURE-INDEX, '').elems).to.eq(0);
  }
}

describe 'the search index', {
  let(:out, { temp-dir('search') });

  after-each { nuke(out()) }

  it 'writes search-index.json', {
    build(out());
    expect(out().add('search-index.json').e).to.be-truthy;
  }

  it 'has one record per built post', {
    build(out());
    my @records = from-json(out().add('search-index.json').slurp);
    expect(@records.elems).to.eq(4);
  }

  it 'records the post url and title', {
    build(out());
    my @records = from-json(out().add('search-index.json').slurp);
    my $alpha = @records.first({ .<url> eq '/posts/alpha' });
    expect($alpha<title>).to.eq('Alpha');
  }

  it 'truncates the body text to the configured length', {
    build(out(), search-text-length => 3);
    my @records = from-json(out().add('search-index.json').slurp);
    expect(@records.map(*.<text>.chars).max).to.be-less-than-or-equal-to(3);
  }

  it 'emits search.js', {
    build(out());
    expect(out().add('search.js').e).to.be-truthy;
  }

  it 'injects the result cap into search.js', {
    build(out(), search-cap => 7);
    expect(out().add('search.js').slurp.contains('BLOGIN_SEARCH_CAP = 7')).to.be-truthy;
  }

  it 'omits search output when search is disabled', {
    build(out(), search => False);
    expect(out().add('search-index.json').e).to.be-falsy;
  }
}

describe 'the search partial', {
  it 'renders a form and the script tag', {
    my $haml = HAML.new(search-paths => 'specs/fixtures/layouts');
    my $html = $haml.render(:file<search>);
    expect($html.contains('/search.js') && $html.contains('data-blogin-search')).to.be-truthy;
  }
}
