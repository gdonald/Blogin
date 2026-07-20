use v6.d;

use Blogin::Post;
use Blogin::Layout;
use Blogin::Slug;
use Blogin::Nav;
use Blogin::Feed;
use Blogin::Search;

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

# Writes only files whose content changed, records the write log, and tracks
# every produced path so stale output can be pruned. Thread-safe for the
# parallel post writes.
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
  IO::Path:D :$out!, IO() :$layouts!, :%site, :@nav, Bool :$clean-urls!,
  Bool :$debug!, Int :$page-size!, Writer :$writer!, Str :$framework!,
) {
  my @written;

  my @chunks = @sorted.rotor($page-size, :partial);
  @chunks = ([],) unless @chunks;
  my $total = @chunks.elems;

  for @chunks.kv -> $index, @chunk {
    my $page-num = $index + 1;

    my $out-file = listing-file($out, $section, $page-num, :$at-root, :$clean-urls);
    my $prev-url = $page-num > 1     ?? listing-url($section, $page-num - 1, :$at-root, :$clean-urls) !! '';
    my $next-url = $page-num < $total ?? listing-url($section, $page-num + 1, :$at-root, :$clean-urls) !! '';

    my @entries = @chunk.map({
      %( title => .<post>.title, url => .<url>, date => (.<post>.date.defined ?? .<post>.date.Str !! '') )
    });

    my $html = Blogin::Layout::render-listing(
      :$layouts, :$section, :%site, :@nav, :@entries,
      page-number => $page-num, total-pages => $total,
      :$prev-url, :$next-url, :$debug, :$framework,
    );

    $writer.write($out-file, $html);
    @written.push($out-file);
  }

  @written;
}

sub build-listings(
  :@pages, :@nav, IO::Path:D :$out!, IO() :$layouts!, :%site, :%sections,
  Bool :$clean-urls!, Bool :$debug!, Int :$page-size!, Str :$home-section!,
  Writer :$writer!, Str :$framework!,
  --> Array
) {
  my sub page-size-for(Str $section) {
    (%sections{$section}<page-size> // $page-size).Int;
  }

  my %by-section;
  %by-section{ .<section> }.push($_) for @pages;

  my @written;

  my sub sorted-of(Str $section) {
    %by-section{$section}.sort({
      (date-key($^b) <=> date-key($^a)) || ($^a<post>.slug leg $^b<post>.slug)
    });
  }

  for %by-section.keys.grep(*.chars).sort -> $section {
    @written.append: write-section-listing(
      $section, sorted-of($section),
      :$out, :$layouts, :%site, :@nav, :$clean-urls, :$debug, :$writer, :$framework,
      page-size => page-size-for($section),
    );
  }

  if $home-section.chars && (%by-section{$home-section}:exists) {
    @written.append: write-section-listing(
      $home-section, sorted-of($home-section),
      :at-root, :$out, :$layouts, :%site, :@nav, :$clean-urls, :$debug, :$writer, :$framework,
      page-size => page-size-for($home-section),
    );
  }

  @written;
}

sub build-tags(
  :@pages, :@nav, IO::Path:D :$out!, IO() :$layouts!, :%site,
  Bool :$clean-urls!, Bool :$debug!, Writer :$writer!, Str :$framework!,
  --> Array
) {
  my %tag-posts;
  for @pages -> $page {
    %tag-posts{$_}.push($page) for $page<post>.tags;
  }

  my @written;
  return @written unless %tag-posts;

  for %tag-posts.keys.sort -> $tag {
    my @sorted = %tag-posts{$tag}.sort({
      (date-key($^b) <=> date-key($^a)) || ($^a<post>.slug leg $^b<post>.slug)
    });

    my $slug     = Blogin::Slug::slugify($tag);
    my $out-file = $clean-urls ?? $out.add("tags/$slug.html") !! $out.add("tags/$slug").add('index.html');

    my @entries = @sorted.map({
      %( title => .<post>.title, url => .<url>, date => (.<post>.date.defined ?? .<post>.date.Str !! '') )
    });

    my $html = Blogin::Layout::render-listing(
      :$layouts, :%site, :@nav, :@entries, templates => ['tag', 'index'], :$debug, :$framework,
    );

    $writer.write($out-file, $html);
    @written.push($out-file);
  }

  my @tag-entries = %tag-posts.keys.sort.map(-> $tag {
    my $slug = Blogin::Slug::slugify($tag);
    %(
      title => "$tag ({ %tag-posts{$tag}.elems })",
      url   => ($clean-urls ?? "/tags/$slug" !! "/tags/$slug/"),
      date  => '',
    )
  });

  my $index-file = $clean-urls ?? $out.add('tags.html') !! $out.add('tags').add('index.html');

  my $html = Blogin::Layout::render-listing(
    :$layouts, :%site, :@nav, entries => @tag-entries, templates => ['index'], :$debug, :$framework,
  );

  $writer.write($index-file, $html);
  @written.push($index-file);

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

sub feed-entries(@sorted, Str $base) {
  @sorted.map({
    %(
      title => .<post>.title,
      url   => $base ~ .<url>,
      date  => (.<post>.date.defined ?? .<post>.date.Str !! ''),
    )
  });
}

sub newest-date(@sorted) {
  return '' unless @sorted;
  @sorted[0]<post>.date.defined ?? @sorted[0]<post>.date.Str !! '';
}

sub build-feeds(
  :@pages, :@page-files, IO::Path:D :$out!, :%site, Bool :$clean-urls!, Writer :$writer!,
  --> Array
) {
  my @written;

  my $base  = %site<base-url> // '';
  my $title = %site<title>    // '';

  my sub newest-first(@list) {
    @list.sort({ (date-key($^b) <=> date-key($^a)) || ($^a<post>.slug leg $^b<post>.slug) });
  }

  my @all = newest-first(@pages);

  my $site-feed = Blogin::Feed::atom(
    :$title,
    site-url => "$base/",
    feed-url => "$base/feed.xml",
    updated  => newest-date(@all),
    entries  => feed-entries(@all, $base),
  );

  $writer.write($out.add('feed.xml'), $site-feed);
  @written.push($out.add('feed.xml'));

  my %by-section;
  %by-section{ .<section> }.push($_) for @pages;

  for %by-section.keys.grep(*.chars).sort -> $section {
    my @sorted = newest-first(%by-section{$section});

    my $feed = Blogin::Feed::atom(
      title    => "$title: $section",
      site-url => "$base/$section",
      feed-url => "$base/$section/feed.xml",
      updated  => newest-date(@sorted),
      entries  => feed-entries(@sorted, $base),
    );

    my $file = $out.add($section).add('feed.xml');
    $writer.write($file, $feed);
    @written.push($file);
  }

  my @locs = @page-files.map({ $base ~ file-to-url($_, $out, $clean-urls) }).sort;
  my $sitemap = Blogin::Feed::sitemap(locs => @locs);

  $writer.write($out.add('sitemap.xml'), $sitemap);
  @written.push($out.add('sitemap.xml'));

  @written;
}

our sub build(
  IO()  :$content!,
  IO()  :$out!,
  IO()  :$layouts = $content.parent.add('layouts'),
  IO()  :$static  = $content.parent.add('static'),
        :%site = %(),
  Bool  :$drafts = False,
  Int   :$jobs = ($*KERNEL.cpu-cores // 1),
  Bool  :$clean-urls = True,
  Str   :$framework = 'none',
  Bool  :$debug = False,
  Int   :$page-size = 10,
  Str   :$home-section = '',
        :%sections = %(),
  Bool  :$search = True,
  Int   :$search-text-length = 2000,
  Int   :$search-cap = 10,
  Bool  :$highlight = False,
  Bool  :$force = False,
  --> BuildResult
) {
  my @nav = Blogin::Nav::build-tree($content, :%sections, :$clean-urls);

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

  my @ordered = @pages.sort({ -.<post>.body.chars });

  my $haml-lock = Lock.new;

  my @written = @ordered.hyper(:degree($jobs max 1), :batch(1)).map(-> $page {
    my $parts = Blogin::Layout::render-parts(
      post      => $page<post>,
      framework => $framework,
      highlight => $highlight,
    );

    $page<text> = $parts<text>;

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
      );
    });

    $writer.write($page<out-file>, $html);
    $page<out-file>;
  }).List;

  my @listings = build-listings(
    :@pages, :@nav, :$out, :$layouts, :%site, :%sections, :$clean-urls, :$debug,
    :$page-size, :$home-section, :$writer, :$framework,
  );

  @listings.append: build-tags(
    :@pages, :@nav, :$out, :$layouts, :%site, :$clean-urls, :$debug, :$writer, :$framework,
  );

  if @pages.elems {
    my @page-files = [ |@written, |@listings ];

    @listings.append: build-feeds(
      :@pages, :@page-files, :$out, :%site, :$clean-urls, :$writer,
    );

    if $search {
      my $index-file = $out.add('search-index.json');
      $writer.write($index-file, Blogin::Search::index-json(@pages, text-length => $search-text-length));
      @listings.push: $index-file;

      my $js-file = $out.add('search.js');
      $writer.write($js-file, Blogin::Search::search-js(cap => $search-cap));
      @listings.push: $js-file;
    }
  }

  copy-static($static, $out, $writer) if $static.d;

  $writer.prune($out);

  BuildResult.new(
    written   => @written,
    rendered  => @pages,
    listings  => @listings,
    write-log => $writer.written,
  );
}
