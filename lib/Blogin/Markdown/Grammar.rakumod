use v6.d;

unit grammar Blogin::Markdown::Grammar;

token TOP { <inline>* }

token inline {
  || <esc>
  || <code>
  || <image>
  || <footref>
  || <link>
  || <reflink>
  || <autolink>
  || <strong>
  || <emph>
  || <strike>
  || <hardbreak>
  || <softbreak>
  || <text>
}

token footref { '[^' $<label>=(<[\w-]>+) ']' }

token reflink {
  '[' $<text>=(<-[\]]>+) ']' '[' $<label>=(<-[\]]>*) ']'
}

token esc { '\\' $<char>=(<[ \\ \` \* _ \~ \[ \] \( \) \# \+ \- \. \! \< \> ]>) }

token code { ('`'+) $<body>=[ .*? ] $0 }

token image {
  '!' '[' $<alt>=(<-[\]]>*) ']'
  '(' \h* $<url>=(<-[)\s]>*) [ \h+ '"' $<title>=(<-["]>*) '"' ]? \h* ')'
  <attr-block>?
}

token link {
  '[' $<text>=(<-[\]]>*) ']'
  '(' \h* $<url>=(<-[)\s]>*) [ \h+ '"' $<title>=(<-["]>*) '"' ]? \h* ')'
  <attr-block>?
}

token attr-block { '{' \h* <attr>+ %% \h+ \h* '}' }

token attr {
  || '.' $<class>=(<[\w-]>+)
  || '#' $<id>=(<[\w-]>+)
  || $<key>=(<[\w-]>+) '=' [ '"' $<val>=(<-["]>*) '"' | $<val>=(<-[}\s]>+) ]
}

token autolink {
  '<' $<url>=( \w+ '://' <-[>\s]>+ | <-[>\s@]>+ '@' <-[>\s]>+ ) '>'
}

token strong {
  || '**' [ <!before '**'> <inline> ]+ '**'
  || '__' [ <!before '__'> <inline> ]+ '__'
}

token emph {
  || '*' [ <!before '*'> <inline> ]+ '*'
  || '_' [ <!before '_'> <inline> ]+ '_'
}

token strike { '~~' [ <!before '~~'> <inline> ]+ '~~' }

token hardbreak { [ ' ' ** 2..* | '\\' ] \n }

token softbreak { \n }

token text {
  || [ <!before <hardbreak>> <-[ \\ \` \* _ \~ \[ \! \< \n ]> ]+
  || .
}
