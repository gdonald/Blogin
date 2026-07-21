use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Assets;

describe 'Blogin::Assets::minify-css', {
  it 'strips comments', {
    expect(Blogin::Assets::minify-css('/* note */ body { color: red; }').contains('note')).to.be-falsy;
  }

  it 'collapses whitespace and closes rules tightly', {
    expect(Blogin::Assets::minify-css("body \{\n  color: red;\n\}\n")).to.eq('body{color:red}');
  }
}

describe 'Blogin::Assets::minify-js', {
  it 'drops blank lines and whole-line comments', {
    expect(Blogin::Assets::minify-js("// header\nvar a = 1;\n\nvar b = 2;\n")).to.eq("var a = 1;\nvar b = 2;");
  }

  it 'keeps line breaks so semicolon insertion is safe', {
    expect(Blogin::Assets::minify-js("  a()\n  b()\n").contains("\n")).to.be-truthy;
  }
}

describe 'Blogin::Assets::content-hash', {
  it 'is stable for the same content', {
    expect(Blogin::Assets::content-hash('abc')).to.eq(Blogin::Assets::content-hash('abc'));
  }

  it 'differs for different content', {
    expect(Blogin::Assets::content-hash('abc')).to.not.eq(Blogin::Assets::content-hash('abd'));
  }

  it 'is eight hex characters', {
    expect(Blogin::Assets::content-hash('abc') ~~ / ^ <[0..9 a..f]> ** 8 $ /).to.be-truthy;
  }
}

describe 'Blogin::Assets::fingerprint-name', {
  it 'inserts the hash before the extension', {
    expect(Blogin::Assets::fingerprint-name('style.css', 'abc12345')).to.eq('style.abc12345.css');
  }

  it 'appends the hash when there is no extension', {
    expect(Blogin::Assets::fingerprint-name('LICENSE', 'abc12345')).to.eq('LICENSE.abc12345');
  }
}

describe 'Blogin::Assets::rewrite-refs', {
  let(:manifest, { %( '/style.css' => '/style.abc123.css' ) });

  it 'rewrites a double-quoted reference', {
    expect(Blogin::Assets::rewrite-refs('<link href="/style.css">', manifest())).to.eq('<link href="/style.abc123.css">');
  }

  it 'rewrites a css url() reference', {
    expect(Blogin::Assets::rewrite-refs('background: url(/style.css);', manifest())).to.eq('background: url(/style.abc123.css);');
  }

  it 'leaves an unrelated reference alone', {
    expect(Blogin::Assets::rewrite-refs('<a href="/other.css">', manifest())).to.eq('<a href="/other.css">');
  }
}

describe 'mirroring the assets source directory', {
  my $FIXTURE = 'specs/fixtures/assets'.IO;

  let(:out, {
    my $dir = temp-dir('assets-src');
    build-fixture($FIXTURE, $dir);
    $dir
  });

  after-each { nuke(out()) }

  it 'copies a source css asset into public/assets/css', {
    expect(out().add('assets/css/site.css').e).to.be-truthy;
  }

  it 'copies a source js asset into public/assets/js preserving structure', {
    expect(out().add('assets/js/app.js').e).to.be-truthy;
  }

  it 'places the generated content stylesheet under assets/css', {
    expect(out().add('assets/css/blogin.css').e).to.be-truthy;
  }
}

describe 'minifying and fingerprinting through a build', {
  my $FIXTURE = 'specs/fixtures/assets'.IO;

  let(:out, {
    my $dir = temp-dir('assets');
    build-fixture($FIXTURE, $dir, minify => True, fingerprint => True);
    $dir
  });

  after-each { nuke(out()) }

  sub fingerprinted(IO::Path:D $dir, Str $stem, Str $ext) {
    $dir.dir.first({ .basename ~~ / ^ "$stem." <[0..9 a..f]>+ ".$ext" $ / });
  }

  it 'renames the stylesheet to a fingerprinted name', {
    expect(fingerprinted(out(), 'style', 'css').defined).to.be-truthy;
  }

  it 'removes the unfingerprinted stylesheet', {
    expect(out().add('style.css').e).to.be-falsy;
  }

  it 'minifies the fingerprinted stylesheet', {
    my $file = fingerprinted(out(), 'style', 'css');
    expect($file.slurp.contains('/*')).to.be-falsy;
  }

  it 'minifies a javascript asset, dropping comment and blank lines', {
    my $file = fingerprinted(out().add('assets/js'), 'app', 'js');
    expect($file.slurp.contains('// a comment') || $file.slurp ~~ /\n\n/).to.be-falsy;
  }

  it 'rewrites the page reference to the fingerprinted stylesheet', {
    my $name = fingerprinted(out(), 'style', 'css').basename;
    expect(out().add('posts/hello.html').slurp.contains("/$name")).to.be-truthy;
  }

  it 'fingerprints the generated content stylesheet too', {
    expect(fingerprinted(out().add('assets/css'), 'blogin', 'css').defined && !out().add('assets/css/blogin.css').e).to.be-truthy;
  }
}
