---
title: Meta-programming a dynamic method inside a dynamic class in Raku and Ruby
date: 2023-08-02
slug: meta-programming-a-dynamic-method-inside-a-dynamic-class-in-raku-and-ruby
description: I wanted to learn to create a dynamically-named method inside a dynamically-named class in Raku. This is what I ended up with: #!/usr/bin/env raku my $class-name = "MyDynamicClass"; my $method-name =...
tags: [meta, raku, ruby]
archives: [2023-08]
---
I wanted to learn to create a dynamically-named method inside a dynamically-named class in Raku. This is what I ended up with:

```perl
#!/usr/bin/env raku

my $class-name = "MyDynamicClass";
my $method-name = "dynamic-method";

my $class = Metamodel::ClassHOW.new_type(:name($class-name));

$class.^add_method($method-name, method ($arg) {
  say "Dynamic method '$method-name' called with arg '$arg'";
});

my $obj = $class.^compose.new;
$obj."$method-name"('hello');
```

The equivalent Ruby code would be:

```ruby
#!/usr/bin/env ruby

klass_name = 'MyDynamicClass'
method_name = 'dynamic_method'

klass = Object.const_set(klass_name, Class.new)

klass.class_eval do
  define_method(method_name) do |arg|
    puts "Dynamic method '#{method_name}' called with arg '#{arg}'"
  end
end

obj = klass.new
obj.send(method_name, 'hello')
```
