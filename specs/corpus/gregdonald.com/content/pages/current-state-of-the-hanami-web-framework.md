---
title: Current State of the Hanami Web Framework
date: 2022-11-27
slug: current-state-of-the-hanami-web-framework
description: About two days ago I built the “bookshelf” app described in the Hanami guide. Hanami is currently missing any sort of “model” layer, as well as any sort of “view” layer, as you might expect to be...
tags: [hanami, mvc, web-framework]
archives: [2022-11]
---
About two days ago I built the “bookshelf” app described in the Hanami guide. [Hanami](https://hanamirb.org/) is currently missing any sort of “model” layer, as well as any sort of “view” layer, as you might expect to be present from using [Ruby on Rails](https://rubyonrails.org/) or other MVC [web frameworks](https://en.wikipedia.org/wiki/Web_framework).

I asked about these missing pieces on the Hanami forum and it was explained to me the missing pieces [will appear](https://discourse.hanamirb.org/t/what-about-models/748) in the [2.1 release](https://discourse.hanamirb.org/t/view-layer-what-to-use-for-forms-and-server-side-html-rendering/747), in Q1 2023, and in the meantime to please go look at two other projects, to learn by watching them being built. I looked at the two projects but saw no model or view layer code.

And as nice as I can think to say this, I don't understand how they got to a place where they decided a 2.0 version number was appropriate, given these basic components are not present, not even in some very basic form. Hanami presently seems more like a 0.5 version framework to me.

The Hanami homepage currently contains the words "full-featured". I have no idea why they would say that. "2.0" implies to me a second major version of an already completely fleshed-out framework. ¯\_(ツ)_/¯

I stopped looking at it for now. I don't want to invent all the missing pieces only to have my code become obsolete in 6 months.
