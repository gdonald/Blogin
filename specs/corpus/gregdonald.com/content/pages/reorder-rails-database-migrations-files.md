---
title: Reorder Rails database migrations files
date: 2023-11-26
slug: reorder-rails-database-migrations-files
description: On new Rails apps, that are not in production yet, I sometimes like to reorder my database migrations. I wrote a small Ruby script to automate things.
tags: [database, rails]
archives: [2023-11]
---
On new Rails apps, that are not in production yet, I sometimes like to reorder my database migrations. I wrote a small Ruby script to automate things.

```ruby
#!/usr/bin/env ruby

x = STEP = 10

Dir.entries('.').sort.each do |file|
  next unless file =~ /^[\d]+.*rb$/

  File.rename(file, format('%05<x>d_%<suffix>s', x:, suffix: file.split('_')[1..].join('_')))
  x += STEP
end
```

I use this Ruby script to rename files in the current directory (`.`, which stands for the current directory) according to a specific pattern. It is aimed at Ruby files with names starting with numbers, specifically Rails database migration files. Let me break down the code a bit:

`#!/usr/bin/env ruby`: This is a shebang line used in Unix-based systems to tell the system to execute this script using a Ruby interpreter.

`x = STEP = 10`: This line initializes two variables, `x` and `STEP`, and sets them both to `10`. `x` will be used to generate new filenames, and `STEP` is the increment value for `x`.

`Dir.entries('.')`: This gets all entries (files and directories) in the current directory.

`.sort.each do |file|`: This iterates over the sorted list of directory entries.

`next unless file =~ /^[\d]+.*rb$/`: This line is a guard clause. It skips to the next iteration unless the filename matches the given regular expression. The regex `^[\d]+.*rb$` matches any string that starts (`^`) with one or more digits (`[\d]+`), followed by any characters (`.*`), and ends with `rb` (indicating a Ruby file).

`File.rename(...)`: This renames the file. The new name is generated using `format`, which is a method to create formatted strings.

`format('%05<x>d_%<suffix>s', x:, suffix: ...`: The format string `'%05<x>d_%<suffix>s'` creates a new filename with two parts:

- `%05<x>d` formats `x` as a zero-padded decimal integer with a width of 5 characters.
- `%<suffix>s` is a placeholder for the suffix of the filename, which is everything after the first underscore in the original filename.

`file.split('_')[1..].join('_')`: This splits the original filename at underscores, takes all parts except the first one (which is the numeric prefix), and then joins these parts back together with underscores.

`x += STEP`: Increments `x` by `STEP` (which is 10) for the next iteration.

This script renames all Ruby files in the current directory that start with a number. It renumbers these files starting from 10, incrementing by 10 for each file, and retains the rest of the original filename after the first underscore. For example, if there's a file named `123_example.rb`, it would be renamed to `00010_example.rb`. The next file would then start from `00020`, and so on.

After I run this script I reset my database:

```bash
> be rails db:drop &&
  be rails db:create &&
  be rails db:migrate &&
  be rails db:migrate RAILS_ENV=test &&
  be rails db:seed
```

The `be` is a shell alias `alias be='bundle exec'`
