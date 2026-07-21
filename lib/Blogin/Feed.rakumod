use v6.d;

use JSON::Fast;

unit module Blogin::Feed;

sub xml-escape(Str $text --> Str) {
  $text.trans(
    [ '&', '<', '>', '"', "'" ] =>
    [ '&amp;', '&lt;', '&gt;', '&quot;', '&apos;' ],
  );
}

sub atom-time(Str $date --> Str) {
  $date.chars ?? "{ $date }T00:00:00Z" !! '1970-01-01T00:00:00Z';
}

sub rss-time(Str $date --> Str) {
  return 'Thu, 01 Jan 1970 00:00:00 +0000' unless $date ~~ /^ (\d ** 4) '-' (\d\d) '-' (\d\d) $/;

  my $day = try Date.new(+$0, +$1, +$2);

  return 'Thu, 01 Jan 1970 00:00:00 +0000' without $day;

  my @weekdays = <Mon Tue Wed Thu Fri Sat Sun>;
  my @months   = <Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec>;

  my $weekday = @weekdays[$day.day-of-week - 1];
  my $month   = @months[$day.month - 1];
  my $mday    = $day.day.fmt('%02d');

  "$weekday, $mday $month { $day.year } 00:00:00 +0000";
}

our sub atom(
  Str  :$title!,
  Str  :$site-url = '',
  Str  :$feed-url = '',
  Str  :$updated = '',
       :@entries,
  --> Str
) is export {
  my @lines;

  @lines.push('<?xml version="1.0" encoding="utf-8"?>');
  @lines.push('<feed xmlns="http://www.w3.org/2005/Atom">');
  @lines.push("  <title>{ xml-escape($title) }</title>");
  @lines.push("  <id>{ xml-escape($site-url) }</id>");
  @lines.push("  <link href=\"{ xml-escape($site-url) }\"/>");
  @lines.push("  <link href=\"{ xml-escape($feed-url) }\" rel=\"self\"/>");
  @lines.push("  <updated>{ atom-time($updated) }</updated>");

  for @entries -> $entry {
    @lines.push('  <entry>');
    @lines.push("    <title>{ xml-escape($entry<title>) }</title>");
    @lines.push("    <id>{ xml-escape($entry<url>) }</id>");
    @lines.push("    <link href=\"{ xml-escape($entry<url>) }\"/>");
    @lines.push("    <updated>{ atom-time($entry<date>) }</updated>");

    with $entry<summary> -> $summary {
      @lines.push("    <summary>{ xml-escape($summary) }</summary>") if $summary.chars;
    }

    @lines.push('  </entry>');
  }

  @lines.push('</feed>');

  @lines.join("\n") ~ "\n";
}

our sub rss(
  Str  :$title!,
  Str  :$site-url = '',
  Str  :$feed-url = '',
  Str  :$updated = '',
       :@entries,
  --> Str
) is export {
  my @lines;

  @lines.push('<?xml version="1.0" encoding="utf-8"?>');
  @lines.push('<rss version="2.0">');
  @lines.push('  <channel>');
  @lines.push("    <title>{ xml-escape($title) }</title>");
  @lines.push("    <link>{ xml-escape($site-url) }</link>");
  @lines.push("    <description>{ xml-escape($title) }</description>");
  @lines.push("    <lastBuildDate>{ rss-time($updated) }</lastBuildDate>") if $updated.chars;

  for @entries -> $entry {
    my $summary = ($entry<summary> // '').chars ?? $entry<summary> !! $entry<title>;

    @lines.push('    <item>');
    @lines.push("      <title>{ xml-escape($entry<title>) }</title>");
    @lines.push("      <link>{ xml-escape($entry<url>) }</link>");
    @lines.push("      <guid isPermaLink=\"true\">{ xml-escape($entry<url>) }</guid>");
    @lines.push("      <pubDate>{ rss-time($entry<date>) }</pubDate>");
    @lines.push("      <description>{ xml-escape($summary) }</description>");
    @lines.push('    </item>');
  }

  @lines.push('  </channel>');
  @lines.push('</rss>');

  @lines.join("\n") ~ "\n";
}

our sub json-feed(
  Str  :$title!,
  Str  :$site-url = '',
  Str  :$feed-url = '',
  Str  :$updated = '',
       :@entries,
  --> Str
) is export {
  my @items = @entries.map(-> $entry {
    my $summary = ($entry<summary> // '').chars ?? $entry<summary> !! $entry<title>;

    %(
      id             => $entry<url>,
      url            => $entry<url>,
      title          => $entry<title>,
      date_published => atom-time($entry<date>),
      content_text   => $summary,
    )
  });

  my %feed = (
    version       => 'https://jsonfeed.org/version/1.1',
    title         => $title,
    home_page_url => $site-url,
    feed_url      => $feed-url,
    items         => @items.Array,
  );

  to-json(%feed) ~ "\n";
}

our sub sitemap(:@locs --> Str) is export {
  my @lines;

  @lines.push('<?xml version="1.0" encoding="utf-8"?>');
  @lines.push('<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">');

  for @locs -> $loc {
    @lines.push("  <url><loc>{ xml-escape($loc) }</loc></url>");
  }

  @lines.push('</urlset>');

  @lines.join("\n") ~ "\n";
}
