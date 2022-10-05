#![no_std]

extern crate alloc;

use alloc::format;
use jsmn::*;
use warduino::print;
use warduino::bench_start;
use warduino::bench_finish;

fn main() {
    bench_start();
    for _ in 0..1 {
        bench();
    }
    let runtime = bench_finish();
    print(format!("weather: iterations=1 runtime: {}us\n", runtime).as_bytes());
}

fn bench() {
    let forecast = br#"
    {
       "latitude":50.75,
       "longitude":4.25,
       "generationtime_ms":0.3349781036376953,
       "utc_offset_seconds":7200,
       "timezone":"Europe/Berlin",
       "timezone_abbreviation":"CEST",
       "elevation":64,
       "daily_units":{
          "time":"iso8601",
          "temperature_2m_max":"C",
          "temperature_2m_min":"C",
          "sunrise":"iso8601",
          "sunset":"iso8601",
          "rain_sum":"mm",
          "windspeed_10m_max":"km/h",
          "winddirection_10m_dominant":""
       },
       "daily":{
          "time":[
             "2019-04-12"
          ],
          "temperature_2m_max":[
             7.4
          ],
          "temperature_2m_min":[
             2.5
          ],
          "sunrise":[
             "2019-04-12T06:53"
          ],
          "sunset":[
             "2019-04-12T20:33"
          ],
          "rain_sum":[
             0
          ],
          "windspeed_10m_max":[
             16.6
          ],
          "winddirection_10m_dominant":[
             46.295578
          ]
       }
    }
    "#;   
    let tokens = &mut [Token::default(); 8];
    let p = &mut JsonParser::new();
    let r = p.parse(forecast, tokens).unwrap();

    assert!(r > 0);
    assert_eq!(&forecast[tokens[2].start.unwrap()..tokens[2].end.unwrap()], b"50.75");
}
