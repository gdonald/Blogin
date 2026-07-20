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
}

sub remove-tree(IO::Path:D $dir) {
  return unless $dir.e;

  for $dir.dir -> $entry {
    $entry.d ?? remove-tree($entry) !! $entry.unlink;
  }

  $dir.rmdir;
}

sub copy-tree(IO::Path:D $from, IO::Path:D $to) {
  for $from.dir.sort(*.basename) -> $entry {
    my $dest = $to.add($entry.basename);

    if $entry.d {
      $dest.mkdir;
      copy-tree($entry, $dest);
    }
    else {
      $entry.copy($dest);
    }
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
  Bool :$debug!, Int :$page-size!,
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
      :$prev-url, :$next-url, :$debug,
    );

    $out-file.parent.mkdir;
    $out-file.spurt($html);
    @written.push($out-file);
  }

  @written;
}

sub build-listings(
  :@pages, :@nav, IO::Path:D :$out!, IO() :$layouts!, :%site, :%sections,
  Bool :$clean-urls!, Bool :$debug!, Int :$page-size!, Str :$home-section!,
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
      :$out, :$layouts, :%site, :@nav, :$clean-urls, :$debug,
      page-size => page-size-for($section),
    );
  }

  if $home-section.chars && (%by-section{$home-section}:exists) {
    @written.append: write-section-listing(
      $home-section, sorted-of($home-section),
      :at-root, :$out, :$layouts, :%site, :@nav, :$clean-urls, :$debug,
      page-size => page-size-for($home-section),
    );
  }

  @written;
}

sub build-tags(
  :@pages, :@nav, IO::Path:D :$out!, IO() :$layouts!, :%site,
  Bool :$clean-urls!, Bool :$debug!,
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
      :$layouts, :%site, :@nav, :@entries, templates => ['tag', 'index'], :$debug,
    );

    $out-file.parent.mkdir;
    $out-file.spurt($html);
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
    :$layouts, :%site, :@nav, entries => @tag-entries, templates => ['index'], :$debug,
  );

  $index-file.parent.mkdir;
  $index-file.spurt($html);
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
  :@pages, :@page-files, IO::Path:D :$out!, :%site, Bool :$clean-urls!,
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

  $out.add('feed.xml').spurt($site-feed);
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
    $file.parent.mkdir;
    $file.spurt($feed);
    @written.push($file);
  }

  my @locs = @page-files.map({ $base ~ file-to-url($_, $out, $clean-urls) }).sort;
  my $sitemap = Blogin::Feed::sitemap(locs => @locs);

  $out.add('sitemap.xml').spurt($sitemap);
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
  --> BuildResult
) {
  my @nav = Blogin::Nav::build-tree($content, :%sections, :$clean-urls);
  remove-tree($out);
  $out.mkdir;

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

  .<out-file>.parent.mkdir for @pages;

  my @ordered = @pages.sort({ -.<post>.body.chars });

  my $haml-lock = Lock.new;

  my @written = @ordered.hyper(:degree($jobs max 1), :batch(1)).map(-> $page {
    my $parts = Blogin::Layout::render-parts(
      post      => $page<post>,
      framework => $framework,
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
      );
    });

    $page<out-file>.spurt($html);
    $page<out-file>;
  }).List;

  my @listings = build-listings(
    :@pages, :@nav, :$out, :$layouts, :%site, :%sections, :$clean-urls, :$debug,
    :$page-size, :$home-section,
  );

  @listings.append: build-tags(
    :@pages, :@nav, :$out, :$layouts, :%site, :$clean-urls, :$debug,
  );

  if @pages.elems {
    my @page-files = [ |@written, |@listings ];

    @listings.append: build-feeds(
      :@pages, :@page-files, :$out, :%site, :$clean-urls,
    );

    if $search {
      @listings.push: Blogin::Search::write-index(@pages, :$out, text-length => $search-text-length);
      @listings.push: Blogin::Search::write-js(:$out, cap => $search-cap);
    }
  }

  copy-tree($static, $out) if $static.d;

  BuildResult.new(written => @written, rendered => @pages, listings => @listings);
}
