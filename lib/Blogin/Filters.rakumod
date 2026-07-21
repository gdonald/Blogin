use v6.d;

unit module Blogin::Filters;

our sub truncate(Str $text, Int $length, Str $ellipsis = '…' --> Str) {
  return $text if $text.chars <= $length;

  my $cut = $text.substr(0, $length).trim;

  with $cut.rindex(' ') -> $space {
    $cut = $cut.substr(0, $space);
  }

  $cut.trim ~ $ellipsis;
}

my @MONTHS   = <January February March April May June July August September October November December>;
my @WEEKDAYS = <Monday Tuesday Wednesday Thursday Friday Saturday Sunday>;

# Format a YYYY-MM-DD string with a small strftime subset.
our sub format-date(Str $iso, Str $format = '%Y-%m-%d' --> Str) {
  return $iso unless $iso ~~ / ^ (\d ** 4) '-' (\d\d) '-' (\d\d) $ /;

  my $date = try Date.new(+$0, +$1, +$2);

  return $iso without $date;

  my %fields =
    '%Y' => $date.year.Str,
    '%m' => $date.month.fmt('%02d'),
    '%d' => $date.day.fmt('%02d'),
    '%e' => $date.day.Str,
    '%B' => @MONTHS[$date.month - 1],
    '%b' => @MONTHS[$date.month - 1].substr(0, 3),
    '%A' => @WEEKDAYS[$date.day-of-week - 1],
    '%a' => @WEEKDAYS[$date.day-of-week - 1].substr(0, 3),
    ;

  $format.subst(/ '%' <[YmdeBbAa]> /, { %fields{ ~$_ } // ~$_ }, :g);
}

# Group a list of hashes by the value of a field into ordered { key, items }.
our sub group-by(@items, Str $field --> Array) {
  my %groups;
  my @order;

  for @items -> %item {
    my $key = (%item{$field} // '').Str;

    @order.push($key) unless %groups{$key}:exists;
    %groups{$key}.push(%item);
  }

  @order.sort.reverse.map({ %( key => $_, items => %groups{$_} ) }).Array;
}
