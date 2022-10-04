#![no_std]

extern crate alloc;

use alloc::format;
use jsmn::*;
use warduino::print;
use warduino::bench_start;
use warduino::bench_finish;

fn main() {
    bench_start();
    for _ in 0..50 {
        bench();
    }
    let runtime = bench_finish();
    print(format!("sensor: iterations=50 runtime: {}us\n", runtime).as_bytes());
}

fn bench() {
    let sensor = br#"
    {
       "sensor": {
            "value": 50.75,
            "last-measurement": "2019-04-12T10:22:51"
        }
    }
    "#;

    let tokens = &mut [Token::default(); 8];
    let p = &mut JsonParser::new();
    let r = p.parse(sensor, tokens).unwrap();

    assert!(r > 0);
    assert_eq!(&sensor[tokens[4].start.unwrap()..tokens[4].end.unwrap()], b"50.75");
}
