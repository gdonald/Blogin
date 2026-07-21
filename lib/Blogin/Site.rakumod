use v6.d;

use Blogin::Post;
use Blogin::Layout;
use Blogin::Slug;
use Blogin::Nav;
use Blogin::Feed;
use Blogin::Search;
use Blogin::Style;
use Blogin::Data;
use Blogin::Summary;
use Blogin::Assets;

unit module Blogin::Site;

class BuildResult {
  has @.written;
  has @.rendered;
  has @.listings;
  has @.write-log;
}

sub all-files(IO::Path:D $dir) {
  return () unless $dir.d;

  gather for $dir.dir -> $entry {
    $entry.d ?? (.take for all-files($entry)) !! $entry.take;
  }
}

sub prune-empty-dirs(IO::Path:D $dir) {
  return unless $dir.d;

  for $dir.dir.grep(*.d) -> $sub {
    prune-empty-dirs($sub);
    $sub.rmdir if $sub.d && !$sub.dir.elems;
  }
}

sub remove-tree(IO::Path:D $path) {
  return unless $path.e;

  if $path.d {
    remove-tree($_) for $path.dir;
    $path.rmdir;
  }
  else {
    $path.unlink;
  }
}

sub within(IO::Path:D $target, IO::Path:D $base --> Bool) {
  my $inner = $target.absolute.chomp('/');
  my $outer = $base.absolute.chomp('/');

  $inner ne $outer && $inner.starts-with("$outer/");
}

our sub clean(IO() :$out!, IO() :$root = $*CWD --> Int) {
  my $target = $out.absolute.IO;
  my $base   = $root.absolute.IO;

  die "refusing to clean '{ $target }': it is not inside '{ $base }'"
    unless within($target, $base);

  return 0 unless $target.d;

  my $count = all-files($target).elems;

  remove-tree($target);

  $count;
}

# Change-detecting writer. Thread-safe; tracks written paths so prune() can
# remove stale output.
class Writer {
  has Bool  $.force = False;
  has       @.written;
  has       %!expected;
  has Lock  $!lock .= new;

  method write(IO::Path:D $file, Str $content) {
    $!lock.protect({
      %!expected{ $file.absolute } = True;

      if $!force || !$file.e || $file.slurp ne $content {
        $file.parent.mkdir;
        $file.spurt($content);
        @!written.push($file);
      }
    });
  }

  method copy(IO::Path:D $src, IO::Path:D $dest) {
    $!lock.protect({
      %!expected{ $dest.absolute } = True;

      my $content = $src.slurp(:bin);

      if $!force || !$dest.e || !($dest.slurp(:bin) eqv $content) {
        $dest.parent.mkdir;
        $dest.spurt($content);
        @!written.push($dest);
      }
    });
  }

  method prune(IO::Path:D $out) {
    return unless $out.d;

    for all-files($out) -> $file {
      $file.unlink unless %!expected{ $file.absolute };
    }

    prune-empty-dirs($out);
  }
}

# Resize each raster image in the output to the configured widths and add a
# srcset to every reference. Runs before fingerprinting so the variants are
# fingerprinted and their srcset URLs rewritten too.
sub build-responsive-images(IO::Path:D $out, @widths) {
  return unless @widths;

  my $tool = Blogin::Assets::resizer();

  unless $tool.chars {
    note 'blogin: responsive images requested but no image resizer (ImageMagick or sips) found; skipping';
    return;
  }

  my @images = all-files($out).grep({ Blogin::Assets::is-raster($_) }).List;
  my %srcsets;

  for @images -> $image {
    my $source-width = Blogin::Assets::image-width($image, $tool);

    next unless $source-width > 0;

    my @variants;
    for @widths.grep(* < $source-width).sort -> $width {
      my $variant = $image.parent.add(Blogin::Assets::variant-name($image.basename, $width));

      next unless Blogin::Assets::resize($image, $variant, $width, $tool);

      @variants.push(%( width => $width, url => '/' ~ $variant.relative($out) ));
    }

    next unless @variants;

    my $url = '/' ~ $image.relative($out);
    %srcsets{$url} = Blogin::Assets::srcset-value($url, $source-width, @variants);
  }

  return unless %srcsets;

  for all-files($out).grep({ so .extension.lc eq 'html' }) -> $file {
    my $text    = $file.slurp;
    my $rewrite = Blogin::Assets::add-srcset($text, %srcsets);

    $file.spurt($rewrite) if $rewrite ne $text;
  }
}

# Minify and fingerprint emitted and copied assets after the writer has settled
# the output. Runs on canonical filenames, so prune has already removed any
# fingerprinted files from a previous build.
sub optimize-assets(IO::Path:D $out, Bool :$minify!, Bool :$fingerprint!) {
  return unless $minify || $fingerprint;

  if $minify {
    for all-files($out).grep({ so .extension.lc eq any(<css js>) }) -> $file {
      my $original = $file.slurp;
      my $minified = $file.extension.lc eq 'css'
        ?? Blogin::Assets::minify-css($original)
        !! Blogin::Assets::minify-js($original);

      $file.spurt($minified) if $minified ne $original;
    }
  }

  if $fingerprint {
    my %manifest;

    for all-files($out).grep({ Blogin::Assets::is-fingerprintable($_) }) -> $file {
      my $hash     = Blogin::Assets::content-hash($file.slurp(:bin));
      my $new-name = Blogin::Assets::fingerprint-name($file.basename, $hash);

      next if $file.basename eq $new-name;

      my $new-file = $file.parent.add($new-name);

      %manifest{ '/' ~ $file.relative($out) } = '/' ~ $new-file.relative($out);
      $file.rename($new-file);
    }

    for all-files($out).grep({ so .extension.lc eq any(<html css>) }) -> $file {
      my $text    = $file.slurp;
      my $rewrite = Blogin::Assets::rewrite-refs($text, %manifest);

      $file.spurt($rewrite) if $rewrite ne $text;
    }
  }
}

sub copy-static(IO::Path:D $from, IO::Path:D $to, Writer $writer) {
  for $from.dir.sort(*.basename) -> $entry {
    my $dest = $to.add($entry.basename);

    $entry.d ?? copy-static($entry, $dest, $writer) !! $writer.copy($entry, $dest);
  }
}

sub find-markdown(IO::Path:D $dir) {
  return () unless $dir.d;

  gather for $dir.dir.sort(*.basename) -> $entry {
    if $entry.d {
      .take for find-markdown($entry);
    }
    elsif $entry.extension eq 'md' {
      take $entry;
    }
  }
}

sub section-of(IO::Path:D $file, IO::Path:D $content --> Str) {
  my $section = $file.parent.relative($content);

  $section eq '.' ?? '' !! $section;
}

sub date-key(%page) {
  %page<post>.date.defined ?? %page<post>.date.daycount !! 0;
}

sub order-key(%page) {
  %page<post>.order // Inf;
}

sub newest-first(@pages) {
  @pages.sort({
    (order-key($^a) <=> order-key($^b))
      || (date-key($^b) <=> date-key($^a))
      || ($^a<post>.slug leg $^b<post>.slug)
  });
}

sub by-section(@pages --> Hash) {
  my %grouped;
  %grouped{ .<section> }.push($_) for @pages;
  %grouped;
}

sub entry-of(%page, Str :$base = '') {
  %(
    title       => %page<post>.title,
    url         => $base ~ %page<url>,
    date        => %page<post>.date-str,
    description => %page<post>.description,
    summary     => %page<summary> // '',
  );
}

sub listing-url(Str $section, Int $page-num, Bool :$at-root, Bool :$clean-urls) {
  my $base = $at-root ?? '' !! $section;

  my $path = $page-num == 1
    ?? $base
    !! ($base.chars ?? "$base/page/$page-num" !! "page/$page-num");

  return '/' unless $path.chars;

  $clean-urls ?? "/$path" !! "/$path/";
}

sub listing-file(IO::Path:D $out, Str $section, Int $page-num, Bool :$at-root, Bool :$clean-urls) {
  my $base = $at-root ?? '' !! $section;

  if $page-num == 1 {
    return $out.add('index.html') unless $base.chars;
    return $clean-urls ?? $out.add("$base.html") !! $out.add($base).add('index.html');
  }

  my $rel = $base.chars ?? "$base/page/$page-num" !! "page/$page-num";

  $clean-urls ?? $out.add("$rel.html") !! $out.add($rel).add('index.html');
}

sub write-section-listing(
  Str $section, @sorted,
  Bool :$at-root = False,
  IO::Path:D :$out!, IO() :$layouts!, :%site, :%data, :@nav, Bool :$clean-urls!,
  Bool :$debug!, Int :$page-size!, Writer :$writer!, Str :$framework!,
  :@templates = ['index'],
  Bool :$index-dates = True,
) {
  my @written;

  my @chunks = @sorted.rotor($page-size, :partial);
  @chunks = ([],) unless @chunks;
  my $total = @chunks.elems;

  for @chunks.kv -> $index, @chunk {
    my $page-num = $index + 1;

    my $out-file = listing-file($out, $section, $page-num, :$at-root, :$clean-urls);
    my $url      = listing-url($section, $page-num, :$at-root, :$clean-urls);
    my $prev-url = $page-num > 1     ?? listing-url($section, $page-num - 1, :$at-root, :$clean-urls) !! '';
    my $next-url = $page-num < $total ?? listing-url($section, $page-num + 1, :$at-root, :$clean-urls) !! '';

    my @entries = @chunk.map({ entry-of($_) });

    my $html = Blogin::Layout::render-listing(
      :$layouts, :$section, :$url, :%site, :%data, :@nav, :@entries, :@templates,
      page-number => $page-num, total-pages => $total,
      :$prev-url, :$next-url, :$debug, :$framework, :$index-dates,
    );

    $writer.write($out-file, $html);
    @written.push($out-file);
  }

  @written;
}

sub build-listings(
  :@pages, :@nav, IO::Path:D :$out!, IO() :$layouts!, :%site, :%sections,
  Bool :$clean-urls!, Bool :$debug!, Int :$page-size!, Str :$home-section!,
  Writer :$writer!, Str :$framework!, :&data-for!,
  --> Array
) {
  my sub page-size-for(Str $section) {
    (%sections{$section}<page-size> // $page-size).Int;
  }

  my sub index-dates-for(Str $section) {
    (%sections{$section}<index-dates> // True).Bool;
  }

  my %by-section = by-section(@pages);
  my @written;

  for %by-section.keys.grep(*.chars).sort -> $section {
    @written.append: write-section-listing(
      $section, newest-first(%by-section{$section}),
      :$out, :$layouts, :%site, data => data-for($section), :@nav, :$clean-urls, :$debug, :$writer, :$framework,
      page-size => page-size-for($section),
      index-dates => index-dates-for($section),
    );
  }

  if $home-section.chars && (%by-section{$home-section}:exists) {
    @written.append: write-section-listing(
      $home-section, newest-first(%by-section{$home-section}),
      :at-root, :$out, :$layouts, :%site, data => data-for($home-section), :@nav, :$clean-urls, :$debug, :$writer, :$framework,
      page-size => page-size-for($home-section),
      index-dates => index-dates-for($home-section),
      templates => ['home', 'index'],
    );
  }

  @written;
}

sub build-taxonomy(
  Str $name,
  :@pages, :@nav, IO::Path:D :$out!, IO() :$layouts!, :%site, :%data,
  Bool :$clean-urls!, Bool :$debug!, Writer :$writer!, Str :$framework!,
  --> Array
) {
  my %term-posts;
  for @pages -> $page {
    %term-posts{$_}.push($page) for $page<post>.terms($name);
  }

  my @written;
  return @written unless %term-posts;

  my @term-templates  = ($name, $name.subst(/ 's' $ /, ''), 'term', 'index').unique;
  my @index-templates = ($name, 'index').unique;

  for %term-posts.keys.sort -> $term {
    my @sorted = newest-first(%term-posts{$term});

    my $slug     = Blogin::Slug::slugify($term);
    my $out-file = $clean-urls ?? $out.add("$name/$slug.html") !! $out.add("$name/$slug").add('index.html');
    my $url      = $clean-urls ?? "/$name/$slug" !! "/$name/$slug/";

    my @entries = @sorted.map({ entry-of($_) });

    my $html = Blogin::Layout::render-listing(
      :$layouts, :$url, :%site, :%data, :@nav, :@entries, templates => @term-templates, :$debug, :$framework,
    );

    $writer.write($out-file, $html);
    @written.push($out-file);
  }

  my @term-entries = %term-posts.keys.sort.map(-> $term {
    my $slug = Blogin::Slug::slugify($term);
    %(
      title => "$term ({ %term-posts{$term}.elems })",
      url   => ($clean-urls ?? "/$name/$slug" !! "/$name/$slug/"),
      date  => '',
    )
  });

  my $index-file = $clean-urls ?? $out.add("$name.html") !! $out.add($name).add('index.html');
  my $index-url  = $clean-urls ?? "/$name" !! "/$name/";

  my $html = Blogin::Layout::render-listing(
    :$layouts, url => $index-url, :%site, :%data, :@nav, entries => @term-entries, templates => @index-templates, :$debug, :$framework,
  );

  $writer.write($index-file, $html);
  @written.push($index-file);

  @written;
}

sub build-taxonomies(
  @taxonomies,
  :@pages, :@nav, IO::Path:D :$out!, IO() :$layouts!, :%site,
  Bool :$clean-urls!, Bool :$debug!, Writer :$writer!, Str :$framework!, :&data-for!,
  --> Array
) {
  my %data = data-for('');

  my @written;

  for @taxonomies -> $name {
    @written.append: build-taxonomy(
      $name, :@pages, :@nav, :$out, :$layouts, :%site, :%data,
      :$clean-urls, :$debug, :$writer, :$framework,
    );
  }

  @written;
}

sub file-to-url(IO::Path:D $file, IO::Path:D $out, Bool $clean-urls --> Str) {
  my $rel = $file.relative($out);

  return '/' if $rel eq 'index.html';

  if $clean-urls {
    $rel ~~ s/ '.html' $ //;
    return "/$rel";
  }

  $rel ~~ s/ '/index.html' $ //;
  "/$rel/";
}

sub newest-date(@sorted) {
  @sorted ?? @sorted[0]<post>.date-str !! '';
}

sub robots-txt(Str $base --> Str) {
  my @lines = 'User-agent: *', 'Allow: /';

  @lines.push("Sitemap: { $base.subst(/ '/' $ /, '') }/sitemap.xml") if $base.chars;

  @lines.join("\n") ~ "\n";
}

sub feed-filename(Str $format --> Str) {
  given $format {
    when 'rss'  { 'rss.xml' }
    when 'json' { 'feed.json' }
    default     { 'feed.xml' }
  }
}

sub render-feed(Str $format, %args --> Str) {
  given $format {
    when 'rss'  { Blogin::Feed::rss(|%args) }
    when 'json' { Blogin::Feed::json-feed(|%args) }
    default     { Blogin::Feed::atom(|%args) }
  }
}

sub build-feeds(
  :@pages, :@page-files, IO::Path:D :$out!, :%site, Bool :$clean-urls!, Bool :$robots!,
  :@feed-formats!, Writer :$writer!,
  --> Array
) {
  my @written;

  my $base  = %site<base-url> // '';
  my $title = %site<title>    // '';

  my @all = newest-first(@pages);

  my %by-section = by-section(@pages);

  for @feed-formats -> $format {
    my $file = feed-filename($format);

    my $site-feed = render-feed($format, %(
      :$title,
      site-url => "$base/",
      feed-url => "$base/$file",
      updated  => newest-date(@all),
      entries  => @all.map({ entry-of($_, :$base) }),
    ));

    $writer.write($out.add($file), $site-feed);
    @written.push($out.add($file));

    for %by-section.keys.grep(*.chars).sort -> $section {
      my @sorted = newest-first(%by-section{$section});

      my $feed = render-feed($format, %(
        title    => "$title: $section",
        site-url => "$base/$section",
        feed-url => "$base/$section/$file",
        updated  => newest-date(@sorted),
        entries  => @sorted.map({ entry-of($_, :$base) }),
      ));

      my $section-file = $out.add($section).add($file);
      $writer.write($section-file, $feed);
      @written.push($section-file);
    }
  }

  my @locs = @page-files.map({ $base ~ file-to-url($_, $out, $clean-urls) }).sort;
  my $sitemap = Blogin::Feed::sitemap(locs => @locs);

  $writer.write($out.add('sitemap.xml'), $sitemap);
  @written.push($out.add('sitemap.xml'));

  if $robots {
    $writer.write($out.add('robots.txt'), robots-txt($base));
    @written.push($out.add('robots.txt'));
  }

  @written;
}

our sub build(
  IO()  :$content!,
  IO()  :$out!,
  IO()  :$layouts = $content.parent.add('layouts'),
  IO()  :$static  = $content.parent.add('static'),
  IO()  :$data    = $content.parent.add('data'),
        :%site = %(),
  Bool  :$drafts = False,
  Int   :$jobs = ($*KERNEL.cpu-cores // 1),
  Bool  :$clean-urls = True,
  Str   :$framework = 'none',
  Bool  :$debug = False,
  Int   :$page-size = 10,
  Str   :$home-section = '',
        :%sections = %(),
        :@taxonomies = ['tags'],
        :@feed-formats = ['atom'],
  Bool  :$search = True,
  Int   :$search-text-length = 2000,
  Int   :$search-cap = 10,
  Bool  :$highlight = False,
  Int   :$summary-length = 200,
  Bool  :$robots = True,
  Bool  :$minify = False,
  Bool  :$fingerprint = False,
        :@image-widths = [],
  Bool  :$force = False,
  --> BuildResult
) {
  my @nav = Blogin::Nav::build-tree($content, :%sections, :$clean-urls);

  my %data-global = Blogin::Data::load($data);
  my %data-cache;
  my $data-lock = Lock.new;

  my sub data-for(Str $section --> Hash) {
    $data-lock.protect({
      %data-cache{$section} //= Blogin::Data::resolve(%data-global, $content, $section);
    });
  }

  $out.mkdir unless $out.d;

  my $writer = Writer.new(:$force);

  my @pages;
  my %seen;

  for find-markdown($content) -> $file {
    my $post    = Blogin::Post.load($file);
    my $section = section-of($file, $content);

    next if $post.draft && !$drafts;

    my $url-path = $section.chars ?? "$section/{ $post.slug }" !! $post.slug;
    my $url      = $clean-urls ?? "/$url-path" !! "/$url-path/";
    my $out-file = $clean-urls
      ?? $out.add("$url-path.html")
      !! $out.add($url-path).add('index.html');

    my $key = $out-file.absolute;

    if %seen{$key}:exists {
      die "two posts resolve to the same URL '$url': "
        ~ "{ %seen{$key} } and { $file }";
    }

    %seen{$key} = $file.Str;

    @pages.push(%(
      post     => $post,
      section  => $section,
      url      => $url,
      out-file => $out-file,
    ));
  }

  my %prev-of;
  my %next-of;

  for by-section(@pages).values -> @section-pages {
    my @sorted = newest-first(@section-pages);

    for @sorted.kv -> $index, $page {
      %prev-of{$page<url>} = @sorted[$index - 1] if $index > 0;
      %next-of{$page<url>} = @sorted[$index + 1] if $index < @sorted.end;
    }
  }

  my @ordered = @pages.sort({ -.<post>.body.chars });

  my $haml-lock = Lock.new;

  my @written = @ordered.hyper(:degree($jobs max 1), :batch(1)).map(-> $page {
    my $parts = Blogin::Layout::render-parts(
      post      => $page<post>,
      framework => $framework,
      highlight => $highlight,
    );

    $page<text>     = $parts<text>;
    $page<headings> = $parts<headings>;

    my $body    = $page<post>.body;
    my $excerpt = $body.contains(Blogin::Summary::MORE)
      ?? Blogin::Layout::plain-text($body.substr(0, $body.index(Blogin::Summary::MORE)), :$framework)
      !! '';

    $page<summary> = Blogin::Summary::choose(
      explicit => $page<post>.summary,
      :$excerpt,
      text     => $parts<text>,
      length   => $summary-length,
    );

    my $show-dates = (%sections{$page<section>}<show-dates> // True).Bool;

    my $section-layout = (%sections{$page<section>}<layout> // '').Str;
    my @templates = $section-layout.chars ?? [$section-layout, 'show'] !! ['show'];

    my $prev = %prev-of{$page<url>};
    my $next = %next-of{$page<url>};

    my $html = $haml-lock.protect({
      Blogin::Layout::render-with-layout(
        post      => $page<post>,
        body-html => $parts<html>,
        layouts   => $layouts,
        site      => %site,
        section   => $page<section>,
        url       => $page<url>,
        nav       => @nav,
        debug     => $debug,
        framework => $framework,
        show-dates => $show-dates,
        templates  => @templates,
        data       => data-for($page<section>),
        summary    => $page<summary>,
        headings   => $page<headings>,
        prev-url   => ($prev ?? $prev<url> !! ''),
        prev-title => ($prev ?? $prev<post>.title !! ''),
        next-url   => ($next ?? $next<url> !! ''),
        next-title => ($next ?? $next<post>.title !! ''),
      );
    });

    $writer.write($page<out-file>, $html);
    $page<out-file>;
  }).List;

  my @listings = build-listings(
    :@pages, :@nav, :$out, :$layouts, :%site, :%sections, :$clean-urls, :$debug,
    :$page-size, :$home-section, :$writer, :$framework, :&data-for,
  );

  @listings.append: build-taxonomies(
    @taxonomies.map({ $_ ~~ Str ?? $_ !! |$_ }),
    :@pages, :@nav, :$out, :$layouts, :%site, :$clean-urls, :$debug, :$writer, :$framework, :&data-for,
  );

  if @pages.elems {
    my @page-files = [ |@written, |@listings ];

    @listings.append: build-feeds(
      :@pages, :@page-files, :$out, :%site, :$clean-urls, :$robots, :$writer,
      feed-formats => @feed-formats.map({ $_ ~~ Str ?? $_ !! |$_ }),
    );

    if $search {
      my $index-file = $out.add('search-index.json');
      $writer.write($index-file, Blogin::Search::index-json(@pages, text-length => $search-text-length));
      @listings.push: $index-file;

      my $js-file = $out.add('search.js');
      $writer.write($js-file, Blogin::Search::search-js(cap => $search-cap));
      @listings.push: $js-file;

      my $css-file = $out.add('search.css');
      $writer.write($css-file, Blogin::Search::search-css());
      @listings.push: $css-file;
    }

    my $style-file = $out.add('blogin.css');
    $writer.write($style-file, Blogin::Style::content-css());
    @listings.push: $style-file;
  }

  copy-static($static, $out, $writer) if $static.d;

  $writer.prune($out);

  build-responsive-images($out, @image-widths.map({ $_ ~~ Int ?? $_ !! |$_ }));
  optimize-assets($out, :$minify, :$fingerprint);

  BuildResult.new(
    written   => @written,
    rendered  => @pages,
    listings  => @listings,
    write-log => $writer.written,
  );
}
