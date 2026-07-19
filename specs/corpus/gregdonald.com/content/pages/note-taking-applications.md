---
title: Note Taking Applications
date: 2024-06-14
slug: note-taking-applications
description: As I delve deeper into the field of cybersecurity and penetration testing I’ve discovered that if you search Google or Youtube for “penetration testing note-taking applications” you’ll see there are...
tags: [notes]
archives: [2024-06]
---
As I delve deeper into the field of cybersecurity and penetration testing I’ve discovered that if you search Google or Youtube for “penetration testing note-taking applications” you’ll see there are a surprising number of opinions out there on which application is best. I agree with them, that a reliable note-taking app is indispensable for this sort of work, and so I’ve been on a quest to find which application actually works best for me.

So far I’ve tried [Obsidian](https://obsidian.md/), [CherryTree](https://www.giuspen.net/cherrytree/), [Writerside](https://www.jetbrains.com/writerside/), and [Emacs](https://www.gnu.org/software/emacs/) [org-mode](https://orgmode.org/) with [org-roam](https://www.orgroam.com/). I liked all of them for various reasons but in the end I like org-mode the best. There’s no vendor lock-in and I have no doubt Emacs will be around long after I’m not.

I found the idea with CherryTree storing everything in a local sqlite database to be rather odd. The file system on my computer already provides all the same functionality without using another layer of storage on top of it. Making files and folders will produce the same sort of hierarchy. Seems overkill for what I need. At one point it hit me that I could not use `cat` to view a CherryTree entry in my terminal. :(

The output from Writeside is very nice and the effort required to add new content is very low. The app seemed very easy to use. But right away I can see that if I ever stop using the Jetbrains Toolbox then all my Writerside work will be very hard to move to another application. I was hoping to find some sort of build script in the output, something I could use in the future to regenerate my content without Writerside, but nothing was present. This scenario screams vendor lock-in.

I can’t actually say Obsidian didn’t make the cut because I may end up using it down the road. I see no vendor lock-in as the files are just Markdown that I could move to another application, even to Emacs org-mode since the Markdown and org file formats are very similar.

I’m already a decent Emacs user with a [solid config](https://github.com/gdonald/emacs-config), so I’m going to work on ramping up my org-mode and org-roam skills for now and see how far that takes me.
