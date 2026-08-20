# Console

```Rust
let debug = WarduinoSession::connect(config)?;
let adapter = WarduinoAdapter::new(debug);
let app = Console::new(adapter);
```


![](assets/mock.png)
