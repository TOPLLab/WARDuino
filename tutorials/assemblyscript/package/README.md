<div align="center">

<img width="100" height="100" src="media/assemblyscript-logo.svg" alt="AssemblyScript logo">
<img width="100" height="100" src="media/warduino-logo.png" alt="WARDuino logo">

<h1>as-warduino</h1>
<p><strong>WARDuino primitives library for AssemblyScript</strong></p>

</div>

## Features

+ Access WARDuino primitives from AssemblyScript
+ Use enums and classes for arguments of primitives

## Installation

This package requires **WARDuino 0.4.x** and **AssemblyScript 0.17.14+**.

```bash
npm i as-warduino
```

## Example

Imports in AssemblyScript changed starting from version `0.20.x`.
Before you could write:

```ts
import { delay } from "as-warduino";
```

Starting from `0.20.x` you need to import the library in the following way:

```ts
import { delay } from "as-warduino/assembly";
```

A simple blink program looks as follows:

```ts
import {pinMode, PinMode, PinVoltage, digitalWrite, delay} from "as-warduino/assembly";

export function main(): void {
    let led = 26;
    pinMode(led, PinMode.OUTPUT);

    let pause: u32 = 1000;
    while (true) {
        digitalWrite(led, PinVoltage.HIGH);
        delay(pause);
        digitalWrite(led, PinVoltage.LOW);
        delay(pause);
    }
}
```

