---
title: Raspberry Pi LEDs Test
date: 2015-01-20
slug: raspberry-pi-leds-test
description: #!/usr/bin/env python3 import time import RPi.GPIO as GPIO GPIO.setmode( GPIO.BCM ) ENABLE = 1; DISABLE = 0 RED = 23; GREEN = 24; BLUE = 25 RGB = [ RED, GREEN, BLUE ] RGB2 = RGB[::-1]
tags: [raspberry-pi]
archives: [2015-01]
---
{{< youtube id="f_mAmhkxCNQ" >}}

```python
#!/usr/bin/env python3

import time
import RPi.GPIO as GPIO

GPIO.setmode( GPIO.BCM )

ENABLE = 1; DISABLE = 0

RED = 23; GREEN = 24; BLUE  = 25
RGB = [ RED, GREEN, BLUE ]
RGB2 = RGB[::-1]

for led in RGB:
    GPIO.setup( led, GPIO.OUT )

def race( leds,	start, factor, times, forward=True ):
    t = start
    for x in range( 0, times ):
        for led in leds:
            GPIO.output( led, ENABLE )
            time.sleep( t )
            GPIO.output( led, DISABLE )
	if forward: t /= factor
        else: t *= factor

def main():
    for	x in range( 0, 10 ):
        race( RGB, 0.37, 1.06, 400 )
        race( RGB2, 0.00000023, 1.035, 400, False )

main()

try:
    GPIO.cleanup()
finally:
    pass
```
