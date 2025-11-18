;; WARDuino port of M5stack cardputer Basic Display example
;; https://github.com/m5stack/M5Cardputer/blob/master/examples/Basic/display/display.ino
(module
  ;; Type declarations
  (type $int32->int32->i32->i32->void (func (param i32 i32 i32 i32)))
  (type $int32->i32->i32->i32->i32->void (func (param i32 i32 i32 i32 i32)))
  (type $void->int32 (func (result i32)))
  (type $void->void (func))

  ;; Imports from the WARDuino VM
  (import "env" "display_setup" (func $env.setup (type $void->void)))
  (import "env" "display_width" (func $env.width (type $void->int32)))
  (import "env" "int_random" (func $env.rand (type $void->int32)))
  (import "env" "display_height" (func $env.height (type $void->int32)))
  (import "env" "display_fillrect" (func $env.fillrect (type $int32->i32->i32->i32->i32->void)))
  (import "env" "display_fillcircle" (func $env.fillcircle (type $int32->int32->i32->i32->void)))

  ;; chaos function (public)
  (func $chaos (type $void->void)
    (local $x i32)
    (local $y i32)
    (local $r i32)

    ;; Initialise
    call $env.setup

    ;; Update display in infinite loop
    loop $infinite
        ;;    int x      = rand() % M5Cardputer.Display.width();
        call $env.rand
        call $env.width
        i32.rem_u
        local.set $x
        ;;    int y      = rand() % M5Cardputer.Display.height();
        call $env.rand
        call $env.height
        i32.rem_u
        local.set $y
        ;;    int r      = (M5Cardputer.Display.width() >> 4) + 2;
        call $env.width
        i32.const 4
        i32.shr_s
        i32.const 2
        i32.add
        local.set $r
        ;;    M5Cardputer.Display.fillCircle(x, y, r, c);
        local.get $x
        local.get $y
        local.get $r
        call $env.rand
        call $env.fillcircle
        ;;    int x      = rand() % M5Cardputer.Display.width();
        call $env.rand
        call $env.width
        i32.rem_u
        local.set $x
        ;;    int y      = rand() % M5Cardputer.Display.height();
        call $env.rand
        call $env.height
        i32.rem_u
        local.set $y
        ;;    M5Cardputer.Display.fillRectangle(x, y, w, h, c);
        local.get $x
        local.get $y
        local.get $r
        local.get $r
        call $env.rand
        call $env.fillrect
    br $infinite                   ;; jump back to start of loop
    end)

  ;; Export chaos as function
  (export "main" (func $chaos)))
