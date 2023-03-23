(module
 (type $i32tovoid                   (func (param i32)   (result)))
 (type $i32toi32                    (func (param i32) (result i32)))
 (type $i32tof32                    (func (param i32) (result f32)))
 (type $f32tovoid                   (func (param f32) (result)))
 (type $voidtovoid                  (func (param) (result)))
 (type $voidtof32                   (func (param) (result f32)))
 (type $voidtoi32                   (func (param) (result i32)))
 (type $callbackType                (func (param i32 i32 i32 i32 i32) (result)))
 (type $subscribeType               (func (param i32 i32 i32) (result)))

 (import "env" "chip_delay"          (func $delay       (type $i32tovoid)))
 (import "env" "print_int"           (func $print       (type $f32tovoid)))
 (import "env" "req_temp"            (func $reqTemp     (type $i32tof32)))
 (import "env" "subscribe_interrupt" (func $subscribeInterrupt (type $subscribeType)))


 (export "main"                     (func $main))

 (global $connectedStatus (mut i32) (i32.const 1))
 (global $sensorA i32 (i32.const 3030))
 (global $sensorB i32 (i32.const 3031))
 (global $connected (mut f32) (f32.const 0))

 (global $wifiBtn i32 (i32.const 37))

 (memory $memory 1)
 (table 1 funcref)
 (elem $e0 (i32.const 0) $disconnectSensors)

(func $isConnected (type $i32toi32)
    global.get $connectedStatus)

(func $disconnectSensors (type $callbackType)
    i32.const 0
    (global.set $connectedStatus))

(func $initCallback (type $voidtovoid)
    global.get $wifiBtn ;; pin
    i32.const 0 ;; Table idx of $disconnectSensors
    i32.const 1 ;; trigger callback on CHANGE
    call $subscribeInterrupt)

(func $inc_connected (type $voidtovoid)
    (f32.add
      (global.get $connected)
      (f32.const 1))
    (global.set $connected))

(func $getTemp (type $i32tof32)
    (local.get 0)
    (call $isConnected)
    (if (result f32)
        (then 
         (call $inc_connected)
         (local.get 0)
         (call $reqTemp))
        (else
          (f32.const 0.0))))

(func $avgTemp (type $voidtof32)
    (global.get $sensorA)
    (call $getTemp)
    (global.get $sensorB)
    (call $getTemp)
    f32.add
    (global.get $connected)
    f32.div)


 (func $main (type $voidtovoid)
    (call $initCallback)
    (loop 
       (global.set $connected (f32.const 0))
       (call $avgTemp)
       (call $print)
       ;;sleep 3sec
       (i32.const 3000)
       (call $delay)
       (br 0))))