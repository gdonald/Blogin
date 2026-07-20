use v6.d;

unit module Blogin::Markdown::Node;

role Node is export {}

class Document does Node is export {
  has @.children;
}

class Paragraph does Node is export {
  has @.children;
}

class Heading does Node is export {
  has Int $.level;
  has @.children;
}

class ThematicBreak does Node is export {}

class CodeBlock does Node is export {
  has Str $.info = '';
  has Str $.text;
}

class BlockQuote does Node is export {
  has @.children;
}

class List does Node is export {
  has Bool $.ordered;
  has Int  $.start = 1;
  has Bool $.tight = True;
  has @.items;
}

class ListItem does Node is export {
  has Bool $.task    = False;
  has Bool $.checked = False;
  has @.children;
}

class Table does Node is export {
  has @.aligns;
  has @.header;
  has @.rows;
}

class Text does Node is export {
  has Str $.text;
}

class Emphasis does Node is export {
  has @.children;
}

class Strong does Node is export {
  has @.children;
}

class Strikethrough does Node is export {
  has @.children;
}

class CodeSpan does Node is export {
  has Str $.text;
}

class Link does Node is export {
  has Str $.url;
  has Str $.title = '';
  has %.attrs;
  has @.children;
}

class Image does Node is export {
  has Str $.url;
  has Str $.title = '';
  has Str $.alt;
  has %.attrs;
}

class SoftBreak does Node is export {}

class LineBreak does Node is export {}
