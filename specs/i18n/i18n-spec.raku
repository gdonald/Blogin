use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin;
use Blogin::Config;
use Blogin::Log;
use Blogin::Site;

my $FIXTURE = 'specs/fixtures/i18n'.IO;

describe 'Blogin::Config language keys', {
  it 'defaults languages to empty', {
    expect(Blogin::Config.new.languages.elems).to.eq(0);
  }

  it 'reads the languages list in order', {
    my @langs = Blogin::Config.from-data(%( languages => ['en', 'fr'] )).languages;
    expect(@langs.join(',')).to.eq('en,fr');
  }

  it 'reads a per-language title override', {
    my $config = Blogin::Config.from-data(%( language-config => %( fr => %( title => 'Mon Site' ) ) ));
    expect($config.language-config<fr><title>).to.eq('Mon Site');
  }

  it 'rejects a non-map language-config', {
    try Blogin::Config.from-data(%( language-config => 'nope' ));
    expect($!.message.contains('language-config')).to.be-truthy;
  }
}

describe 'Blogin::Site::url-paths', {
  it 'maps a filename translation key to its url-path', {
    my %paths = Blogin::Site::url-paths($FIXTURE.add('content/fr'));
    expect(%paths<posts/hello>).to.eq('posts/bonjour');
  }
}

describe 'building two language trees', {
  let(:out, {
    my $dir    = temp-dir('i18n');
    my $config = Blogin::Config.load($FIXTURE.add('blogin.json'));
    Blogin::build(src => $FIXTURE.add('content'), :$config, out => $dir, log => Blogin::Log.new(level => 'quiet'));
    $dir
  });

  after-each { nuke(out()) }

  it 'writes the english post under its language prefix', {
    expect(out().add('en/posts/hello.html').e).to.be-truthy;
  }

  it 'writes the french post under its own prefix with a translated slug', {
    expect(out().add('fr/posts/bonjour.html').e).to.be-truthy;
  }

  it 'writes a listing per language', {
    expect(out().add('en/posts.html').e && out().add('fr/posts.html').e).to.be-truthy;
  }

  it 'writes a feed per language rooted at the language url', {
    expect(out().add('en/feed.xml').slurp.contains('<id>https://e.com/en/</id>')).to.be-truthy;
  }

  it 'applies the per-language title', {
    expect(out().add('en/posts/hello.html').slurp.contains("<p class='site'>My Site</p>")).to.be-truthy;
  }

  it 'applies the other per-language title', {
    expect(out().add('fr/posts/bonjour.html').slurp.contains("<p class='site'>Mon Site</p>")).to.be-truthy;
  }

  it 'redirects the site root to the default language', {
    expect(out().add('index.html').slurp.contains('url=/en/')).to.be-truthy;
  }
}

describe 'the language switcher cross-links', {
  let(:hello, {
    my $dir    = temp-dir('i18n-switch');
    my $config = Blogin::Config.load($FIXTURE.add('blogin.json'));
    Blogin::build(src => $FIXTURE.add('content'), :$config, out => $dir, log => Blogin::Log.new(level => 'quiet'));
    my $html = $dir.add('en/posts/hello.html').slurp;
    nuke($dir);
    $html
  });

  it 'links to the same post in the current language', {
    expect(hello().contains("href='/en/posts/hello'")).to.be-truthy;
  }

  it 'links to the translation in the other language', {
    expect(hello().contains("href='/fr/posts/bonjour'")).to.be-truthy;
  }
}

describe 'a post with no translation', {
  let(:solo, {
    my $dir    = temp-dir('i18n-solo');
    my $config = Blogin::Config.load($FIXTURE.add('blogin.json'));
    Blogin::build(src => $FIXTURE.add('content'), :$config, out => $dir, log => Blogin::Log.new(level => 'quiet'));
    my $html = $dir.add('en/posts/solo.html').slurp;
    nuke($dir);
    $html
  });

  it 'links the untranslated language to its home page', {
    expect(solo().contains("href='/fr/'")).to.be-truthy;
  }
}

describe 'a section listing switcher', {
  let(:listing, {
    my $dir    = temp-dir('i18n-listing');
    my $config = Blogin::Config.load($FIXTURE.add('blogin.json'));
    Blogin::build(src => $FIXTURE.add('content'), :$config, out => $dir, log => Blogin::Log.new(level => 'quiet'));
    my $html = $dir.add('en/posts.html').slurp;
    nuke($dir);
    $html
  });

  it 'links the section to the same section in the other language', {
    expect(listing().contains("href='/fr/posts'")).to.be-truthy;
  }
}
