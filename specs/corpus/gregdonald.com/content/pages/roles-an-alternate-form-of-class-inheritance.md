---
title: Roles - an alternate form of class inheritance
date: 2019-10-08
slug: roles-an-alternate-form-of-class-inheritance
description: Roles are collections of methods and attributes that can be mixed into classes. A role provides an alternate form of code reuse from inheritance. Roles are mixed in using "is" or "does".
tags: [raku]
archives: [2019-10]
---
Roles are collections of methods and attributes that can be mixed into classes. A role provides an alternate form of code reuse from inheritance. Roles are typically mixed in using "does".

```actionscript
role Seller {
  method sell { say self.^name ~ ' is selling' }
}

role Buyer {
  method buy { say self.^name ~ ' is buying' }
}

class Consumer does Buyer {}
class Producer does Seller {}
class Company does Buyer does Seller {}

my $producer = Producer.new;
my $consumer = Consumer.new;
my $company = Company.new;

$producer.sell;
$consumer.buy;
$company.sell;
$company.buy;
```

Output:

```clike
Producer is selling
Consumer is buying
Company is selling
Company is buying
```
