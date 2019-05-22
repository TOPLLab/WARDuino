(; 
/* 
 __      __                  .___    .__               
/  \    /  \_____ _______  __| _/_ __|__| ____   ____  
\   \/\/   /\__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
 \        /  / __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
  \__/\  /  (____  /__|  \____ |____/|__|___|  /\____/ 
       \/        \/           \/             \/        
       
WARDuino (c) by Christophe Scholliers & Robbert Gurdeep Singh 

WARDuino is licensed under a
Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You should have received a copy of the license along with this
work. If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
;)

(module 
 (; Arduino imports ;)

 (import "esp8266" "chip_pin_mode"      (func $pin_mode      (type $1))) 
 (import "esp8266" "chip_digital_write" (func $digital_write (type $2))) 
 (import "esp8266" "chip_delay"         (func $delay         (type $3))) 
 (import "esp8266" "blink"              (func $blink_a       (type $5))) 

 (; Type declarations ;)
 (type $1 (func (param i32) (param i32) (result)))
 (type $2 (func (param i32) (param i32) (result)))
 (type $3 (func (param i32)             (result)))
 (type $4 (func (param)                 (result)))
 (type $5 (func (param i32)             (result)))

 (; Define one function ;)
 (export "main" (func $blink_arduino))
 (; The wait function ;)
 (func $wait (type $4)
    (;  Delay time   ;)
    (i32.const 1000) 
    (call $delay)
 )
 (; The blink function ;)
 (func $blink_arduino (type $4) 
   (;  LED    ;)
    (i32.const 16)
    (; OUTPUT ;)
    (i32.const 1) 
    (call $pin_mode)


  (loop
    (;  LED    ;)
    (i32.const 16)
    (; HIGH INVERTED ;)
    (i32.const 0)
    (call $digital_write)

    (call $wait)

    (;  LED    ;)
    (i32.const 16)
    (; LOW INVERTED ;)
    (i32.const 1)
    (call $digital_write)


    (call $wait)

    (br 0))

  (call $blink_a (i32.const 5)))
 
)
