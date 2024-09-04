(module
 (type $i32_i32_=>_i32 (func (param i32 i32) (result i32)))
 (type $i32_i32_=>_none (func (param i32 i32)))
 (type $i32_=>_i32 (func (param i32) (result i32)))
 (type $i32_i32_i32_=>_none (func (param i32 i32 i32)))
 (type $i32_=>_none (func (param i32)))
 (type $i32_i32_i32_=>_i32 (func (param i32 i32 i32) (result i32)))
 (type $none_=>_none (func))
 (type $i32_i32_i32_i32_=>_none (func (param i32 i32 i32 i32)))
 (type $i32_i32_i32_i32_i32_=>_i32 (func (param i32 i32 i32 i32 i32) (result i32)))
 (type $i32_i64_i32_=>_none (func (param i32 i64 i32)))
 (type $i64_i32_=>_i32 (func (param i64 i32) (result i32)))
 (type $i32_i64_i32_i32_=>_none (func (param i32 i64 i32 i32)))
 (import "env" "abort" (func $~lib/builtins/abort (param i32 i32 i32 i32)))
 (import "env" "print_string" (func $~lib/as-warduino/assembly/warduino/_print (param i32 i32)))
 (global $~lib/as-warduino/assembly/index/PinVoltage.LOW i32 (i32.const 0))
 (global $~lib/as-warduino/assembly/index/PinVoltage.HIGH i32 (i32.const 1))
 (global $~lib/as-warduino/assembly/index/InterruptMode.CHANGED i32 (i32.const 1))
 (global $~lib/as-warduino/assembly/index/InterruptMode.FALLING i32 (i32.const 2))
 (global $~lib/as-warduino/assembly/index/InterruptMode.RISING i32 (i32.const 3))
 (global $~lib/as-warduino/assembly/index/PinMode.INPUT i32 (i32.const 0))
 (global $~lib/as-warduino/assembly/index/PinMode.OUTPUT i32 (i32.const 2))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.NoShield i32 (i32.const 255))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.Idle i32 (i32.const 0))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.SsidUnavailable i32 (i32.const 1))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.ScanCompleted i32 (i32.const 2))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.Connected i32 (i32.const 3))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.ConnectFailed i32 (i32.const 4))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.ConnectionLost i32 (i32.const 5))
 (global $~lib/as-warduino/assembly/index/WiFi.Status.Disconnected i32 (i32.const 6))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectionTimeout i32 (i32.const -4))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectionLost i32 (i32.const -3))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectFailed i32 (i32.const -2))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.Disconnected i32 (i32.const -1))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.Connected i32 (i32.const 0))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectBadProtocol i32 (i32.const 1))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectBadClientId i32 (i32.const 2))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectUnavailable i32 (i32.const 3))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectBadCredentials i32 (i32.const 4))
 (global $~lib/as-warduino/assembly/index/MQTT.Status.ConnectUnauthorized i32 (i32.const 5))
 (global $~lib/as-warduino/assembly/index/buffersizeMaximum i32 (i32.const 1000))
 (global $~lib/shared/runtime/Runtime.Stub i32 (i32.const 0))
 (global $~lib/shared/runtime/Runtime.Minimal i32 (i32.const 1))
 (global $~lib/shared/runtime/Runtime.Incremental i32 (i32.const 2))
 (global $~lib/rt/stub/startOffset (mut i32) (i32.const 0))
 (global $~lib/rt/stub/offset (mut i32) (i32.const 0))
 (global $~argumentsLength (mut i32) (i32.const 0))
 (global $~lib/native/ASC_SHRINK_LEVEL i32 (i32.const 0))
 (global $~lib/memory/__heap_base i32 (i32.const 2364))
 (memory $0 1 2)
 (data $0 (i32.const 12) "L\00\00\00\00\00\00\00\00\00\00\00\02\00\00\008\00\00\00A\00S\00C\00I\00I\00 \00T\00a\00b\00l\00e\00 \00~\00 \00C\00h\00a\00r\00a\00c\00t\00e\00r\00 \00M\00a\00p\00\n\00\00\00\00\00")
 (data $1 (i32.const 92) "<\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00(\00\00\00A\00l\00l\00o\00c\00a\00t\00i\00o\00n\00 \00t\00o\00o\00 \00l\00a\00r\00g\00e\00\00\00\00\00")
 (data $2 (i32.const 156) "<\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\1e\00\00\00~\00l\00i\00b\00/\00r\00t\00/\00s\00t\00u\00b\00.\00t\00s\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00")
 (data $3 (i32.const 220) "<\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00$\00\00\00U\00n\00p\00a\00i\00r\00e\00d\00 \00s\00u\00r\00r\00o\00g\00a\00t\00e\00\00\00\00\00\00\00\00\00")
 (data $4 (i32.const 284) ",\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\1c\00\00\00~\00l\00i\00b\00/\00s\00t\00r\00i\00n\00g\00.\00t\00s\00")
 (data $5 (i32.const 332) ",\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\0e\00\00\00,\00 \00d\00e\00c\00:\00 \00\00\00\00\00\00\00\00\00\00\00\00\00\00\00")
 (data $6 (i32.const 380) "|\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00d\00\00\00t\00o\00S\00t\00r\00i\00n\00g\00(\00)\00 \00r\00a\00d\00i\00x\00 \00a\00r\00g\00u\00m\00e\00n\00t\00 \00m\00u\00s\00t\00 \00b\00e\00 \00b\00e\00t\00w\00e\00e\00n\00 \002\00 \00a\00n\00d\00 \003\006\00\00\00\00\00\00\00\00\00")
 (data $7 (i32.const 508) "<\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00&\00\00\00~\00l\00i\00b\00/\00u\00t\00i\00l\00/\00n\00u\00m\00b\00e\00r\00.\00t\00s\00\00\00\00\00\00\00")
 (data $8 (i32.const 572) "\1c\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\02\00\00\000\00\00\00\00\00\00\00\00\00\00\00")
 (data $9 (i32.const 604) "0\000\000\001\000\002\000\003\000\004\000\005\000\006\000\007\000\008\000\009\001\000\001\001\001\002\001\003\001\004\001\005\001\006\001\007\001\008\001\009\002\000\002\001\002\002\002\003\002\004\002\005\002\006\002\007\002\008\002\009\003\000\003\001\003\002\003\003\003\004\003\005\003\006\003\007\003\008\003\009\004\000\004\001\004\002\004\003\004\004\004\005\004\006\004\007\004\008\004\009\005\000\005\001\005\002\005\003\005\004\005\005\005\006\005\007\005\008\005\009\006\000\006\001\006\002\006\003\006\004\006\005\006\006\006\007\006\008\006\009\007\000\007\001\007\002\007\003\007\004\007\005\007\006\007\007\007\008\007\009\008\000\008\001\008\002\008\003\008\004\008\005\008\006\008\007\008\008\008\009\009\000\009\001\009\002\009\003\009\004\009\005\009\006\009\007\009\008\009\009\00")
 (data $10 (i32.const 1004) "\1c\04\00\00\00\00\00\00\00\00\00\00\02\00\00\00\00\04\00\000\000\000\001\000\002\000\003\000\004\000\005\000\006\000\007\000\008\000\009\000\00a\000\00b\000\00c\000\00d\000\00e\000\00f\001\000\001\001\001\002\001\003\001\004\001\005\001\006\001\007\001\008\001\009\001\00a\001\00b\001\00c\001\00d\001\00e\001\00f\002\000\002\001\002\002\002\003\002\004\002\005\002\006\002\007\002\008\002\009\002\00a\002\00b\002\00c\002\00d\002\00e\002\00f\003\000\003\001\003\002\003\003\003\004\003\005\003\006\003\007\003\008\003\009\003\00a\003\00b\003\00c\003\00d\003\00e\003\00f\004\000\004\001\004\002\004\003\004\004\004\005\004\006\004\007\004\008\004\009\004\00a\004\00b\004\00c\004\00d\004\00e\004\00f\005\000\005\001\005\002\005\003\005\004\005\005\005\006\005\007\005\008\005\009\005\00a\005\00b\005\00c\005\00d\005\00e\005\00f\006\000\006\001\006\002\006\003\006\004\006\005\006\006\006\007\006\008\006\009\006\00a\006\00b\006\00c\006\00d\006\00e\006\00f\007\000\007\001\007\002\007\003\007\004\007\005\007\006\007\007\007\008\007\009\007\00a\007\00b\007\00c\007\00d\007\00e\007\00f\008\000\008\001\008\002\008\003\008\004\008\005\008\006\008\007\008\008\008\009\008\00a\008\00b\008\00c\008\00d\008\00e\008\00f\009\000\009\001\009\002\009\003\009\004\009\005\009\006\009\007\009\008\009\009\009\00a\009\00b\009\00c\009\00d\009\00e\009\00f\00a\000\00a\001\00a\002\00a\003\00a\004\00a\005\00a\006\00a\007\00a\008\00a\009\00a\00a\00a\00b\00a\00c\00a\00d\00a\00e\00a\00f\00b\000\00b\001\00b\002\00b\003\00b\004\00b\005\00b\006\00b\007\00b\008\00b\009\00b\00a\00b\00b\00b\00c\00b\00d\00b\00e\00b\00f\00c\000\00c\001\00c\002\00c\003\00c\004\00c\005\00c\006\00c\007\00c\008\00c\009\00c\00a\00c\00b\00c\00c\00c\00d\00c\00e\00c\00f\00d\000\00d\001\00d\002\00d\003\00d\004\00d\005\00d\006\00d\007\00d\008\00d\009\00d\00a\00d\00b\00d\00c\00d\00d\00d\00e\00d\00f\00e\000\00e\001\00e\002\00e\003\00e\004\00e\005\00e\006\00e\007\00e\008\00e\009\00e\00a\00e\00b\00e\00c\00e\00d\00e\00e\00e\00f\00f\000\00f\001\00f\002\00f\003\00f\004\00f\005\00f\006\00f\007\00f\008\00f\009\00f\00a\00f\00b\00f\00c\00f\00d\00f\00e\00f\00f\00\00\00\00\00\00\00\00\00\00\00\00\00")
 (data $11 (i32.const 2060) "\\\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00H\00\00\000\001\002\003\004\005\006\007\008\009\00a\00b\00c\00d\00e\00f\00g\00h\00i\00j\00k\00l\00m\00n\00o\00p\00q\00r\00s\00t\00u\00v\00w\00x\00y\00z\00\00\00\00\00")
 (data $12 (i32.const 2156) "\1c\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00")
 (data $13 (i32.const 2188) ",\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\0e\00\00\00,\00 \00h\00e\00x\00:\00 \00\00\00\00\00\00\00\00\00\00\00\00\00\00\00")
 (data $14 (i32.const 2236) ",\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\0e\00\00\00,\00 \00o\00c\00t\00:\00 \00\00\00\00\00\00\00\00\00\00\00\00\00\00\00")
 (data $15 (i32.const 2284) ",\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\0e\00\00\00,\00 \00b\00i\00n\00:\00 \00\00\00\00\00\00\00\00\00\00\00\00\00\00\00")
 (data $16 (i32.const 2332) "\1c\00\00\00\00\00\00\00\00\00\00\00\02\00\00\00\02\00\00\00\n\00\00\00\00\00\00\00\00\00\00\00")
 (table $0 1 funcref)
 (elem $0 (i32.const 1))
 (export "main" (func $main/ascii/main))
 (export "memory" (memory $0))
 (export "table" (table $0))
 (start $~start)
 (func $~lib/rt/common/OBJECT#get:rtSize (param $this i32) (result i32)
  (i32.load offset=16
   (local.get $this)
  )
 )
 (func $~lib/string/String.UTF8.byteLength (param $str i32) (param $nullTerminated i32) (result i32)
  (local $strOff i32)
  (local $strEnd i32)
  (local $bufLen i32)
  (local $c1 i32)
  ;;@ ~lib/string.ts:672:6
  (local.set $strOff
   ;;@ ~lib/string.ts:672:19
   (local.get $str)
  )
  ;;@ ~lib/string.ts:673:6
  (local.set $strEnd
   ;;@ ~lib/string.ts:673:19
   (i32.add
    (local.get $strOff)
    ;;@ ~lib/string.ts:673:28
    (call $~lib/rt/common/OBJECT#get:rtSize
     ;;@ ~lib/string.ts:673:35
     (i32.sub
      ;;@ ~lib/string.ts:673:54
      (local.get $str)
      ;;@ ~lib/string.ts:673:79
      (i32.const 20)
     )
    )
   )
  )
  ;;@ ~lib/string.ts:674:6
  (local.set $bufLen
   ;;@ ~lib/string.ts:674:19
   (i32.ne
    (local.get $nullTerminated)
    (i32.const 0)
   )
  )
  ;;@ ~lib/string.ts:675:6
  (block $while-break|0
   (loop $while-continue|0
    (if
     ;;@ ~lib/string.ts:675:13
     (i32.lt_u
      (local.get $strOff)
      ;;@ ~lib/string.ts:675:22
      (local.get $strEnd)
     )
     (block
      ;;@ ~lib/string.ts:676:8
      (local.set $c1
       ;;@ ~lib/string.ts:676:17
       (i32.load16_u
        ;;@ ~lib/string.ts:676:32
        (local.get $strOff)
       )
      )
      ;;@ ~lib/string.ts:677:8
      (if
       ;;@ ~lib/string.ts:677:12
       (i32.lt_u
        (local.get $c1)
        ;;@ ~lib/string.ts:677:17
        (i32.const 128)
       )
       (block
        ;;@ ~lib/string.ts:679:10
        (if
         ;;@ ~lib/string.ts:679:14
         (i32.and
          (local.get $nullTerminated)
          ;;@ ~lib/string.ts:679:31
          (i32.eqz
           ;;@ ~lib/string.ts:679:32
           (local.get $c1)
          )
         )
         ;;@ ~lib/string.ts:679:36
         (br $while-break|0)
        )
        ;;@ ~lib/string.ts:680:10
        (local.set $bufLen
         (i32.add
          (local.get $bufLen)
          ;;@ ~lib/string.ts:680:20
          (i32.const 1)
         )
        )
       )
       ;;@ ~lib/string.ts:681:15
       (if
        ;;@ ~lib/string.ts:681:19
        (i32.lt_u
         (local.get $c1)
         ;;@ ~lib/string.ts:681:24
         (i32.const 2048)
        )
        ;;@ ~lib/string.ts:682:10
        (local.set $bufLen
         (i32.add
          (local.get $bufLen)
          ;;@ ~lib/string.ts:682:20
          (i32.const 2)
         )
        )
        (block
         ;;@ ~lib/string.ts:684:10
         (if
          ;;@ ~lib/string.ts:684:14
          (if (result i32)
           (i32.eq
            ;;@ ~lib/string.ts:684:15
            (i32.and
             (local.get $c1)
             ;;@ ~lib/string.ts:684:20
             (i32.const 64512)
            )
            ;;@ ~lib/string.ts:684:31
            (i32.const 55296)
           )
           ;;@ ~lib/string.ts:684:41
           (i32.lt_u
            (i32.add
             (local.get $strOff)
             ;;@ ~lib/string.ts:684:50
             (i32.const 2)
            )
            ;;@ ~lib/string.ts:684:54
            (local.get $strEnd)
           )
           (i32.const 0)
          )
          ;;@ ~lib/string.ts:685:12
          (if
           ;;@ ~lib/string.ts:685:16
           (i32.eq
            ;;@ ~lib/string.ts:685:17
            (i32.and
             (i32.load16_u offset=2
              ;;@ ~lib/string.ts:685:32
              (local.get $strOff)
             )
             ;;@ ~lib/string.ts:685:45
             (i32.const 64512)
            )
            ;;@ ~lib/string.ts:685:56
            (i32.const 56320)
           )
           (block
            ;;@ ~lib/string.ts:686:14
            (local.set $bufLen
             (i32.add
              (local.get $bufLen)
              ;;@ ~lib/string.ts:686:24
              (i32.const 4)
             )
            )
            ;;@ ~lib/string.ts:686:27
            (local.set $strOff
             (i32.add
              (local.get $strOff)
              ;;@ ~lib/string.ts:686:37
              (i32.const 4)
             )
            )
            ;;@ ~lib/string.ts:687:14
            (br $while-continue|0)
           )
          )
         )
         ;;@ ~lib/string.ts:690:10
         (local.set $bufLen
          (i32.add
           (local.get $bufLen)
           ;;@ ~lib/string.ts:690:20
           (i32.const 3)
          )
         )
        )
       )
      )
      ;;@ ~lib/string.ts:692:8
      (local.set $strOff
       (i32.add
        (local.get $strOff)
        ;;@ ~lib/string.ts:692:18
        (i32.const 2)
       )
      )
      (br $while-continue|0)
     )
    )
   )
  )
  ;;@ ~lib/string.ts:694:6
  (return
   ;;@ ~lib/string.ts:694:13
   (local.get $bufLen)
  )
 )
 (func $~lib/rt/stub/maybeGrowMemory (param $newOffset i32)
  (local $pagesBefore i32)
  (local $maxOffset i32)
  (local $pagesNeeded i32)
  (local $4 i32)
  (local $5 i32)
  (local $pagesWanted i32)
  ;;@ ~lib/rt/stub.ts:13:2
  (local.set $pagesBefore
   ;;@ ~lib/rt/stub.ts:13:20
   (memory.size)
  )
  ;;@ ~lib/rt/stub.ts:14:2
  (local.set $maxOffset
   ;;@ ~lib/rt/stub.ts:14:18
   (i32.and
    ;;@ ~lib/rt/stub.ts:14:19
    (i32.add
     ;;@ ~lib/rt/stub.ts:14:20
     (i32.shl
      (local.get $pagesBefore)
      ;;@ ~lib/rt/stub.ts:14:42
      (i32.const 16)
     )
     ;;@ ~lib/rt/stub.ts:14:48
     (i32.const 15)
    )
    ;;@ ~lib/rt/stub.ts:14:59
    (i32.xor
     ;;@ ~lib/rt/stub.ts:14:60
     (i32.const 15)
     (i32.const -1)
    )
   )
  )
  ;;@ ~lib/rt/stub.ts:15:2
  (if
   ;;@ ~lib/rt/stub.ts:15:6
   (i32.gt_u
    (local.get $newOffset)
    ;;@ ~lib/rt/stub.ts:15:18
    (local.get $maxOffset)
   )
   (block
    ;;@ ~lib/rt/stub.ts:16:4
    (local.set $pagesNeeded
     ;;@ ~lib/rt/stub.ts:16:22
     (i32.shr_u
      ;;@ ~lib/rt/stub.ts:16:29
      (i32.and
       ;;@ ~lib/rt/stub.ts:16:30
       (i32.add
        (i32.sub
         (local.get $newOffset)
         ;;@ ~lib/rt/stub.ts:16:42
         (local.get $maxOffset)
        )
        ;;@ ~lib/rt/stub.ts:16:54
        (i32.const 65535)
       )
       ;;@ ~lib/rt/stub.ts:16:64
       (i32.xor
        ;;@ ~lib/rt/stub.ts:16:65
        (i32.const 65535)
        (i32.const -1)
       )
      )
      ;;@ ~lib/rt/stub.ts:16:77
      (i32.const 16)
     )
    )
    ;;@ ~lib/rt/stub.ts:17:4
    (local.set $pagesWanted
     ;;@ ~lib/rt/stub.ts:17:22
     (select
      (local.tee $4
       ;;@ ~lib/rt/stub.ts:17:26
       (local.get $pagesBefore)
      )
      (local.tee $5
       ;;@ ~lib/rt/stub.ts:17:39
       (local.get $pagesNeeded)
      )
      (i32.gt_s
       (local.get $4)
       (local.get $5)
      )
     )
    )
    ;;@ ~lib/rt/stub.ts:18:4
    (if
     ;;@ ~lib/rt/stub.ts:18:8
     (i32.lt_s
      (memory.grow
       ;;@ ~lib/rt/stub.ts:18:20
       (local.get $pagesWanted)
      )
      ;;@ ~lib/rt/stub.ts:18:35
      (i32.const 0)
     )
     ;;@ ~lib/rt/stub.ts:19:6
     (if
      ;;@ ~lib/rt/stub.ts:19:10
      (i32.lt_s
       (memory.grow
        ;;@ ~lib/rt/stub.ts:19:22
        (local.get $pagesNeeded)
       )
       ;;@ ~lib/rt/stub.ts:19:37
       (i32.const 0)
      )
      ;;@ ~lib/rt/stub.ts:19:40
      (unreachable)
     )
    )
   )
  )
  ;;@ ~lib/rt/stub.ts:22:2
  (global.set $~lib/rt/stub/offset
   ;;@ ~lib/rt/stub.ts:22:11
   (local.get $newOffset)
  )
 )
 (func $~lib/rt/common/BLOCK#set:mmInfo (param $this i32) (param $mmInfo i32)
  (i32.store
   (local.get $this)
   (local.get $mmInfo)
  )
 )
 (func $~lib/rt/stub/__alloc (param $size i32) (result i32)
  (local $block i32)
  (local $ptr i32)
  (local $size|3 i32)
  (local $payloadSize i32)
  ;;@ ~lib/rt/stub.ts:33:2
  (if
   ;;@ ~lib/rt/stub.ts:33:6
   (i32.gt_u
    (local.get $size)
    ;;@ ~lib/rt/stub.ts:33:13
    (i32.const 1073741820)
   )
   ;;@ ~lib/rt/stub.ts:33:28
   (block
    (call $~lib/builtins/abort
     ;;@ ~lib/rt/stub.ts:33:44
     (i32.const 112)
     (i32.const 176)
     (i32.const 33)
     (i32.const 29)
    )
    (unreachable)
   )
  )
  ;;@ ~lib/rt/stub.ts:34:2
  (local.set $block
   ;;@ ~lib/rt/stub.ts:34:14
   (global.get $~lib/rt/stub/offset)
  )
  ;;@ ~lib/rt/stub.ts:35:2
  (local.set $ptr
   ;;@ ~lib/rt/stub.ts:35:12
   (i32.add
    (global.get $~lib/rt/stub/offset)
    ;;@ ~lib/rt/stub.ts:35:21
    (i32.const 4)
   )
  )
  ;;@ ~lib/rt/stub.ts:36:2
  (local.set $payloadSize
   ;;@ ~lib/rt/stub.ts:36:20
   (block $~lib/rt/stub/computeSize|inlined.0 (result i32)
    (local.set $size|3
     ;;@ ~lib/rt/stub.ts:36:32
     (local.get $size)
    )
    ;;@ ~lib/rt/stub.ts:27:2
    (br $~lib/rt/stub/computeSize|inlined.0
     ;;@ ~lib/rt/stub.ts:27:9
     (i32.sub
      ;;@ ~lib/rt/stub.ts:27:10
      (i32.and
       ;;@ ~lib/rt/stub.ts:27:11
       (i32.add
        (i32.add
         (local.get $size|3)
         ;;@ ~lib/rt/stub.ts:27:18
         (i32.const 4)
        )
        ;;@ ~lib/rt/stub.ts:27:35
        (i32.const 15)
       )
       ;;@ ~lib/rt/stub.ts:27:46
       (i32.xor
        ;;@ ~lib/rt/stub.ts:27:47
        (i32.const 15)
        (i32.const -1)
       )
      )
      ;;@ ~lib/rt/stub.ts:27:58
      (i32.const 4)
     )
    )
   )
  )
  ;;@ ~lib/rt/stub.ts:37:2
  (call $~lib/rt/stub/maybeGrowMemory
   ;;@ ~lib/rt/stub.ts:37:18
   (i32.add
    (local.get $ptr)
    ;;@ ~lib/rt/stub.ts:37:24
    (local.get $payloadSize)
   )
  )
  ;;@ ~lib/rt/stub.ts:38:2
  (call $~lib/rt/common/BLOCK#set:mmInfo
   (local.get $block)
   ;;@ ~lib/rt/stub.ts:38:17
   (local.get $payloadSize)
  )
  ;;@ ~lib/rt/stub.ts:39:2
  (return
   ;;@ ~lib/rt/stub.ts:39:9
   (local.get $ptr)
  )
 )
 (func $~lib/rt/common/OBJECT#set:gcInfo (param $this i32) (param $gcInfo i32)
  (i32.store offset=4
   (local.get $this)
   (local.get $gcInfo)
  )
 )
 (func $~lib/rt/common/OBJECT#set:gcInfo2 (param $this i32) (param $gcInfo2 i32)
  (i32.store offset=8
   (local.get $this)
   (local.get $gcInfo2)
  )
 )
 (func $~lib/rt/common/OBJECT#set:rtId (param $this i32) (param $rtId i32)
  (i32.store offset=12
   (local.get $this)
   (local.get $rtId)
  )
 )
 (func $~lib/rt/common/OBJECT#set:rtSize (param $this i32) (param $rtSize i32)
  (i32.store offset=16
   (local.get $this)
   (local.get $rtSize)
  )
 )
 (func $~lib/rt/stub/__new (param $size i32) (param $id i32) (result i32)
  (local $ptr i32)
  (local $object i32)
  ;;@ ~lib/rt/stub.ts:86:2
  (if
   ;;@ ~lib/rt/stub.ts:86:6
   (i32.gt_u
    (local.get $size)
    ;;@ ~lib/rt/stub.ts:86:13
    (i32.const 1073741804)
   )
   ;;@ ~lib/rt/stub.ts:86:29
   (block
    (call $~lib/builtins/abort
     ;;@ ~lib/rt/stub.ts:86:45
     (i32.const 112)
     (i32.const 176)
     (i32.const 86)
     (i32.const 30)
    )
    (unreachable)
   )
  )
  ;;@ ~lib/rt/stub.ts:87:2
  (local.set $ptr
   ;;@ ~lib/rt/stub.ts:87:12
   (call $~lib/rt/stub/__alloc
    ;;@ ~lib/rt/stub.ts:87:20
    (i32.add
     (i32.const 16)
     ;;@ ~lib/rt/stub.ts:87:38
     (local.get $size)
    )
   )
  )
  ;;@ ~lib/rt/stub.ts:88:2
  (local.set $object
   ;;@ ~lib/rt/stub.ts:88:15
   (i32.sub
    ;;@ ~lib/rt/stub.ts:88:34
    (local.get $ptr)
    ;;@ ~lib/rt/stub.ts:88:40
    (i32.const 4)
   )
  )
  ;;@ ~lib/rt/stub.ts:89:2
  (call $~lib/rt/common/OBJECT#set:gcInfo
   (local.get $object)
   ;;@ ~lib/rt/stub.ts:89:18
   (i32.const 0)
  )
  ;;@ ~lib/rt/stub.ts:90:2
  (call $~lib/rt/common/OBJECT#set:gcInfo2
   (local.get $object)
   ;;@ ~lib/rt/stub.ts:90:19
   (i32.const 0)
  )
  ;;@ ~lib/rt/stub.ts:91:2
  (call $~lib/rt/common/OBJECT#set:rtId
   (local.get $object)
   ;;@ ~lib/rt/stub.ts:91:16
   (local.get $id)
  )
  ;;@ ~lib/rt/stub.ts:92:2
  (call $~lib/rt/common/OBJECT#set:rtSize
   (local.get $object)
   ;;@ ~lib/rt/stub.ts:92:18
   (local.get $size)
  )
  ;;@ ~lib/rt/stub.ts:93:2
  (return
   ;;@ ~lib/rt/stub.ts:93:9
   (i32.add
    (local.get $ptr)
    ;;@ ~lib/rt/stub.ts:93:15
    (i32.const 16)
   )
  )
 )
 (func $~lib/string/String#get:length (param $this i32) (result i32)
  ;;@ ~lib/string.ts:51:4
  (return
   ;;@ ~lib/string.ts:51:11
   (i32.shr_u
    (call $~lib/rt/common/OBJECT#get:rtSize
     (i32.sub
      ;;@ ~lib/string.ts:51:30
      (local.get $this)
      ;;@ ~lib/string.ts:51:56
      (i32.const 20)
     )
    )
    ;;@ ~lib/string.ts:51:82
    (i32.const 1)
   )
  )
 )
 (func $~lib/string/String.UTF8.encodeUnsafe (param $str i32) (param $len i32) (param $buf i32) (param $nullTerminated i32) (param $errorMode i32) (result i32)
  (local $strEnd i32)
  (local $bufOff i32)
  (local $c1 i32)
  (local $b0 i32)
  (local $b1 i32)
  (local $c2 i32)
  (local $b0|11 i32)
  (local $b1|12 i32)
  (local $b2 i32)
  (local $b3 i32)
  (local $b0|15 i32)
  (local $b1|16 i32)
  (local $b2|17 i32)
  (local $18 i32)
  ;;@ ~lib/string.ts:706:6
  (local.set $strEnd
   ;;@ ~lib/string.ts:706:19
   (i32.add
    (local.get $str)
    ;;@ ~lib/string.ts:706:26
    (i32.shl
     (local.get $len)
     ;;@ ~lib/string.ts:706:40
     (i32.const 1)
    )
   )
  )
  ;;@ ~lib/string.ts:707:6
  (local.set $bufOff
   ;;@ ~lib/string.ts:707:19
   (local.get $buf)
  )
  ;;@ ~lib/string.ts:708:6
  (block $while-break|0
   (loop $while-continue|0
    (if
     ;;@ ~lib/string.ts:708:13
     (i32.lt_u
      (local.get $str)
      ;;@ ~lib/string.ts:708:19
      (local.get $strEnd)
     )
     (block
      ;;@ ~lib/string.ts:709:8
      (local.set $c1
       ;;@ ~lib/string.ts:709:17
       (i32.load16_u
        ;;@ ~lib/string.ts:709:32
        (local.get $str)
       )
      )
      ;;@ ~lib/string.ts:710:8
      (if
       ;;@ ~lib/string.ts:710:12
       (i32.lt_u
        (local.get $c1)
        ;;@ ~lib/string.ts:710:17
        (i32.const 128)
       )
       (block
        ;;@ ~lib/string.ts:711:10
        (i32.store8
         ;;@ ~lib/string.ts:711:20
         (local.get $bufOff)
         ;;@ ~lib/string.ts:711:28
         (local.get $c1)
        )
        ;;@ ~lib/string.ts:712:10
        (local.set $bufOff
         (i32.add
          (local.get $bufOff)
          (i32.const 1)
         )
        )
        ;;@ ~lib/string.ts:714:10
        (if
         ;;@ ~lib/string.ts:714:14
         (i32.and
          (local.get $nullTerminated)
          ;;@ ~lib/string.ts:714:31
          (i32.eqz
           ;;@ ~lib/string.ts:714:32
           (local.get $c1)
          )
         )
         ;;@ ~lib/string.ts:714:36
         (return
          ;;@ ~lib/string.ts:714:43
          (i32.sub
           (local.get $bufOff)
           ;;@ ~lib/string.ts:714:52
           (local.get $buf)
          )
         )
        )
       )
       ;;@ ~lib/string.ts:715:15
       (if
        ;;@ ~lib/string.ts:715:19
        (i32.lt_u
         (local.get $c1)
         ;;@ ~lib/string.ts:715:24
         (i32.const 2048)
        )
        (block
         ;;@ ~lib/string.ts:716:10
         (local.set $b0
          ;;@ ~lib/string.ts:716:19
          (i32.or
           (i32.shr_u
            (local.get $c1)
            ;;@ ~lib/string.ts:716:25
            (i32.const 6)
           )
           ;;@ ~lib/string.ts:716:29
           (i32.const 192)
          )
         )
         ;;@ ~lib/string.ts:717:10
         (local.set $b1
          ;;@ ~lib/string.ts:717:19
          (i32.or
           (i32.and
            (local.get $c1)
            ;;@ ~lib/string.ts:717:24
            (i32.const 63)
           )
           ;;@ ~lib/string.ts:717:29
           (i32.const 128)
          )
         )
         ;;@ ~lib/string.ts:718:10
         (i32.store16
          ;;@ ~lib/string.ts:718:21
          (local.get $bufOff)
          ;;@ ~lib/string.ts:718:29
          (i32.or
           (i32.shl
            (local.get $b1)
            ;;@ ~lib/string.ts:718:35
            (i32.const 8)
           )
           ;;@ ~lib/string.ts:718:39
           (local.get $b0)
          )
         )
         ;;@ ~lib/string.ts:719:10
         (local.set $bufOff
          (i32.add
           (local.get $bufOff)
           ;;@ ~lib/string.ts:719:20
           (i32.const 2)
          )
         )
        )
        (block
         ;;@ ~lib/string.ts:727:10
         (if
          ;;@ ~lib/string.ts:727:14
          (i32.eq
           ;;@ ~lib/string.ts:727:15
           (i32.and
            (local.get $c1)
            ;;@ ~lib/string.ts:727:20
            (i32.const 63488)
           )
           ;;@ ~lib/string.ts:727:31
           (i32.const 55296)
          )
          (block
           ;;@ ~lib/string.ts:728:12
           (if
            ;;@ ~lib/string.ts:728:16
            (if (result i32)
             (i32.lt_u
              (local.get $c1)
              ;;@ ~lib/string.ts:728:21
              (i32.const 56320)
             )
             ;;@ ~lib/string.ts:728:31
             (i32.lt_u
              (i32.add
               (local.get $str)
               ;;@ ~lib/string.ts:728:37
               (i32.const 2)
              )
              ;;@ ~lib/string.ts:728:41
              (local.get $strEnd)
             )
             (i32.const 0)
            )
            (block
             ;;@ ~lib/string.ts:729:14
             (local.set $c2
              ;;@ ~lib/string.ts:729:23
              (i32.load16_u offset=2
               ;;@ ~lib/string.ts:729:38
               (local.get $str)
              )
             )
             ;;@ ~lib/string.ts:730:14
             (if
              ;;@ ~lib/string.ts:730:18
              (i32.eq
               ;;@ ~lib/string.ts:730:19
               (i32.and
                (local.get $c2)
                ;;@ ~lib/string.ts:730:24
                (i32.const 64512)
               )
               ;;@ ~lib/string.ts:730:35
               (i32.const 56320)
              )
              (block
               ;;@ ~lib/string.ts:731:16
               (local.set $c1
                ;;@ ~lib/string.ts:731:21
                (i32.or
                 (i32.add
                  (i32.const 65536)
                  ;;@ ~lib/string.ts:731:32
                  (i32.shl
                   ;;@ ~lib/string.ts:731:33
                   (i32.and
                    (local.get $c1)
                    ;;@ ~lib/string.ts:731:38
                    (i32.const 1023)
                   )
                   ;;@ ~lib/string.ts:731:49
                   (i32.const 10)
                  )
                 )
                 ;;@ ~lib/string.ts:731:56
                 (i32.and
                  (local.get $c2)
                  ;;@ ~lib/string.ts:731:61
                  (i32.const 1023)
                 )
                )
               )
               ;;@ ~lib/string.ts:732:16
               (local.set $b0|11
                ;;@ ~lib/string.ts:732:25
                (i32.or
                 (i32.shr_u
                  (local.get $c1)
                  ;;@ ~lib/string.ts:732:31
                  (i32.const 18)
                 )
                 ;;@ ~lib/string.ts:732:36
                 (i32.const 240)
                )
               )
               ;;@ ~lib/string.ts:733:16
               (local.set $b1|12
                ;;@ ~lib/string.ts:733:25
                (i32.or
                 (i32.and
                  (i32.shr_u
                   (local.get $c1)
                   ;;@ ~lib/string.ts:733:31
                   (i32.const 12)
                  )
                  ;;@ ~lib/string.ts:733:36
                  (i32.const 63)
                 )
                 ;;@ ~lib/string.ts:733:41
                 (i32.const 128)
                )
               )
               ;;@ ~lib/string.ts:734:16
               (local.set $b2
                ;;@ ~lib/string.ts:734:25
                (i32.or
                 (i32.and
                  (i32.shr_u
                   (local.get $c1)
                   ;;@ ~lib/string.ts:734:31
                   (i32.const 6)
                  )
                  ;;@ ~lib/string.ts:734:36
                  (i32.const 63)
                 )
                 ;;@ ~lib/string.ts:734:41
                 (i32.const 128)
                )
               )
               ;;@ ~lib/string.ts:735:16
               (local.set $b3
                ;;@ ~lib/string.ts:735:25
                (i32.or
                 (i32.and
                  (local.get $c1)
                  ;;@ ~lib/string.ts:735:36
                  (i32.const 63)
                 )
                 ;;@ ~lib/string.ts:735:41
                 (i32.const 128)
                )
               )
               ;;@ ~lib/string.ts:736:16
               (i32.store
                ;;@ ~lib/string.ts:736:27
                (local.get $bufOff)
                ;;@ ~lib/string.ts:736:35
                (i32.or
                 (i32.or
                  (i32.or
                   (i32.shl
                    (local.get $b3)
                    ;;@ ~lib/string.ts:736:41
                    (i32.const 24)
                   )
                   ;;@ ~lib/string.ts:736:46
                   (i32.shl
                    (local.get $b2)
                    ;;@ ~lib/string.ts:736:52
                    (i32.const 16)
                   )
                  )
                  ;;@ ~lib/string.ts:736:57
                  (i32.shl
                   (local.get $b1|12)
                   ;;@ ~lib/string.ts:736:63
                   (i32.const 8)
                  )
                 )
                 ;;@ ~lib/string.ts:736:67
                 (local.get $b0|11)
                )
               )
               ;;@ ~lib/string.ts:737:16
               (local.set $bufOff
                (i32.add
                 (local.get $bufOff)
                 ;;@ ~lib/string.ts:737:26
                 (i32.const 4)
                )
               )
               ;;@ ~lib/string.ts:737:29
               (local.set $str
                (i32.add
                 (local.get $str)
                 ;;@ ~lib/string.ts:737:36
                 (i32.const 4)
                )
               )
               ;;@ ~lib/string.ts:738:16
               (br $while-continue|0)
              )
             )
            )
           )
           ;;@ ~lib/string.ts:741:12
           (if
            ;;@ ~lib/string.ts:741:16
            (i32.ne
             (local.get $errorMode)
             ;;@ ~lib/string.ts:741:29
             (i32.const 0)
            )
            (block
             ;;@ ~lib/string.ts:742:14
             (if
              ;;@ ~lib/string.ts:742:18
              (i32.eq
               (local.get $errorMode)
               ;;@ ~lib/string.ts:742:31
               (i32.const 2)
              )
              ;;@ ~lib/string.ts:742:48
              (block
               (call $~lib/builtins/abort
                ;;@ ~lib/string.ts:742:64
                (i32.const 240)
                (i32.const 304)
                (i32.const 742)
                (i32.const 49)
               )
               (unreachable)
              )
             )
             ;;@ ~lib/string.ts:743:14
             (local.set $c1
              ;;@ ~lib/string.ts:743:19
              (i32.const 65533)
             )
            )
           )
          )
         )
         ;;@ ~lib/string.ts:746:10
         (local.set $b0|15
          ;;@ ~lib/string.ts:746:19
          (i32.or
           (i32.shr_u
            (local.get $c1)
            ;;@ ~lib/string.ts:746:25
            (i32.const 12)
           )
           ;;@ ~lib/string.ts:746:30
           (i32.const 224)
          )
         )
         ;;@ ~lib/string.ts:747:10
         (local.set $b1|16
          ;;@ ~lib/string.ts:747:19
          (i32.or
           (i32.and
            (i32.shr_u
             (local.get $c1)
             ;;@ ~lib/string.ts:747:25
             (i32.const 6)
            )
            ;;@ ~lib/string.ts:747:30
            (i32.const 63)
           )
           ;;@ ~lib/string.ts:747:35
           (i32.const 128)
          )
         )
         ;;@ ~lib/string.ts:748:10
         (local.set $b2|17
          ;;@ ~lib/string.ts:748:19
          (i32.or
           (i32.and
            (local.get $c1)
            ;;@ ~lib/string.ts:748:30
            (i32.const 63)
           )
           ;;@ ~lib/string.ts:748:35
           (i32.const 128)
          )
         )
         ;;@ ~lib/string.ts:749:10
         (i32.store16
          ;;@ ~lib/string.ts:749:21
          (local.get $bufOff)
          ;;@ ~lib/string.ts:749:29
          (i32.or
           (i32.shl
            (local.get $b1|16)
            ;;@ ~lib/string.ts:749:35
            (i32.const 8)
           )
           ;;@ ~lib/string.ts:749:39
           (local.get $b0|15)
          )
         )
         ;;@ ~lib/string.ts:750:10
         (i32.store8 offset=2
          ;;@ ~lib/string.ts:750:20
          (local.get $bufOff)
          ;;@ ~lib/string.ts:750:28
          (local.get $b2|17)
         )
         ;;@ ~lib/string.ts:751:10
         (local.set $bufOff
          (i32.add
           (local.get $bufOff)
           ;;@ ~lib/string.ts:751:20
           (i32.const 3)
          )
         )
        )
       )
      )
      ;;@ ~lib/string.ts:753:8
      (local.set $str
       (i32.add
        (local.get $str)
        ;;@ ~lib/string.ts:753:15
        (i32.const 2)
       )
      )
      (br $while-continue|0)
     )
    )
   )
  )
  ;;@ ~lib/string.ts:755:6
  (if
   ;;@ ~lib/string.ts:755:10
   (local.get $nullTerminated)
   ;;@ ~lib/string.ts:756:8
   (i32.store8
    ;;@ ~lib/string.ts:756:18
    (block (result i32)
     (local.set $bufOff
      (i32.add
       (local.tee $18
        (local.get $bufOff)
       )
       (i32.const 1)
      )
     )
     (local.get $18)
    )
    ;;@ ~lib/string.ts:756:28
    (i32.const 0)
   )
  )
  ;;@ ~lib/string.ts:758:6
  (return
   ;;@ ~lib/string.ts:758:13
   (i32.sub
    (local.get $bufOff)
    ;;@ ~lib/string.ts:758:22
    (local.get $buf)
   )
  )
 )
 (func $~lib/string/String.UTF8.encode (param $str i32) (param $nullTerminated i32) (param $errorMode i32) (result i32)
  (local $buf i32)
  ;;@ ~lib/string.ts:698:6
  (local.set $buf
   ;;@ ~lib/string.ts:698:16
   (call $~lib/rt/stub/__new
    ;;@ ~lib/string.ts:698:46
    (call $~lib/string/String.UTF8.byteLength
     ;;@ ~lib/string.ts:698:64
     (local.get $str)
     ;;@ ~lib/string.ts:698:69
     (local.get $nullTerminated)
    )
    ;;@ ~lib/string.ts:698:86
    (i32.const 1)
   )
  )
  ;;@ ~lib/string.ts:699:6
  (drop
   (call $~lib/string/String.UTF8.encodeUnsafe
    ;;@ ~lib/string.ts:699:19
    (local.get $str)
    ;;@ ~lib/string.ts:699:43
    (call $~lib/string/String#get:length
     (local.get $str)
    )
    ;;@ ~lib/string.ts:699:55
    (local.get $buf)
    ;;@ ~lib/string.ts:699:79
    (local.get $nullTerminated)
    ;;@ ~lib/string.ts:699:95
    (local.get $errorMode)
   )
  )
  ;;@ ~lib/string.ts:700:6
  (return
   ;;@ ~lib/string.ts:700:13
   (local.get $buf)
  )
 )
 (func $~lib/string/String.UTF8.encode@varargs (param $str i32) (param $nullTerminated i32) (param $errorMode i32) (result i32)
  (block $2of2
   (block $1of2
    (block $0of2
     (block $outOfRange
      (br_table $0of2 $1of2 $2of2 $outOfRange
       (i32.sub
        (global.get $~argumentsLength)
        (i32.const 1)
       )
      )
     )
     (unreachable)
    )
    (local.set $nullTerminated
     ;;@ ~lib/string.ts:697:63
     (i32.const 0)
    )
   )
   (local.set $errorMode
    ;;@ ~lib/string.ts:697:93
    (i32.const 0)
   )
  )
  (call $~lib/string/String.UTF8.encode
   (local.get $str)
   (local.get $nullTerminated)
   (local.get $errorMode)
  )
 )
 (func $~lib/as-warduino/assembly/index/print (param $text i32)
  ;;@ ~lib/as-warduino/assembly/index.ts:119:4
  (call $~lib/as-warduino/assembly/warduino/_print
   ;;@ ~lib/as-warduino/assembly/index.ts:119:16
   (call $~lib/string/String.UTF8.encode@varargs
    ;;@ ~lib/as-warduino/assembly/index.ts:119:35
    (local.get $text)
    ;;@ ~lib/as-warduino/assembly/index.ts:119:41
    (i32.const 1)
    (block (result i32)
     (global.set $~argumentsLength
      (i32.const 2)
     )
     (i32.const 0)
    )
   )
   ;;@ ~lib/as-warduino/assembly/index.ts:119:48
   (call $~lib/string/String.UTF8.byteLength
    ;;@ ~lib/as-warduino/assembly/index.ts:119:71
    (local.get $text)
    ;;@ ~lib/as-warduino/assembly/index.ts:119:77
    (i32.const 1)
   )
  )
 )
 (func $~lib/string/String.fromCharCode (param $unit i32) (param $surr i32) (result i32)
  (local $hasSur i32)
  (local $out i32)
  ;;@ ~lib/string.ts:15:4
  (local.set $hasSur
   ;;@ ~lib/string.ts:15:17
   (i32.gt_s
    (local.get $surr)
    ;;@ ~lib/string.ts:15:24
    (i32.const 0)
   )
  )
  ;;@ ~lib/string.ts:16:4
  (local.set $out
   ;;@ ~lib/string.ts:16:14
   (call $~lib/rt/stub/__new
    ;;@ ~lib/string.ts:16:39
    (i32.shl
     (i32.const 2)
     ;;@ ~lib/string.ts:16:44
     (local.get $hasSur)
    )
    ;;@ ~lib/string.ts:16:57
    (i32.const 2)
   )
  )
  ;;@ ~lib/string.ts:17:4
  (i32.store16
   ;;@ ~lib/string.ts:17:15
   (local.get $out)
   ;;@ ~lib/string.ts:17:39
   (local.get $unit)
  )
  ;;@ ~lib/string.ts:18:4
  (if
   ;;@ ~lib/string.ts:18:8
   (local.get $hasSur)
   ;;@ ~lib/string.ts:18:16
   (i32.store16 offset=2
    ;;@ ~lib/string.ts:18:27
    (local.get $out)
    ;;@ ~lib/string.ts:18:51
    (local.get $surr)
   )
  )
  ;;@ ~lib/string.ts:19:4
  (return
   ;;@ ~lib/string.ts:19:11
   (local.get $out)
  )
 )
 (func $~lib/string/String.fromCharCode@varargs (param $unit i32) (param $surr i32) (result i32)
  (block $1of1
   (block $0of1
    (block $outOfRange
     (br_table $0of1 $1of1 $outOfRange
      (i32.sub
       (global.get $~argumentsLength)
       (i32.const 1)
      )
     )
    )
    (unreachable)
   )
   (local.set $surr
    ;;@ ~lib/string.ts:14:45
    (i32.const -1)
   )
  )
  (call $~lib/string/String.fromCharCode
   (local.get $unit)
   (local.get $surr)
  )
 )
 (func $~lib/util/number/decimalCount32 (param $value i32) (result i32)
  ;;@ ~lib/util/number.ts:134:2
  (if
   ;;@ ~lib/util/number.ts:134:6
   (i32.lt_u
    (local.get $value)
    ;;@ ~lib/util/number.ts:134:14
    (i32.const 100000)
   )
   ;;@ ~lib/util/number.ts:135:4
   (if
    ;;@ ~lib/util/number.ts:135:8
    (i32.lt_u
     (local.get $value)
     ;;@ ~lib/util/number.ts:135:16
     (i32.const 100)
    )
    ;;@ ~lib/util/number.ts:136:6
    (return
     ;;@ ~lib/util/number.ts:136:13
     (i32.add
      (i32.const 1)
      ;;@ ~lib/util/number.ts:136:17
      (i32.ge_u
       ;;@ ~lib/util/number.ts:136:21
       (local.get $value)
       ;;@ ~lib/util/number.ts:136:30
       (i32.const 10)
      )
     )
    )
    ;;@ ~lib/util/number.ts:138:6
    (return
     ;;@ ~lib/util/number.ts:138:13
     (i32.add
      (i32.add
       (i32.const 3)
       ;;@ ~lib/util/number.ts:138:17
       (i32.ge_u
        ;;@ ~lib/util/number.ts:138:21
        (local.get $value)
        ;;@ ~lib/util/number.ts:138:30
        (i32.const 10000)
       )
      )
      ;;@ ~lib/util/number.ts:138:39
      (i32.ge_u
       ;;@ ~lib/util/number.ts:138:43
       (local.get $value)
       ;;@ ~lib/util/number.ts:138:52
       (i32.const 1000)
      )
     )
    )
   )
   ;;@ ~lib/util/number.ts:141:4
   (if
    ;;@ ~lib/util/number.ts:141:8
    (i32.lt_u
     (local.get $value)
     ;;@ ~lib/util/number.ts:141:16
     (i32.const 10000000)
    )
    ;;@ ~lib/util/number.ts:142:6
    (return
     ;;@ ~lib/util/number.ts:142:13
     (i32.add
      (i32.const 6)
      ;;@ ~lib/util/number.ts:142:17
      (i32.ge_u
       ;;@ ~lib/util/number.ts:142:21
       (local.get $value)
       ;;@ ~lib/util/number.ts:142:30
       (i32.const 1000000)
      )
     )
    )
    ;;@ ~lib/util/number.ts:144:6
    (return
     ;;@ ~lib/util/number.ts:144:13
     (i32.add
      (i32.add
       (i32.const 8)
       ;;@ ~lib/util/number.ts:144:17
       (i32.ge_u
        ;;@ ~lib/util/number.ts:144:21
        (local.get $value)
        ;;@ ~lib/util/number.ts:144:30
        (i32.const 1000000000)
       )
      )
      ;;@ ~lib/util/number.ts:144:44
      (i32.ge_u
       ;;@ ~lib/util/number.ts:144:48
       (local.get $value)
       ;;@ ~lib/util/number.ts:144:57
       (i32.const 100000000)
      )
     )
    )
   )
  )
 )
 (func $~lib/util/number/utoa32_dec_lut (param $buffer i32) (param $num i32) (param $offset i32)
  (local $t i32)
  (local $r i32)
  (local $d1 i32)
  (local $d2 i32)
  (local $digits1 i64)
  (local $digits2 i64)
  (local $t|9 i32)
  (local $d1|10 i32)
  (local $digits i32)
  (local $digits|12 i32)
  (local $digit i32)
  ;;@ ~lib/util/number.ts:185:2
  (block $while-break|0
   (loop $while-continue|0
    (if
     ;;@ ~lib/util/number.ts:185:9
     (i32.ge_u
      (local.get $num)
      ;;@ ~lib/util/number.ts:185:16
      (i32.const 10000)
     )
     (block
      ;;@ ~lib/util/number.ts:187:4
      (local.set $t
       ;;@ ~lib/util/number.ts:187:12
       (i32.div_u
        (local.get $num)
        ;;@ ~lib/util/number.ts:187:18
        (i32.const 10000)
       )
      )
      ;;@ ~lib/util/number.ts:188:4
      (local.set $r
       ;;@ ~lib/util/number.ts:188:12
       (i32.rem_u
        (local.get $num)
        ;;@ ~lib/util/number.ts:188:18
        (i32.const 10000)
       )
      )
      ;;@ ~lib/util/number.ts:189:4
      (local.set $num
       ;;@ ~lib/util/number.ts:189:10
       (local.get $t)
      )
      ;;@ ~lib/util/number.ts:191:4
      (local.set $d1
       ;;@ ~lib/util/number.ts:191:13
       (i32.div_u
        (local.get $r)
        ;;@ ~lib/util/number.ts:191:17
        (i32.const 100)
       )
      )
      ;;@ ~lib/util/number.ts:192:4
      (local.set $d2
       ;;@ ~lib/util/number.ts:192:13
       (i32.rem_u
        (local.get $r)
        ;;@ ~lib/util/number.ts:192:17
        (i32.const 100)
       )
      )
      ;;@ ~lib/util/number.ts:194:4
      (local.set $digits1
       ;;@ ~lib/util/number.ts:194:18
       (i64.load32_u
        ;;@ ~lib/util/number.ts:194:33
        (i32.add
         (i32.const 604)
         ;;@ ~lib/util/number.ts:194:43
         (i32.shl
          (local.get $d1)
          ;;@ ~lib/util/number.ts:194:56
          (i32.const 2)
         )
        )
       )
      )
      ;;@ ~lib/util/number.ts:195:4
      (local.set $digits2
       ;;@ ~lib/util/number.ts:195:18
       (i64.load32_u
        ;;@ ~lib/util/number.ts:195:33
        (i32.add
         (i32.const 604)
         ;;@ ~lib/util/number.ts:195:43
         (i32.shl
          (local.get $d2)
          ;;@ ~lib/util/number.ts:195:56
          (i32.const 2)
         )
        )
       )
      )
      ;;@ ~lib/util/number.ts:197:4
      (local.set $offset
       (i32.sub
        (local.get $offset)
        ;;@ ~lib/util/number.ts:197:14
        (i32.const 4)
       )
      )
      ;;@ ~lib/util/number.ts:198:4
      (i64.store
       ;;@ ~lib/util/number.ts:198:15
       (i32.add
        (local.get $buffer)
        ;;@ ~lib/util/number.ts:198:25
        (i32.shl
         (local.get $offset)
         ;;@ ~lib/util/number.ts:198:35
         (i32.const 1)
        )
       )
       ;;@ ~lib/util/number.ts:198:39
       (i64.or
        (local.get $digits1)
        ;;@ ~lib/util/number.ts:198:50
        (i64.shl
         (local.get $digits2)
         ;;@ ~lib/util/number.ts:198:61
         (i64.const 32)
        )
       )
      )
      (br $while-continue|0)
     )
    )
   )
  )
  ;;@ ~lib/util/number.ts:201:2
  (if
   ;;@ ~lib/util/number.ts:201:6
   (i32.ge_u
    (local.get $num)
    ;;@ ~lib/util/number.ts:201:13
    (i32.const 100)
   )
   (block
    ;;@ ~lib/util/number.ts:202:4
    (local.set $t|9
     ;;@ ~lib/util/number.ts:202:13
     (i32.div_u
      (local.get $num)
      ;;@ ~lib/util/number.ts:202:19
      (i32.const 100)
     )
    )
    ;;@ ~lib/util/number.ts:203:4
    (local.set $d1|10
     ;;@ ~lib/util/number.ts:203:13
     (i32.rem_u
      (local.get $num)
      ;;@ ~lib/util/number.ts:203:19
      (i32.const 100)
     )
    )
    ;;@ ~lib/util/number.ts:204:4
    (local.set $num
     ;;@ ~lib/util/number.ts:204:10
     (local.get $t|9)
    )
    ;;@ ~lib/util/number.ts:205:4
    (local.set $offset
     (i32.sub
      (local.get $offset)
      ;;@ ~lib/util/number.ts:205:14
      (i32.const 2)
     )
    )
    ;;@ ~lib/util/number.ts:206:4
    (local.set $digits
     ;;@ ~lib/util/number.ts:206:17
     (i32.load
      ;;@ ~lib/util/number.ts:206:27
      (i32.add
       (i32.const 604)
       ;;@ ~lib/util/number.ts:206:37
       (i32.shl
        (local.get $d1|10)
        ;;@ ~lib/util/number.ts:206:50
        (i32.const 2)
       )
      )
     )
    )
    ;;@ ~lib/util/number.ts:207:4
    (i32.store
     ;;@ ~lib/util/number.ts:207:15
     (i32.add
      (local.get $buffer)
      ;;@ ~lib/util/number.ts:207:25
      (i32.shl
       (local.get $offset)
       ;;@ ~lib/util/number.ts:207:35
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/number.ts:207:39
     (local.get $digits)
    )
   )
  )
  ;;@ ~lib/util/number.ts:210:2
  (if
   ;;@ ~lib/util/number.ts:210:6
   (i32.ge_u
    (local.get $num)
    ;;@ ~lib/util/number.ts:210:13
    (i32.const 10)
   )
   (block
    ;;@ ~lib/util/number.ts:211:4
    (local.set $offset
     (i32.sub
      (local.get $offset)
      ;;@ ~lib/util/number.ts:211:14
      (i32.const 2)
     )
    )
    ;;@ ~lib/util/number.ts:212:4
    (local.set $digits|12
     ;;@ ~lib/util/number.ts:212:17
     (i32.load
      ;;@ ~lib/util/number.ts:212:27
      (i32.add
       (i32.const 604)
       ;;@ ~lib/util/number.ts:212:37
       (i32.shl
        (local.get $num)
        ;;@ ~lib/util/number.ts:212:51
        (i32.const 2)
       )
      )
     )
    )
    ;;@ ~lib/util/number.ts:213:4
    (i32.store
     ;;@ ~lib/util/number.ts:213:15
     (i32.add
      (local.get $buffer)
      ;;@ ~lib/util/number.ts:213:25
      (i32.shl
       (local.get $offset)
       ;;@ ~lib/util/number.ts:213:35
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/number.ts:213:39
     (local.get $digits|12)
    )
   )
   (block
    ;;@ ~lib/util/number.ts:215:4
    (local.set $offset
     (i32.sub
      (local.get $offset)
      ;;@ ~lib/util/number.ts:215:14
      (i32.const 1)
     )
    )
    ;;@ ~lib/util/number.ts:216:4
    (local.set $digit
     ;;@ ~lib/util/number.ts:216:16
     (i32.add
      (i32.const 48)
      ;;@ ~lib/util/number.ts:216:30
      (local.get $num)
     )
    )
    ;;@ ~lib/util/number.ts:217:4
    (i32.store16
     ;;@ ~lib/util/number.ts:217:15
     (i32.add
      (local.get $buffer)
      ;;@ ~lib/util/number.ts:217:25
      (i32.shl
       (local.get $offset)
       ;;@ ~lib/util/number.ts:217:35
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/number.ts:217:39
     (local.get $digit)
    )
   )
  )
 )
 (func $~lib/util/number/utoa_hex_lut (param $buffer i32) (param $num i64) (param $offset i32)
  ;;@ ~lib/util/number.ts:253:2
  (block $while-break|0
   (loop $while-continue|0
    (if
     ;;@ ~lib/util/number.ts:253:9
     (i32.ge_u
      (local.get $offset)
      ;;@ ~lib/util/number.ts:253:19
      (i32.const 2)
     )
     (block
      ;;@ ~lib/util/number.ts:254:4
      (local.set $offset
       (i32.sub
        (local.get $offset)
        ;;@ ~lib/util/number.ts:254:14
        (i32.const 2)
       )
      )
      ;;@ ~lib/util/number.ts:255:4
      (i32.store
       ;;@ ~lib/util/number.ts:256:6
       (i32.add
        (local.get $buffer)
        ;;@ ~lib/util/number.ts:256:16
        (i32.shl
         (local.get $offset)
         ;;@ ~lib/util/number.ts:256:26
         (i32.const 1)
        )
       )
       ;;@ ~lib/util/number.ts:257:6
       (i32.load
        ;;@ ~lib/util/number.ts:257:16
        (i32.add
         (i32.const 1024)
         ;;@ ~lib/util/number.ts:257:23
         (i32.shl
          ;;@ ~lib/util/number.ts:257:24
          (i32.and
           (i32.wrap_i64
            (local.get $num)
           )
           ;;@ ~lib/util/number.ts:257:37
           (i32.const 255)
          )
          ;;@ ~lib/util/number.ts:257:46
          (i32.const 2)
         )
        )
       )
      )
      ;;@ ~lib/util/number.ts:259:4
      (local.set $num
       (i64.shr_u
        (local.get $num)
        ;;@ ~lib/util/number.ts:259:12
        (i64.const 8)
       )
      )
      (br $while-continue|0)
     )
    )
   )
  )
  ;;@ ~lib/util/number.ts:261:2
  (if
   ;;@ ~lib/util/number.ts:261:6
   (i32.and
    (local.get $offset)
    ;;@ ~lib/util/number.ts:261:15
    (i32.const 1)
   )
   ;;@ ~lib/util/number.ts:262:4
   (i32.store16
    ;;@ ~lib/util/number.ts:262:15
    (local.get $buffer)
    ;;@ ~lib/util/number.ts:262:23
    (i32.load16_u
     ;;@ ~lib/util/number.ts:262:33
     (i32.add
      (i32.const 1024)
      ;;@ ~lib/util/number.ts:262:40
      (i32.shl
       (i32.wrap_i64
        (local.get $num)
       )
       ;;@ ~lib/util/number.ts:262:54
       (i32.const 6)
      )
     )
    )
   )
  )
 )
 (func $~lib/util/number/ulog_base (param $num i64) (param $base i32) (result i32)
  (local $value i32)
  (local $b64 i64)
  (local $b i64)
  (local $e i32)
  ;;@ ~lib/util/number.ts:168:2
  (if
   ;;@ ~lib/util/number.ts:168:6
   (block $~lib/util/number/isPowerOf2<i32>|inlined.0 (result i32)
    (local.set $value
     ;;@ ~lib/util/number.ts:168:17
     (local.get $base)
    )
    ;;@ ~lib/util/number.ts:128:2
    (br $~lib/util/number/isPowerOf2<i32>|inlined.0
     ;;@ ~lib/util/number.ts:128:9
     (i32.eq
      (i32.popcnt
       ;;@ ~lib/util/number.ts:128:19
       (local.get $value)
      )
      ;;@ ~lib/util/number.ts:128:29
      (i32.const 1)
     )
    )
   )
   ;;@ ~lib/util/number.ts:169:4
   (return
    ;;@ ~lib/util/number.ts:169:11
    (i32.add
     (i32.div_u
      ;;@ ~lib/util/number.ts:169:12
      (i32.sub
       (i32.const 63)
       ;;@ ~lib/util/number.ts:169:17
       (i32.wrap_i64
        (i64.clz
         ;;@ ~lib/util/number.ts:169:26
         (local.get $num)
        )
       )
      )
      ;;@ ~lib/util/number.ts:169:35
      (i32.sub
       (i32.const 31)
       ;;@ ~lib/util/number.ts:169:40
       (i32.clz
        ;;@ ~lib/util/number.ts:169:49
        (local.get $base)
       )
      )
     )
     ;;@ ~lib/util/number.ts:169:58
     (i32.const 1)
    )
   )
  )
  (local.set $b64
   ;;@ ~lib/util/number.ts:171:12
   (i64.extend_i32_s
    (local.get $base)
   )
  )
  (local.set $b
   ;;@ ~lib/util/number.ts:171:27
   (local.get $b64)
  )
  (local.set $e
   ;;@ ~lib/util/number.ts:171:41
   (i32.const 1)
  )
  ;;@ ~lib/util/number.ts:172:2
  (block $while-break|0
   (loop $while-continue|0
    (if
     ;;@ ~lib/util/number.ts:172:9
     (i64.ge_u
      (local.get $num)
      ;;@ ~lib/util/number.ts:172:16
      (local.get $b)
     )
     (block
      ;;@ ~lib/util/number.ts:173:4
      (local.set $num
       (i64.div_u
        (local.get $num)
        ;;@ ~lib/util/number.ts:173:11
        (local.get $b)
       )
      )
      ;;@ ~lib/util/number.ts:174:4
      (local.set $b
       (i64.mul
        (local.get $b)
        ;;@ ~lib/util/number.ts:174:9
        (local.get $b)
       )
      )
      ;;@ ~lib/util/number.ts:175:4
      (local.set $e
       (i32.shl
        (local.get $e)
        ;;@ ~lib/util/number.ts:175:10
        (i32.const 1)
       )
      )
      (br $while-continue|0)
     )
    )
   )
  )
  ;;@ ~lib/util/number.ts:177:2
  (block $while-break|1
   (loop $while-continue|1
    (if
     ;;@ ~lib/util/number.ts:177:9
     (i64.ge_u
      (local.get $num)
      ;;@ ~lib/util/number.ts:177:16
      (i64.const 1)
     )
     (block
      ;;@ ~lib/util/number.ts:178:4
      (local.set $num
       (i64.div_u
        (local.get $num)
        ;;@ ~lib/util/number.ts:178:11
        (local.get $b64)
       )
      )
      ;;@ ~lib/util/number.ts:179:4
      (local.set $e
       (i32.add
        (local.get $e)
        (i32.const 1)
       )
      )
      (br $while-continue|1)
     )
    )
   )
  )
  ;;@ ~lib/util/number.ts:181:2
  (return
   ;;@ ~lib/util/number.ts:181:9
   (i32.sub
    (local.get $e)
    ;;@ ~lib/util/number.ts:181:13
    (i32.const 1)
   )
  )
 )
 (func $~lib/util/number/utoa64_any_core (param $buffer i32) (param $num i64) (param $offset i32) (param $radix i32)
  (local $base i64)
  (local $shift i64)
  (local $mask i64)
  (local $q i64)
  ;;@ ~lib/util/number.ts:329:2
  (local.set $base
   ;;@ ~lib/util/number.ts:329:13
   (i64.extend_i32_s
    (local.get $radix)
   )
  )
  ;;@ ~lib/util/number.ts:330:2
  (if
   ;;@ ~lib/util/number.ts:330:6
   (i32.eq
    ;;@ ~lib/util/number.ts:330:7
    (i32.and
     (local.get $radix)
     ;;@ ~lib/util/number.ts:330:16
     (i32.sub
      (local.get $radix)
      ;;@ ~lib/util/number.ts:330:24
      (i32.const 1)
     )
    )
    ;;@ ~lib/util/number.ts:330:31
    (i32.const 0)
   )
   (block
    ;;@ ~lib/util/number.ts:331:4
    (local.set $shift
     ;;@ ~lib/util/number.ts:331:16
     (i64.extend_i32_s
      (i32.and
       ;;@ ~lib/util/number.ts:331:20
       (i32.ctz
        ;;@ ~lib/util/number.ts:331:24
        (local.get $radix)
       )
       ;;@ ~lib/util/number.ts:331:33
       (i32.const 7)
      )
     )
    )
    ;;@ ~lib/util/number.ts:332:4
    (local.set $mask
     ;;@ ~lib/util/number.ts:332:15
     (i64.sub
      (local.get $base)
      ;;@ ~lib/util/number.ts:332:22
      (i64.const 1)
     )
    )
    ;;@ ~lib/util/number.ts:333:4
    (loop $do-loop|0
     ;;@ ~lib/util/number.ts:334:6
     (local.set $offset
      (i32.sub
       (local.get $offset)
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/number.ts:335:6
     (i32.store16
      ;;@ ~lib/util/number.ts:335:17
      (i32.add
       (local.get $buffer)
       ;;@ ~lib/util/number.ts:335:27
       (i32.shl
        (local.get $offset)
        ;;@ ~lib/util/number.ts:335:37
        (i32.const 1)
       )
      )
      ;;@ ~lib/util/number.ts:335:41
      (i32.load16_u
       ;;@ ~lib/util/number.ts:335:51
       (i32.add
        (i32.const 2080)
        ;;@ ~lib/util/number.ts:335:58
        (i32.shl
         (i32.wrap_i64
          (i64.and
           ;;@ ~lib/util/number.ts:335:64
           (local.get $num)
           ;;@ ~lib/util/number.ts:335:70
           (local.get $mask)
          )
         )
         ;;@ ~lib/util/number.ts:335:79
         (i32.const 1)
        )
       )
      )
     )
     ;;@ ~lib/util/number.ts:336:6
     (local.set $num
      (i64.shr_u
       (local.get $num)
       ;;@ ~lib/util/number.ts:336:14
       (local.get $shift)
      )
     )
     (br_if $do-loop|0
      (i64.ne
       ;;@ ~lib/util/number.ts:337:13
       (local.get $num)
       (i64.const 0)
      )
     )
    )
   )
   ;;@ ~lib/util/number.ts:339:4
   (loop $do-loop|1
    ;;@ ~lib/util/number.ts:340:6
    (local.set $offset
     (i32.sub
      (local.get $offset)
      (i32.const 1)
     )
    )
    ;;@ ~lib/util/number.ts:341:6
    (local.set $q
     ;;@ ~lib/util/number.ts:341:14
     (i64.div_u
      (local.get $num)
      ;;@ ~lib/util/number.ts:341:20
      (local.get $base)
     )
    )
    ;;@ ~lib/util/number.ts:342:6
    (i32.store16
     ;;@ ~lib/util/number.ts:342:17
     (i32.add
      (local.get $buffer)
      ;;@ ~lib/util/number.ts:342:27
      (i32.shl
       (local.get $offset)
       ;;@ ~lib/util/number.ts:342:37
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/number.ts:342:41
     (i32.load16_u
      ;;@ ~lib/util/number.ts:342:51
      (i32.add
       (i32.const 2080)
       ;;@ ~lib/util/number.ts:342:58
       (i32.shl
        (i32.wrap_i64
         (i64.sub
          ;;@ ~lib/util/number.ts:342:64
          (local.get $num)
          ;;@ ~lib/util/number.ts:342:70
          (i64.mul
           (local.get $q)
           ;;@ ~lib/util/number.ts:342:74
           (local.get $base)
          )
         )
        )
        ;;@ ~lib/util/number.ts:342:83
        (i32.const 1)
       )
      )
     )
    )
    ;;@ ~lib/util/number.ts:343:6
    (local.set $num
     ;;@ ~lib/util/number.ts:343:12
     (local.get $q)
    )
    (br_if $do-loop|1
     (i64.ne
      ;;@ ~lib/util/number.ts:344:13
      (local.get $num)
      (i64.const 0)
     )
    )
   )
  )
 )
 (func $~lib/util/number/itoa32 (param $value i32) (param $radix i32) (result i32)
  (local $sign i32)
  (local $out i32)
  (local $decimals i32)
  (local $buffer i32)
  (local $num i32)
  (local $offset i32)
  (local $decimals|8 i32)
  (local $buffer|9 i32)
  (local $num|10 i32)
  (local $offset|11 i32)
  (local $val32 i32)
  (local $decimals|13 i32)
  ;;@ ~lib/util/number.ts:372:2
  (if
   ;;@ ~lib/util/number.ts:372:6
   (if (result i32)
    (i32.lt_s
     (local.get $radix)
     ;;@ ~lib/util/number.ts:372:14
     (i32.const 2)
    )
    (i32.const 1)
    ;;@ ~lib/util/number.ts:372:19
    (i32.gt_s
     (local.get $radix)
     ;;@ ~lib/util/number.ts:372:27
     (i32.const 36)
    )
   )
   (block
    (call $~lib/builtins/abort
     ;;@ ~lib/util/number.ts:373:25
     (i32.const 400)
     (i32.const 528)
     (i32.const 373)
     (i32.const 5)
    )
    (unreachable)
   )
  )
  ;;@ ~lib/util/number.ts:375:2
  (if
   ;;@ ~lib/util/number.ts:375:6
   (i32.eqz
    ;;@ ~lib/util/number.ts:375:7
    (local.get $value)
   )
   ;;@ ~lib/util/number.ts:375:14
   (return
    ;;@ ~lib/util/number.ts:375:21
    (i32.const 592)
   )
  )
  ;;@ ~lib/util/number.ts:377:2
  (local.set $sign
   ;;@ ~lib/util/number.ts:377:13
   (i32.shl
    ;;@ ~lib/util/number.ts:377:14
    (i32.shr_u
     (local.get $value)
     ;;@ ~lib/util/number.ts:377:24
     (i32.const 31)
    )
    ;;@ ~lib/util/number.ts:377:31
    (i32.const 1)
   )
  )
  ;;@ ~lib/util/number.ts:378:2
  (if
   ;;@ ~lib/util/number.ts:378:6
   (local.get $sign)
   ;;@ ~lib/util/number.ts:378:12
   (local.set $value
    ;;@ ~lib/util/number.ts:378:20
    (i32.sub
     (i32.const 0)
     ;;@ ~lib/util/number.ts:378:21
     (local.get $value)
    )
   )
  )
  ;;@ ~lib/util/number.ts:381:2
  (if
   ;;@ ~lib/util/number.ts:381:6
   (i32.eq
    (local.get $radix)
    ;;@ ~lib/util/number.ts:381:15
    (i32.const 10)
   )
   (block
    ;;@ ~lib/util/number.ts:382:4
    (local.set $decimals
     ;;@ ~lib/util/number.ts:382:19
     (call $~lib/util/number/decimalCount32
      ;;@ ~lib/util/number.ts:382:34
      (local.get $value)
     )
    )
    ;;@ ~lib/util/number.ts:383:4
    (local.set $out
     ;;@ ~lib/util/number.ts:383:10
     (call $~lib/rt/stub/__new
      ;;@ ~lib/util/number.ts:383:35
      (i32.add
       ;;@ ~lib/util/number.ts:383:36
       (i32.shl
        (local.get $decimals)
        ;;@ ~lib/util/number.ts:383:48
        (i32.const 1)
       )
       ;;@ ~lib/util/number.ts:383:53
       (local.get $sign)
      )
      ;;@ ~lib/util/number.ts:383:59
      (i32.const 2)
     )
    )
    ;;@ ~lib/util/number.ts:384:4
    (block $~lib/util/number/utoa32_dec_core|inlined.0
     (local.set $buffer
      ;;@ ~lib/util/number.ts:384:20
      (i32.add
       (local.get $out)
       ;;@ ~lib/util/number.ts:384:45
       (local.get $sign)
      )
     )
     (local.set $num
      ;;@ ~lib/util/number.ts:384:51
      (local.get $value)
     )
     (local.set $offset
      ;;@ ~lib/util/number.ts:384:58
      (local.get $decimals)
     )
     (drop
      ;;@ ~lib/util/number.ts:290:6
      (i32.ge_s
       (i32.const 0)
       ;;@ ~lib/util/number.ts:290:26
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/number.ts:292:9
     (call $~lib/util/number/utoa32_dec_lut
      ;;@ ~lib/util/number.ts:293:19
      (local.get $buffer)
      ;;@ ~lib/util/number.ts:293:27
      (local.get $num)
      ;;@ ~lib/util/number.ts:293:32
      (local.get $offset)
     )
    )
   )
   ;;@ ~lib/util/number.ts:385:9
   (if
    ;;@ ~lib/util/number.ts:385:13
    (i32.eq
     (local.get $radix)
     ;;@ ~lib/util/number.ts:385:22
     (i32.const 16)
    )
    (block
     ;;@ ~lib/util/number.ts:386:4
     (local.set $decimals|8
      ;;@ ~lib/util/number.ts:386:19
      (i32.add
       ;;@ ~lib/util/number.ts:386:20
       (i32.shr_s
        (i32.sub
         (i32.const 31)
         ;;@ ~lib/util/number.ts:386:25
         (i32.clz
          ;;@ ~lib/util/number.ts:386:29
          (local.get $value)
         )
        )
        ;;@ ~lib/util/number.ts:386:39
        (i32.const 2)
       )
       ;;@ ~lib/util/number.ts:386:44
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/number.ts:387:4
     (local.set $out
      ;;@ ~lib/util/number.ts:387:10
      (call $~lib/rt/stub/__new
       ;;@ ~lib/util/number.ts:387:35
       (i32.add
        ;;@ ~lib/util/number.ts:387:36
        (i32.shl
         (local.get $decimals|8)
         ;;@ ~lib/util/number.ts:387:48
         (i32.const 1)
        )
        ;;@ ~lib/util/number.ts:387:53
        (local.get $sign)
       )
       ;;@ ~lib/util/number.ts:387:59
       (i32.const 2)
      )
     )
     ;;@ ~lib/util/number.ts:388:4
     (block $~lib/util/number/utoa32_hex_core|inlined.0
      (local.set $buffer|9
       ;;@ ~lib/util/number.ts:388:20
       (i32.add
        (local.get $out)
        ;;@ ~lib/util/number.ts:388:45
        (local.get $sign)
       )
      )
      (local.set $num|10
       ;;@ ~lib/util/number.ts:388:51
       (local.get $value)
      )
      (local.set $offset|11
       ;;@ ~lib/util/number.ts:388:58
       (local.get $decimals|8)
      )
      (drop
       ;;@ ~lib/util/number.ts:300:6
       (i32.ge_s
        (i32.const 0)
        ;;@ ~lib/util/number.ts:300:26
        (i32.const 1)
       )
      )
      ;;@ ~lib/util/number.ts:302:9
      (call $~lib/util/number/utoa_hex_lut
       ;;@ ~lib/util/number.ts:303:17
       (local.get $buffer|9)
       ;;@ ~lib/util/number.ts:303:25
       (i64.extend_i32_u
        (local.get $num|10)
       )
       ;;@ ~lib/util/number.ts:303:30
       (local.get $offset|11)
      )
     )
    )
    (block
     ;;@ ~lib/util/number.ts:390:4
     (local.set $val32
      ;;@ ~lib/util/number.ts:390:16
      (local.get $value)
     )
     ;;@ ~lib/util/number.ts:391:4
     (local.set $decimals|13
      ;;@ ~lib/util/number.ts:391:19
      (call $~lib/util/number/ulog_base
       ;;@ ~lib/util/number.ts:391:29
       (i64.extend_i32_u
        (local.get $val32)
       )
       ;;@ ~lib/util/number.ts:391:36
       (local.get $radix)
      )
     )
     ;;@ ~lib/util/number.ts:392:4
     (local.set $out
      ;;@ ~lib/util/number.ts:392:10
      (call $~lib/rt/stub/__new
       ;;@ ~lib/util/number.ts:392:35
       (i32.add
        ;;@ ~lib/util/number.ts:392:36
        (i32.shl
         (local.get $decimals|13)
         ;;@ ~lib/util/number.ts:392:48
         (i32.const 1)
        )
        ;;@ ~lib/util/number.ts:392:53
        (local.get $sign)
       )
       ;;@ ~lib/util/number.ts:392:59
       (i32.const 2)
      )
     )
     ;;@ ~lib/util/number.ts:393:4
     (call $~lib/util/number/utoa64_any_core
      ;;@ ~lib/util/number.ts:393:20
      (i32.add
       (local.get $out)
       ;;@ ~lib/util/number.ts:393:45
       (local.get $sign)
      )
      ;;@ ~lib/util/number.ts:393:51
      (i64.extend_i32_u
       (local.get $val32)
      )
      ;;@ ~lib/util/number.ts:393:58
      (local.get $decimals|13)
      ;;@ ~lib/util/number.ts:393:68
      (local.get $radix)
     )
    )
   )
  )
  ;;@ ~lib/util/number.ts:395:2
  (if
   ;;@ ~lib/util/number.ts:395:6
   (local.get $sign)
   ;;@ ~lib/util/number.ts:395:12
   (i32.store16
    ;;@ ~lib/util/number.ts:395:23
    (local.get $out)
    ;;@ ~lib/util/number.ts:395:47
    (i32.const 45)
   )
  )
  ;;@ ~lib/util/number.ts:396:2
  (return
   ;;@ ~lib/util/number.ts:396:9
   (local.get $out)
  )
 )
 (func $~lib/number/I32#toString (param $this i32) (param $radix i32) (result i32)
  ;;@ ~lib/number.ts:79:4
  (return
   ;;@ ~lib/number.ts:79:11
   (call $~lib/util/number/itoa32
    ;;@ ~lib/number.ts:79:18
    (local.get $this)
    ;;@ ~lib/number.ts:79:24
    (local.get $radix)
   )
  )
 )
 (func $~lib/util/memory/memcpy (param $dest i32) (param $src i32) (param $n i32)
  (local $w i32)
  (local $x i32)
  (local $5 i32)
  (local $6 i32)
  (local $7 i32)
  (local $8 i32)
  (local $9 i32)
  (local $10 i32)
  (local $11 i32)
  (local $12 i32)
  (local $13 i32)
  (local $14 i32)
  (local $15 i32)
  (local $16 i32)
  (local $17 i32)
  (local $18 i32)
  (local $19 i32)
  (local $20 i32)
  (local $21 i32)
  (local $22 i32)
  (local $23 i32)
  (local $24 i32)
  (local $25 i32)
  (local $26 i32)
  (local $27 i32)
  (local $28 i32)
  (local $29 i32)
  (local $30 i32)
  (local $31 i32)
  (local $32 i32)
  (local $33 i32)
  (local $34 i32)
  (local $35 i32)
  (local $36 i32)
  (local $37 i32)
  (local $38 i32)
  (local $39 i32)
  (local $40 i32)
  (local $41 i32)
  (local $42 i32)
  (local $43 i32)
  (local $44 i32)
  (local $45 i32)
  (local $46 i32)
  (local $47 i32)
  (local $48 i32)
  (local $49 i32)
  (local $50 i32)
  (local $51 i32)
  (local $52 i32)
  (local $53 i32)
  (local $54 i32)
  (local $55 i32)
  (local $56 i32)
  (local $57 i32)
  (local $58 i32)
  (local $59 i32)
  (local $60 i32)
  (local $61 i32)
  (local $62 i32)
  (local $63 i32)
  (local $64 i32)
  (local $65 i32)
  (local $66 i32)
  (local $67 i32)
  (local $68 i32)
  (local $69 i32)
  (local $70 i32)
  (local $71 i32)
  (local $72 i32)
  (local $73 i32)
  (local $74 i32)
  (local $75 i32)
  (local $76 i32)
  (local $77 i32)
  (local $78 i32)
  (local $79 i32)
  (local $80 i32)
  (local $81 i32)
  (local $82 i32)
  (local $83 i32)
  ;;@ ~lib/util/memory.ts:5:2
  (block $while-break|0
   (loop $while-continue|0
    (if
     ;;@ ~lib/util/memory.ts:5:9
     (if (result i32)
      (local.get $n)
      ;;@ ~lib/util/memory.ts:5:15
      (i32.and
       (local.get $src)
       ;;@ ~lib/util/memory.ts:5:21
       (i32.const 3)
      )
      (i32.const 0)
     )
     (block
      ;;@ ~lib/util/memory.ts:6:4
      (i32.store8
       ;;@ ~lib/util/memory.ts:6:14
       (block (result i32)
        (local.set $dest
         (i32.add
          (local.tee $5
           (local.get $dest)
          )
          (i32.const 1)
         )
        )
        (local.get $5)
       )
       ;;@ ~lib/util/memory.ts:6:22
       (i32.load8_u
        ;;@ ~lib/util/memory.ts:6:31
        (block (result i32)
         (local.set $src
          (i32.add
           (local.tee $6
            (local.get $src)
           )
           (i32.const 1)
          )
         )
         (local.get $6)
        )
       )
      )
      ;;@ ~lib/util/memory.ts:7:4
      (local.set $n
       (i32.sub
        (local.get $n)
        (i32.const 1)
       )
      )
      (br $while-continue|0)
     )
    )
   )
  )
  ;;@ ~lib/util/memory.ts:11:2
  (if
   ;;@ ~lib/util/memory.ts:11:6
   (i32.eq
    ;;@ ~lib/util/memory.ts:11:7
    (i32.and
     (local.get $dest)
     ;;@ ~lib/util/memory.ts:11:14
     (i32.const 3)
    )
    ;;@ ~lib/util/memory.ts:11:20
    (i32.const 0)
   )
   (block
    ;;@ ~lib/util/memory.ts:12:4
    (block $while-break|1
     (loop $while-continue|1
      (if
       ;;@ ~lib/util/memory.ts:12:11
       (i32.ge_u
        (local.get $n)
        ;;@ ~lib/util/memory.ts:12:16
        (i32.const 16)
       )
       (block
        ;;@ ~lib/util/memory.ts:13:6
        (i32.store
         ;;@ ~lib/util/memory.ts:13:17
         (local.get $dest)
         ;;@ ~lib/util/memory.ts:13:28
         (i32.load
          ;;@ ~lib/util/memory.ts:13:38
          (local.get $src)
         )
        )
        ;;@ ~lib/util/memory.ts:14:6
        (i32.store
         ;;@ ~lib/util/memory.ts:14:17
         (i32.add
          (local.get $dest)
          ;;@ ~lib/util/memory.ts:14:25
          (i32.const 4)
         )
         ;;@ ~lib/util/memory.ts:14:28
         (i32.load
          ;;@ ~lib/util/memory.ts:14:38
          (i32.add
           (local.get $src)
           ;;@ ~lib/util/memory.ts:14:45
           (i32.const 4)
          )
         )
        )
        ;;@ ~lib/util/memory.ts:15:6
        (i32.store
         ;;@ ~lib/util/memory.ts:15:17
         (i32.add
          (local.get $dest)
          ;;@ ~lib/util/memory.ts:15:25
          (i32.const 8)
         )
         ;;@ ~lib/util/memory.ts:15:28
         (i32.load
          ;;@ ~lib/util/memory.ts:15:38
          (i32.add
           (local.get $src)
           ;;@ ~lib/util/memory.ts:15:45
           (i32.const 8)
          )
         )
        )
        ;;@ ~lib/util/memory.ts:16:6
        (i32.store
         ;;@ ~lib/util/memory.ts:16:17
         (i32.add
          (local.get $dest)
          ;;@ ~lib/util/memory.ts:16:24
          (i32.const 12)
         )
         ;;@ ~lib/util/memory.ts:16:28
         (i32.load
          ;;@ ~lib/util/memory.ts:16:38
          (i32.add
           (local.get $src)
           ;;@ ~lib/util/memory.ts:16:44
           (i32.const 12)
          )
         )
        )
        ;;@ ~lib/util/memory.ts:17:6
        (local.set $src
         (i32.add
          (local.get $src)
          ;;@ ~lib/util/memory.ts:17:13
          (i32.const 16)
         )
        )
        ;;@ ~lib/util/memory.ts:17:17
        (local.set $dest
         (i32.add
          (local.get $dest)
          ;;@ ~lib/util/memory.ts:17:25
          (i32.const 16)
         )
        )
        ;;@ ~lib/util/memory.ts:17:29
        (local.set $n
         (i32.sub
          (local.get $n)
          ;;@ ~lib/util/memory.ts:17:34
          (i32.const 16)
         )
        )
        (br $while-continue|1)
       )
      )
     )
    )
    ;;@ ~lib/util/memory.ts:19:4
    (if
     ;;@ ~lib/util/memory.ts:19:8
     (i32.and
      (local.get $n)
      ;;@ ~lib/util/memory.ts:19:12
      (i32.const 8)
     )
     (block
      ;;@ ~lib/util/memory.ts:20:6
      (i32.store
       ;;@ ~lib/util/memory.ts:20:17
       (local.get $dest)
       ;;@ ~lib/util/memory.ts:20:27
       (i32.load
        ;;@ ~lib/util/memory.ts:20:37
        (local.get $src)
       )
      )
      ;;@ ~lib/util/memory.ts:21:6
      (i32.store
       ;;@ ~lib/util/memory.ts:21:17
       (i32.add
        (local.get $dest)
        ;;@ ~lib/util/memory.ts:21:24
        (i32.const 4)
       )
       ;;@ ~lib/util/memory.ts:21:27
       (i32.load
        ;;@ ~lib/util/memory.ts:21:37
        (i32.add
         (local.get $src)
         ;;@ ~lib/util/memory.ts:21:43
         (i32.const 4)
        )
       )
      )
      ;;@ ~lib/util/memory.ts:22:6
      (local.set $dest
       (i32.add
        (local.get $dest)
        ;;@ ~lib/util/memory.ts:22:14
        (i32.const 8)
       )
      )
      ;;@ ~lib/util/memory.ts:22:17
      (local.set $src
       (i32.add
        (local.get $src)
        ;;@ ~lib/util/memory.ts:22:24
        (i32.const 8)
       )
      )
     )
    )
    ;;@ ~lib/util/memory.ts:24:4
    (if
     ;;@ ~lib/util/memory.ts:24:8
     (i32.and
      (local.get $n)
      ;;@ ~lib/util/memory.ts:24:12
      (i32.const 4)
     )
     (block
      ;;@ ~lib/util/memory.ts:25:6
      (i32.store
       ;;@ ~lib/util/memory.ts:25:17
       (local.get $dest)
       ;;@ ~lib/util/memory.ts:25:23
       (i32.load
        ;;@ ~lib/util/memory.ts:25:33
        (local.get $src)
       )
      )
      ;;@ ~lib/util/memory.ts:26:6
      (local.set $dest
       (i32.add
        (local.get $dest)
        ;;@ ~lib/util/memory.ts:26:14
        (i32.const 4)
       )
      )
      ;;@ ~lib/util/memory.ts:26:17
      (local.set $src
       (i32.add
        (local.get $src)
        ;;@ ~lib/util/memory.ts:26:24
        (i32.const 4)
       )
      )
     )
    )
    ;;@ ~lib/util/memory.ts:28:4
    (if
     ;;@ ~lib/util/memory.ts:28:8
     (i32.and
      (local.get $n)
      ;;@ ~lib/util/memory.ts:28:12
      (i32.const 2)
     )
     (block
      ;;@ ~lib/util/memory.ts:29:6
      (i32.store16
       ;;@ ~lib/util/memory.ts:29:17
       (local.get $dest)
       ;;@ ~lib/util/memory.ts:29:23
       (i32.load16_u
        ;;@ ~lib/util/memory.ts:29:33
        (local.get $src)
       )
      )
      ;;@ ~lib/util/memory.ts:30:6
      (local.set $dest
       (i32.add
        (local.get $dest)
        ;;@ ~lib/util/memory.ts:30:14
        (i32.const 2)
       )
      )
      ;;@ ~lib/util/memory.ts:30:17
      (local.set $src
       (i32.add
        (local.get $src)
        ;;@ ~lib/util/memory.ts:30:24
        (i32.const 2)
       )
      )
     )
    )
    ;;@ ~lib/util/memory.ts:32:4
    (if
     ;;@ ~lib/util/memory.ts:32:8
     (i32.and
      (local.get $n)
      ;;@ ~lib/util/memory.ts:32:12
      (i32.const 1)
     )
     ;;@ ~lib/util/memory.ts:33:6
     (i32.store8
      ;;@ ~lib/util/memory.ts:33:16
      (block (result i32)
       (local.set $dest
        (i32.add
         (local.tee $7
          (local.get $dest)
         )
         (i32.const 1)
        )
       )
       (local.get $7)
      )
      ;;@ ~lib/util/memory.ts:33:24
      (i32.load8_u
       ;;@ ~lib/util/memory.ts:33:33
       (block (result i32)
        (local.set $src
         (i32.add
          (local.tee $8
           (local.get $src)
          )
          (i32.const 1)
         )
        )
        (local.get $8)
       )
      )
     )
    )
    ;;@ ~lib/util/memory.ts:35:4
    (return)
   )
  )
  ;;@ ~lib/util/memory.ts:40:2
  (if
   ;;@ ~lib/util/memory.ts:40:6
   (i32.ge_u
    (local.get $n)
    ;;@ ~lib/util/memory.ts:40:11
    (i32.const 32)
   )
   ;;@ ~lib/util/memory.ts:41:4
   (block $break|2
    (block $case2|2
     (block $case1|2
      (block $case0|2
       (local.set $9
        ;;@ ~lib/util/memory.ts:41:12
        (i32.and
         (local.get $dest)
         ;;@ ~lib/util/memory.ts:41:24
         (i32.const 3)
        )
       )
       (br_if $case0|2
        (i32.eq
         (local.get $9)
         ;;@ ~lib/util/memory.ts:43:11
         (i32.const 1)
        )
       )
       (br_if $case1|2
        (i32.eq
         (local.get $9)
         ;;@ ~lib/util/memory.ts:62:11
         (i32.const 2)
        )
       )
       (br_if $case2|2
        (i32.eq
         (local.get $9)
         ;;@ ~lib/util/memory.ts:80:11
         (i32.const 3)
        )
       )
       (br $break|2)
      )
      ;;@ ~lib/util/memory.ts:43:14
      (block
       ;;@ ~lib/util/memory.ts:44:8
       (local.set $w
        ;;@ ~lib/util/memory.ts:44:12
        (i32.load
         ;;@ ~lib/util/memory.ts:44:22
         (local.get $src)
        )
       )
       ;;@ ~lib/util/memory.ts:45:8
       (i32.store8
        ;;@ ~lib/util/memory.ts:45:18
        (block (result i32)
         (local.set $dest
          (i32.add
           (local.tee $10
            (local.get $dest)
           )
           (i32.const 1)
          )
         )
         (local.get $10)
        )
        ;;@ ~lib/util/memory.ts:45:26
        (i32.load8_u
         ;;@ ~lib/util/memory.ts:45:35
         (block (result i32)
          (local.set $src
           (i32.add
            (local.tee $11
             (local.get $src)
            )
            (i32.const 1)
           )
          )
          (local.get $11)
         )
        )
       )
       ;;@ ~lib/util/memory.ts:46:8
       (i32.store8
        ;;@ ~lib/util/memory.ts:46:18
        (block (result i32)
         (local.set $dest
          (i32.add
           (local.tee $12
            (local.get $dest)
           )
           (i32.const 1)
          )
         )
         (local.get $12)
        )
        ;;@ ~lib/util/memory.ts:46:26
        (i32.load8_u
         ;;@ ~lib/util/memory.ts:46:35
         (block (result i32)
          (local.set $src
           (i32.add
            (local.tee $13
             (local.get $src)
            )
            (i32.const 1)
           )
          )
          (local.get $13)
         )
        )
       )
       ;;@ ~lib/util/memory.ts:47:8
       (i32.store8
        ;;@ ~lib/util/memory.ts:47:18
        (block (result i32)
         (local.set $dest
          (i32.add
           (local.tee $14
            (local.get $dest)
           )
           (i32.const 1)
          )
         )
         (local.get $14)
        )
        ;;@ ~lib/util/memory.ts:47:26
        (i32.load8_u
         ;;@ ~lib/util/memory.ts:47:35
         (block (result i32)
          (local.set $src
           (i32.add
            (local.tee $15
             (local.get $src)
            )
            (i32.const 1)
           )
          )
          (local.get $15)
         )
        )
       )
       ;;@ ~lib/util/memory.ts:48:8
       (local.set $n
        (i32.sub
         (local.get $n)
         ;;@ ~lib/util/memory.ts:48:13
         (i32.const 3)
        )
       )
       ;;@ ~lib/util/memory.ts:49:8
       (block $while-break|3
        (loop $while-continue|3
         (if
          ;;@ ~lib/util/memory.ts:49:15
          (i32.ge_u
           (local.get $n)
           ;;@ ~lib/util/memory.ts:49:20
           (i32.const 17)
          )
          (block
           ;;@ ~lib/util/memory.ts:50:10
           (local.set $x
            ;;@ ~lib/util/memory.ts:50:14
            (i32.load
             ;;@ ~lib/util/memory.ts:50:24
             (i32.add
              (local.get $src)
              ;;@ ~lib/util/memory.ts:50:30
              (i32.const 1)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:51:10
           (i32.store
            ;;@ ~lib/util/memory.ts:51:21
            (local.get $dest)
            ;;@ ~lib/util/memory.ts:51:27
            (i32.or
             (i32.shr_u
              (local.get $w)
              ;;@ ~lib/util/memory.ts:51:32
              (i32.const 24)
             )
             ;;@ ~lib/util/memory.ts:51:37
             (i32.shl
              (local.get $x)
              ;;@ ~lib/util/memory.ts:51:42
              (i32.const 8)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:52:10
           (local.set $w
            ;;@ ~lib/util/memory.ts:52:14
            (i32.load
             ;;@ ~lib/util/memory.ts:52:24
             (i32.add
              (local.get $src)
              ;;@ ~lib/util/memory.ts:52:30
              (i32.const 5)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:53:10
           (i32.store
            ;;@ ~lib/util/memory.ts:53:21
            (i32.add
             (local.get $dest)
             ;;@ ~lib/util/memory.ts:53:28
             (i32.const 4)
            )
            ;;@ ~lib/util/memory.ts:53:31
            (i32.or
             (i32.shr_u
              (local.get $x)
              ;;@ ~lib/util/memory.ts:53:36
              (i32.const 24)
             )
             ;;@ ~lib/util/memory.ts:53:41
             (i32.shl
              (local.get $w)
              ;;@ ~lib/util/memory.ts:53:46
              (i32.const 8)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:54:10
           (local.set $x
            ;;@ ~lib/util/memory.ts:54:14
            (i32.load
             ;;@ ~lib/util/memory.ts:54:24
             (i32.add
              (local.get $src)
              ;;@ ~lib/util/memory.ts:54:30
              (i32.const 9)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:55:10
           (i32.store
            ;;@ ~lib/util/memory.ts:55:21
            (i32.add
             (local.get $dest)
             ;;@ ~lib/util/memory.ts:55:28
             (i32.const 8)
            )
            ;;@ ~lib/util/memory.ts:55:31
            (i32.or
             (i32.shr_u
              (local.get $w)
              ;;@ ~lib/util/memory.ts:55:36
              (i32.const 24)
             )
             ;;@ ~lib/util/memory.ts:55:41
             (i32.shl
              (local.get $x)
              ;;@ ~lib/util/memory.ts:55:46
              (i32.const 8)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:56:10
           (local.set $w
            ;;@ ~lib/util/memory.ts:56:14
            (i32.load
             ;;@ ~lib/util/memory.ts:56:24
             (i32.add
              (local.get $src)
              ;;@ ~lib/util/memory.ts:56:30
              (i32.const 13)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:57:10
           (i32.store
            ;;@ ~lib/util/memory.ts:57:21
            (i32.add
             (local.get $dest)
             ;;@ ~lib/util/memory.ts:57:28
             (i32.const 12)
            )
            ;;@ ~lib/util/memory.ts:57:32
            (i32.or
             (i32.shr_u
              (local.get $x)
              ;;@ ~lib/util/memory.ts:57:37
              (i32.const 24)
             )
             ;;@ ~lib/util/memory.ts:57:42
             (i32.shl
              (local.get $w)
              ;;@ ~lib/util/memory.ts:57:47
              (i32.const 8)
             )
            )
           )
           ;;@ ~lib/util/memory.ts:58:10
           (local.set $src
            (i32.add
             (local.get $src)
             ;;@ ~lib/util/memory.ts:58:17
             (i32.const 16)
            )
           )
           ;;@ ~lib/util/memory.ts:58:21
           (local.set $dest
            (i32.add
             (local.get $dest)
             ;;@ ~lib/util/memory.ts:58:29
             (i32.const 16)
            )
           )
           ;;@ ~lib/util/memory.ts:58:33
           (local.set $n
            (i32.sub
             (local.get $n)
             ;;@ ~lib/util/memory.ts:58:38
             (i32.const 16)
            )
           )
           (br $while-continue|3)
          )
         )
        )
       )
       ;;@ ~lib/util/memory.ts:60:8
       (br $break|2)
      )
     )
     ;;@ ~lib/util/memory.ts:62:14
     (block
      ;;@ ~lib/util/memory.ts:63:8
      (local.set $w
       ;;@ ~lib/util/memory.ts:63:12
       (i32.load
        ;;@ ~lib/util/memory.ts:63:22
        (local.get $src)
       )
      )
      ;;@ ~lib/util/memory.ts:64:8
      (i32.store8
       ;;@ ~lib/util/memory.ts:64:18
       (block (result i32)
        (local.set $dest
         (i32.add
          (local.tee $16
           (local.get $dest)
          )
          (i32.const 1)
         )
        )
        (local.get $16)
       )
       ;;@ ~lib/util/memory.ts:64:26
       (i32.load8_u
        ;;@ ~lib/util/memory.ts:64:35
        (block (result i32)
         (local.set $src
          (i32.add
           (local.tee $17
            (local.get $src)
           )
           (i32.const 1)
          )
         )
         (local.get $17)
        )
       )
      )
      ;;@ ~lib/util/memory.ts:65:8
      (i32.store8
       ;;@ ~lib/util/memory.ts:65:18
       (block (result i32)
        (local.set $dest
         (i32.add
          (local.tee $18
           (local.get $dest)
          )
          (i32.const 1)
         )
        )
        (local.get $18)
       )
       ;;@ ~lib/util/memory.ts:65:26
       (i32.load8_u
        ;;@ ~lib/util/memory.ts:65:35
        (block (result i32)
         (local.set $src
          (i32.add
           (local.tee $19
            (local.get $src)
           )
           (i32.const 1)
          )
         )
         (local.get $19)
        )
       )
      )
      ;;@ ~lib/util/memory.ts:66:8
      (local.set $n
       (i32.sub
        (local.get $n)
        ;;@ ~lib/util/memory.ts:66:13
        (i32.const 2)
       )
      )
      ;;@ ~lib/util/memory.ts:67:8
      (block $while-break|4
       (loop $while-continue|4
        (if
         ;;@ ~lib/util/memory.ts:67:15
         (i32.ge_u
          (local.get $n)
          ;;@ ~lib/util/memory.ts:67:20
          (i32.const 18)
         )
         (block
          ;;@ ~lib/util/memory.ts:68:10
          (local.set $x
           ;;@ ~lib/util/memory.ts:68:14
           (i32.load
            ;;@ ~lib/util/memory.ts:68:24
            (i32.add
             (local.get $src)
             ;;@ ~lib/util/memory.ts:68:30
             (i32.const 2)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:69:10
          (i32.store
           ;;@ ~lib/util/memory.ts:69:21
           (local.get $dest)
           ;;@ ~lib/util/memory.ts:69:27
           (i32.or
            (i32.shr_u
             (local.get $w)
             ;;@ ~lib/util/memory.ts:69:32
             (i32.const 16)
            )
            ;;@ ~lib/util/memory.ts:69:37
            (i32.shl
             (local.get $x)
             ;;@ ~lib/util/memory.ts:69:42
             (i32.const 16)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:70:10
          (local.set $w
           ;;@ ~lib/util/memory.ts:70:14
           (i32.load
            ;;@ ~lib/util/memory.ts:70:24
            (i32.add
             (local.get $src)
             ;;@ ~lib/util/memory.ts:70:30
             (i32.const 6)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:71:10
          (i32.store
           ;;@ ~lib/util/memory.ts:71:21
           (i32.add
            (local.get $dest)
            ;;@ ~lib/util/memory.ts:71:28
            (i32.const 4)
           )
           ;;@ ~lib/util/memory.ts:71:31
           (i32.or
            (i32.shr_u
             (local.get $x)
             ;;@ ~lib/util/memory.ts:71:36
             (i32.const 16)
            )
            ;;@ ~lib/util/memory.ts:71:41
            (i32.shl
             (local.get $w)
             ;;@ ~lib/util/memory.ts:71:46
             (i32.const 16)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:72:10
          (local.set $x
           ;;@ ~lib/util/memory.ts:72:14
           (i32.load
            ;;@ ~lib/util/memory.ts:72:24
            (i32.add
             (local.get $src)
             ;;@ ~lib/util/memory.ts:72:30
             (i32.const 10)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:73:10
          (i32.store
           ;;@ ~lib/util/memory.ts:73:21
           (i32.add
            (local.get $dest)
            ;;@ ~lib/util/memory.ts:73:28
            (i32.const 8)
           )
           ;;@ ~lib/util/memory.ts:73:31
           (i32.or
            (i32.shr_u
             (local.get $w)
             ;;@ ~lib/util/memory.ts:73:36
             (i32.const 16)
            )
            ;;@ ~lib/util/memory.ts:73:41
            (i32.shl
             (local.get $x)
             ;;@ ~lib/util/memory.ts:73:46
             (i32.const 16)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:74:10
          (local.set $w
           ;;@ ~lib/util/memory.ts:74:14
           (i32.load
            ;;@ ~lib/util/memory.ts:74:24
            (i32.add
             (local.get $src)
             ;;@ ~lib/util/memory.ts:74:30
             (i32.const 14)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:75:10
          (i32.store
           ;;@ ~lib/util/memory.ts:75:21
           (i32.add
            (local.get $dest)
            ;;@ ~lib/util/memory.ts:75:28
            (i32.const 12)
           )
           ;;@ ~lib/util/memory.ts:75:32
           (i32.or
            (i32.shr_u
             (local.get $x)
             ;;@ ~lib/util/memory.ts:75:37
             (i32.const 16)
            )
            ;;@ ~lib/util/memory.ts:75:42
            (i32.shl
             (local.get $w)
             ;;@ ~lib/util/memory.ts:75:47
             (i32.const 16)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:76:10
          (local.set $src
           (i32.add
            (local.get $src)
            ;;@ ~lib/util/memory.ts:76:17
            (i32.const 16)
           )
          )
          ;;@ ~lib/util/memory.ts:76:21
          (local.set $dest
           (i32.add
            (local.get $dest)
            ;;@ ~lib/util/memory.ts:76:29
            (i32.const 16)
           )
          )
          ;;@ ~lib/util/memory.ts:76:33
          (local.set $n
           (i32.sub
            (local.get $n)
            ;;@ ~lib/util/memory.ts:76:38
            (i32.const 16)
           )
          )
          (br $while-continue|4)
         )
        )
       )
      )
      ;;@ ~lib/util/memory.ts:78:8
      (br $break|2)
     )
    )
    ;;@ ~lib/util/memory.ts:80:14
    (block
     ;;@ ~lib/util/memory.ts:81:8
     (local.set $w
      ;;@ ~lib/util/memory.ts:81:12
      (i32.load
       ;;@ ~lib/util/memory.ts:81:22
       (local.get $src)
      )
     )
     ;;@ ~lib/util/memory.ts:82:8
     (i32.store8
      ;;@ ~lib/util/memory.ts:82:18
      (block (result i32)
       (local.set $dest
        (i32.add
         (local.tee $20
          (local.get $dest)
         )
         (i32.const 1)
        )
       )
       (local.get $20)
      )
      ;;@ ~lib/util/memory.ts:82:26
      (i32.load8_u
       ;;@ ~lib/util/memory.ts:82:35
       (block (result i32)
        (local.set $src
         (i32.add
          (local.tee $21
           (local.get $src)
          )
          (i32.const 1)
         )
        )
        (local.get $21)
       )
      )
     )
     ;;@ ~lib/util/memory.ts:83:8
     (local.set $n
      (i32.sub
       (local.get $n)
       ;;@ ~lib/util/memory.ts:83:13
       (i32.const 1)
      )
     )
     ;;@ ~lib/util/memory.ts:84:8
     (block $while-break|5
      (loop $while-continue|5
       (if
        ;;@ ~lib/util/memory.ts:84:15
        (i32.ge_u
         (local.get $n)
         ;;@ ~lib/util/memory.ts:84:20
         (i32.const 19)
        )
        (block
         ;;@ ~lib/util/memory.ts:85:10
         (local.set $x
          ;;@ ~lib/util/memory.ts:85:14
          (i32.load
           ;;@ ~lib/util/memory.ts:85:24
           (i32.add
            (local.get $src)
            ;;@ ~lib/util/memory.ts:85:30
            (i32.const 3)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:86:10
         (i32.store
          ;;@ ~lib/util/memory.ts:86:21
          (local.get $dest)
          ;;@ ~lib/util/memory.ts:86:27
          (i32.or
           (i32.shr_u
            (local.get $w)
            ;;@ ~lib/util/memory.ts:86:32
            (i32.const 8)
           )
           ;;@ ~lib/util/memory.ts:86:36
           (i32.shl
            (local.get $x)
            ;;@ ~lib/util/memory.ts:86:41
            (i32.const 24)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:87:10
         (local.set $w
          ;;@ ~lib/util/memory.ts:87:14
          (i32.load
           ;;@ ~lib/util/memory.ts:87:24
           (i32.add
            (local.get $src)
            ;;@ ~lib/util/memory.ts:87:30
            (i32.const 7)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:88:10
         (i32.store
          ;;@ ~lib/util/memory.ts:88:21
          (i32.add
           (local.get $dest)
           ;;@ ~lib/util/memory.ts:88:28
           (i32.const 4)
          )
          ;;@ ~lib/util/memory.ts:88:31
          (i32.or
           (i32.shr_u
            (local.get $x)
            ;;@ ~lib/util/memory.ts:88:36
            (i32.const 8)
           )
           ;;@ ~lib/util/memory.ts:88:40
           (i32.shl
            (local.get $w)
            ;;@ ~lib/util/memory.ts:88:45
            (i32.const 24)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:89:10
         (local.set $x
          ;;@ ~lib/util/memory.ts:89:14
          (i32.load
           ;;@ ~lib/util/memory.ts:89:24
           (i32.add
            (local.get $src)
            ;;@ ~lib/util/memory.ts:89:30
            (i32.const 11)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:90:10
         (i32.store
          ;;@ ~lib/util/memory.ts:90:21
          (i32.add
           (local.get $dest)
           ;;@ ~lib/util/memory.ts:90:28
           (i32.const 8)
          )
          ;;@ ~lib/util/memory.ts:90:31
          (i32.or
           (i32.shr_u
            (local.get $w)
            ;;@ ~lib/util/memory.ts:90:36
            (i32.const 8)
           )
           ;;@ ~lib/util/memory.ts:90:40
           (i32.shl
            (local.get $x)
            ;;@ ~lib/util/memory.ts:90:45
            (i32.const 24)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:91:10
         (local.set $w
          ;;@ ~lib/util/memory.ts:91:14
          (i32.load
           ;;@ ~lib/util/memory.ts:91:24
           (i32.add
            (local.get $src)
            ;;@ ~lib/util/memory.ts:91:30
            (i32.const 15)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:92:10
         (i32.store
          ;;@ ~lib/util/memory.ts:92:21
          (i32.add
           (local.get $dest)
           ;;@ ~lib/util/memory.ts:92:28
           (i32.const 12)
          )
          ;;@ ~lib/util/memory.ts:92:32
          (i32.or
           (i32.shr_u
            (local.get $x)
            ;;@ ~lib/util/memory.ts:92:37
            (i32.const 8)
           )
           ;;@ ~lib/util/memory.ts:92:41
           (i32.shl
            (local.get $w)
            ;;@ ~lib/util/memory.ts:92:46
            (i32.const 24)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:93:10
         (local.set $src
          (i32.add
           (local.get $src)
           ;;@ ~lib/util/memory.ts:93:17
           (i32.const 16)
          )
         )
         ;;@ ~lib/util/memory.ts:93:21
         (local.set $dest
          (i32.add
           (local.get $dest)
           ;;@ ~lib/util/memory.ts:93:29
           (i32.const 16)
          )
         )
         ;;@ ~lib/util/memory.ts:93:33
         (local.set $n
          (i32.sub
           (local.get $n)
           ;;@ ~lib/util/memory.ts:93:38
           (i32.const 16)
          )
         )
         (br $while-continue|5)
        )
       )
      )
     )
     ;;@ ~lib/util/memory.ts:95:8
     (br $break|2)
    )
   )
  )
  ;;@ ~lib/util/memory.ts:101:2
  (if
   ;;@ ~lib/util/memory.ts:101:6
   (i32.and
    (local.get $n)
    ;;@ ~lib/util/memory.ts:101:10
    (i32.const 16)
   )
   (block
    ;;@ ~lib/util/memory.ts:102:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:102:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $22
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $22)
     )
     ;;@ ~lib/util/memory.ts:102:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:102:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $23
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $23)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:103:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:103:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $24
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $24)
     )
     ;;@ ~lib/util/memory.ts:103:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:103:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $25
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $25)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:104:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:104:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $26
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $26)
     )
     ;;@ ~lib/util/memory.ts:104:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:104:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $27
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $27)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:105:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:105:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $28
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $28)
     )
     ;;@ ~lib/util/memory.ts:105:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:105:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $29
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $29)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:106:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:106:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $30
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $30)
     )
     ;;@ ~lib/util/memory.ts:106:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:106:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $31
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $31)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:107:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:107:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $32
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $32)
     )
     ;;@ ~lib/util/memory.ts:107:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:107:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $33
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $33)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:108:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:108:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $34
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $34)
     )
     ;;@ ~lib/util/memory.ts:108:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:108:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $35
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $35)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:109:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:109:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $36
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $36)
     )
     ;;@ ~lib/util/memory.ts:109:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:109:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $37
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $37)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:110:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:110:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $38
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $38)
     )
     ;;@ ~lib/util/memory.ts:110:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:110:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $39
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $39)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:111:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:111:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $40
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $40)
     )
     ;;@ ~lib/util/memory.ts:111:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:111:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $41
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $41)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:112:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:112:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $42
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $42)
     )
     ;;@ ~lib/util/memory.ts:112:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:112:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $43
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $43)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:113:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:113:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $44
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $44)
     )
     ;;@ ~lib/util/memory.ts:113:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:113:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $45
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $45)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:114:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:114:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $46
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $46)
     )
     ;;@ ~lib/util/memory.ts:114:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:114:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $47
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $47)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:115:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:115:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $48
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $48)
     )
     ;;@ ~lib/util/memory.ts:115:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:115:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $49
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $49)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:116:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:116:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $50
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $50)
     )
     ;;@ ~lib/util/memory.ts:116:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:116:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $51
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $51)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:117:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:117:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $52
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $52)
     )
     ;;@ ~lib/util/memory.ts:117:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:117:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $53
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $53)
      )
     )
    )
   )
  )
  ;;@ ~lib/util/memory.ts:119:2
  (if
   ;;@ ~lib/util/memory.ts:119:6
   (i32.and
    (local.get $n)
    ;;@ ~lib/util/memory.ts:119:10
    (i32.const 8)
   )
   (block
    ;;@ ~lib/util/memory.ts:120:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:120:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $54
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $54)
     )
     ;;@ ~lib/util/memory.ts:120:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:120:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $55
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $55)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:121:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:121:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $56
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $56)
     )
     ;;@ ~lib/util/memory.ts:121:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:121:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $57
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $57)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:122:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:122:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $58
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $58)
     )
     ;;@ ~lib/util/memory.ts:122:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:122:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $59
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $59)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:123:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:123:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $60
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $60)
     )
     ;;@ ~lib/util/memory.ts:123:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:123:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $61
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $61)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:124:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:124:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $62
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $62)
     )
     ;;@ ~lib/util/memory.ts:124:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:124:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $63
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $63)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:125:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:125:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $64
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $64)
     )
     ;;@ ~lib/util/memory.ts:125:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:125:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $65
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $65)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:126:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:126:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $66
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $66)
     )
     ;;@ ~lib/util/memory.ts:126:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:126:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $67
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $67)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:127:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:127:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $68
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $68)
     )
     ;;@ ~lib/util/memory.ts:127:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:127:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $69
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $69)
      )
     )
    )
   )
  )
  ;;@ ~lib/util/memory.ts:129:2
  (if
   ;;@ ~lib/util/memory.ts:129:6
   (i32.and
    (local.get $n)
    ;;@ ~lib/util/memory.ts:129:10
    (i32.const 4)
   )
   (block
    ;;@ ~lib/util/memory.ts:130:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:130:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $70
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $70)
     )
     ;;@ ~lib/util/memory.ts:130:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:130:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $71
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $71)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:131:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:131:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $72
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $72)
     )
     ;;@ ~lib/util/memory.ts:131:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:131:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $73
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $73)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:132:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:132:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $74
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $74)
     )
     ;;@ ~lib/util/memory.ts:132:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:132:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $75
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $75)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:133:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:133:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $76
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $76)
     )
     ;;@ ~lib/util/memory.ts:133:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:133:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $77
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $77)
      )
     )
    )
   )
  )
  ;;@ ~lib/util/memory.ts:135:2
  (if
   ;;@ ~lib/util/memory.ts:135:6
   (i32.and
    (local.get $n)
    ;;@ ~lib/util/memory.ts:135:10
    (i32.const 2)
   )
   (block
    ;;@ ~lib/util/memory.ts:136:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:136:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $78
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $78)
     )
     ;;@ ~lib/util/memory.ts:136:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:136:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $79
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $79)
      )
     )
    )
    ;;@ ~lib/util/memory.ts:137:4
    (i32.store8
     ;;@ ~lib/util/memory.ts:137:14
     (block (result i32)
      (local.set $dest
       (i32.add
        (local.tee $80
         (local.get $dest)
        )
        (i32.const 1)
       )
      )
      (local.get $80)
     )
     ;;@ ~lib/util/memory.ts:137:22
     (i32.load8_u
      ;;@ ~lib/util/memory.ts:137:31
      (block (result i32)
       (local.set $src
        (i32.add
         (local.tee $81
          (local.get $src)
         )
         (i32.const 1)
        )
       )
       (local.get $81)
      )
     )
    )
   )
  )
  ;;@ ~lib/util/memory.ts:139:2
  (if
   ;;@ ~lib/util/memory.ts:139:6
   (i32.and
    (local.get $n)
    ;;@ ~lib/util/memory.ts:139:10
    (i32.const 1)
   )
   ;;@ ~lib/util/memory.ts:140:4
   (i32.store8
    ;;@ ~lib/util/memory.ts:140:14
    (block (result i32)
     (local.set $dest
      (i32.add
       (local.tee $82
        (local.get $dest)
       )
       (i32.const 1)
      )
     )
     (local.get $82)
    )
    ;;@ ~lib/util/memory.ts:140:22
    (i32.load8_u
     ;;@ ~lib/util/memory.ts:140:31
     (block (result i32)
      (local.set $src
       (i32.add
        (local.tee $83
         (local.get $src)
        )
        (i32.const 1)
       )
      )
      (local.get $83)
     )
    )
   )
  )
 )
 (func $~lib/memory/memory.copy (param $dst i32) (param $src i32) (param $n i32)
  (local $dest i32)
  (local $src|4 i32)
  (local $n|5 i32)
  (local $6 i32)
  (local $7 i32)
  (local $8 i32)
  (local $9 i32)
  ;;@ ~lib/memory.ts:28:4
  (block $~lib/util/memory/memmove|inlined.0
   (local.set $dest
    ;;@ ~lib/memory.ts:28:12
    (local.get $dst)
   )
   (local.set $src|4
    ;;@ ~lib/memory.ts:28:17
    (local.get $src)
   )
   (local.set $n|5
    ;;@ ~lib/memory.ts:28:22
    (local.get $n)
   )
   ;;@ ~lib/util/memory.ts:147:2
   (if
    ;;@ ~lib/util/memory.ts:147:6
    (i32.eq
     (local.get $dest)
     ;;@ ~lib/util/memory.ts:147:14
     (local.get $src|4)
    )
    ;;@ ~lib/util/memory.ts:147:19
    (br $~lib/util/memory/memmove|inlined.0)
   )
   (drop
    ;;@ ~lib/util/memory.ts:148:6
    (i32.lt_s
     (i32.const 0)
     ;;@ ~lib/util/memory.ts:148:25
     (i32.const 1)
    )
   )
   ;;@ ~lib/util/memory.ts:148:28
   (if
    ;;@ ~lib/util/memory.ts:149:8
    (i32.le_u
     (i32.sub
      (i32.sub
       (local.get $src|4)
       ;;@ ~lib/util/memory.ts:149:14
       (local.get $dest)
      )
      ;;@ ~lib/util/memory.ts:149:21
      (local.get $n|5)
     )
     ;;@ ~lib/util/memory.ts:149:26
     (i32.sub
      (i32.const 0)
      ;;@ ~lib/util/memory.ts:149:28
      (i32.shl
       (local.get $n|5)
       ;;@ ~lib/util/memory.ts:149:33
       (i32.const 1)
      )
     )
    )
    (block
     ;;@ ~lib/util/memory.ts:150:6
     (call $~lib/util/memory/memcpy
      ;;@ ~lib/util/memory.ts:150:13
      (local.get $dest)
      ;;@ ~lib/util/memory.ts:150:19
      (local.get $src|4)
      ;;@ ~lib/util/memory.ts:150:24
      (local.get $n|5)
     )
     ;;@ ~lib/util/memory.ts:151:6
     (br $~lib/util/memory/memmove|inlined.0)
    )
   )
   ;;@ ~lib/util/memory.ts:154:2
   (if
    ;;@ ~lib/util/memory.ts:154:6
    (i32.lt_u
     (local.get $dest)
     ;;@ ~lib/util/memory.ts:154:13
     (local.get $src|4)
    )
    (block
     (drop
      ;;@ ~lib/util/memory.ts:155:8
      (i32.lt_s
       (i32.const 0)
       ;;@ ~lib/util/memory.ts:155:27
       (i32.const 2)
      )
     )
     ;;@ ~lib/util/memory.ts:155:30
     (if
      ;;@ ~lib/util/memory.ts:156:10
      (i32.eq
       ;;@ ~lib/util/memory.ts:156:11
       (i32.and
        (local.get $src|4)
        ;;@ ~lib/util/memory.ts:156:17
        (i32.const 7)
       )
       ;;@ ~lib/util/memory.ts:156:24
       (i32.and
        (local.get $dest)
        ;;@ ~lib/util/memory.ts:156:31
        (i32.const 7)
       )
      )
      (block
       ;;@ ~lib/util/memory.ts:157:8
       (block $while-break|0
        (loop $while-continue|0
         (if
          ;;@ ~lib/util/memory.ts:157:15
          (i32.and
           (local.get $dest)
           ;;@ ~lib/util/memory.ts:157:22
           (i32.const 7)
          )
          (block
           ;;@ ~lib/util/memory.ts:158:10
           (if
            ;;@ ~lib/util/memory.ts:158:14
            (i32.eqz
             ;;@ ~lib/util/memory.ts:158:15
             (local.get $n|5)
            )
            ;;@ ~lib/util/memory.ts:158:18
            (br $~lib/util/memory/memmove|inlined.0)
           )
           ;;@ ~lib/util/memory.ts:159:10
           (local.set $n|5
            (i32.sub
             ;;@ ~lib/util/memory.ts:159:12
             (local.get $n|5)
             (i32.const 1)
            )
           )
           ;;@ ~lib/util/memory.ts:160:10
           (i32.store8
            ;;@ ~lib/util/memory.ts:160:20
            (block (result i32)
             (local.set $dest
              (i32.add
               (local.tee $6
                (local.get $dest)
               )
               (i32.const 1)
              )
             )
             (local.get $6)
            )
            ;;@ ~lib/util/memory.ts:160:28
            (i32.load8_u
             ;;@ ~lib/util/memory.ts:160:37
             (block (result i32)
              (local.set $src|4
               (i32.add
                (local.tee $7
                 (local.get $src|4)
                )
                (i32.const 1)
               )
              )
              (local.get $7)
             )
            )
           )
           (br $while-continue|0)
          )
         )
        )
       )
       ;;@ ~lib/util/memory.ts:162:8
       (block $while-break|1
        (loop $while-continue|1
         (if
          ;;@ ~lib/util/memory.ts:162:15
          (i32.ge_u
           (local.get $n|5)
           ;;@ ~lib/util/memory.ts:162:20
           (i32.const 8)
          )
          (block
           ;;@ ~lib/util/memory.ts:163:10
           (i64.store
            ;;@ ~lib/util/memory.ts:163:21
            (local.get $dest)
            ;;@ ~lib/util/memory.ts:163:27
            (i64.load
             ;;@ ~lib/util/memory.ts:163:37
             (local.get $src|4)
            )
           )
           ;;@ ~lib/util/memory.ts:164:10
           (local.set $n|5
            (i32.sub
             (local.get $n|5)
             ;;@ ~lib/util/memory.ts:164:18
             (i32.const 8)
            )
           )
           ;;@ ~lib/util/memory.ts:165:10
           (local.set $dest
            (i32.add
             (local.get $dest)
             ;;@ ~lib/util/memory.ts:165:18
             (i32.const 8)
            )
           )
           ;;@ ~lib/util/memory.ts:166:10
           (local.set $src|4
            (i32.add
             (local.get $src|4)
             ;;@ ~lib/util/memory.ts:166:18
             (i32.const 8)
            )
           )
           (br $while-continue|1)
          )
         )
        )
       )
      )
     )
     ;;@ ~lib/util/memory.ts:170:4
     (block $while-break|2
      (loop $while-continue|2
       (if
        ;;@ ~lib/util/memory.ts:170:11
        (local.get $n|5)
        (block
         ;;@ ~lib/util/memory.ts:171:6
         (i32.store8
          ;;@ ~lib/util/memory.ts:171:16
          (block (result i32)
           (local.set $dest
            (i32.add
             (local.tee $8
              (local.get $dest)
             )
             (i32.const 1)
            )
           )
           (local.get $8)
          )
          ;;@ ~lib/util/memory.ts:171:24
          (i32.load8_u
           ;;@ ~lib/util/memory.ts:171:33
           (block (result i32)
            (local.set $src|4
             (i32.add
              (local.tee $9
               (local.get $src|4)
              )
              (i32.const 1)
             )
            )
            (local.get $9)
           )
          )
         )
         ;;@ ~lib/util/memory.ts:172:6
         (local.set $n|5
          (i32.sub
           ;;@ ~lib/util/memory.ts:172:8
           (local.get $n|5)
           (i32.const 1)
          )
         )
         (br $while-continue|2)
        )
       )
      )
     )
    )
    (block
     (drop
      ;;@ ~lib/util/memory.ts:175:8
      (i32.lt_s
       (i32.const 0)
       ;;@ ~lib/util/memory.ts:175:27
       (i32.const 2)
      )
     )
     ;;@ ~lib/util/memory.ts:175:30
     (if
      ;;@ ~lib/util/memory.ts:176:10
      (i32.eq
       ;;@ ~lib/util/memory.ts:176:11
       (i32.and
        (local.get $src|4)
        ;;@ ~lib/util/memory.ts:176:17
        (i32.const 7)
       )
       ;;@ ~lib/util/memory.ts:176:24
       (i32.and
        (local.get $dest)
        ;;@ ~lib/util/memory.ts:176:31
        (i32.const 7)
       )
      )
      (block
       ;;@ ~lib/util/memory.ts:177:8
       (block $while-break|3
        (loop $while-continue|3
         (if
          ;;@ ~lib/util/memory.ts:177:15
          (i32.and
           ;;@ ~lib/util/memory.ts:177:16
           (i32.add
            (local.get $dest)
            ;;@ ~lib/util/memory.ts:177:23
            (local.get $n|5)
           )
           ;;@ ~lib/util/memory.ts:177:28
           (i32.const 7)
          )
          (block
           ;;@ ~lib/util/memory.ts:178:10
           (if
            ;;@ ~lib/util/memory.ts:178:14
            (i32.eqz
             ;;@ ~lib/util/memory.ts:178:15
             (local.get $n|5)
            )
            ;;@ ~lib/util/memory.ts:178:18
            (br $~lib/util/memory/memmove|inlined.0)
           )
           ;;@ ~lib/util/memory.ts:179:10
           (i32.store8
            ;;@ ~lib/util/memory.ts:179:20
            (i32.add
             (local.get $dest)
             ;;@ ~lib/util/memory.ts:179:27
             (local.tee $n|5
              (i32.sub
               ;;@ ~lib/util/memory.ts:179:29
               (local.get $n|5)
               (i32.const 1)
              )
             )
            )
            ;;@ ~lib/util/memory.ts:179:32
            (i32.load8_u
             ;;@ ~lib/util/memory.ts:179:41
             (i32.add
              (local.get $src|4)
              ;;@ ~lib/util/memory.ts:179:47
              (local.get $n|5)
             )
            )
           )
           (br $while-continue|3)
          )
         )
        )
       )
       ;;@ ~lib/util/memory.ts:181:8
       (block $while-break|4
        (loop $while-continue|4
         (if
          ;;@ ~lib/util/memory.ts:181:15
          (i32.ge_u
           (local.get $n|5)
           ;;@ ~lib/util/memory.ts:181:20
           (i32.const 8)
          )
          (block
           ;;@ ~lib/util/memory.ts:182:10
           (local.set $n|5
            (i32.sub
             (local.get $n|5)
             ;;@ ~lib/util/memory.ts:182:15
             (i32.const 8)
            )
           )
           ;;@ ~lib/util/memory.ts:183:10
           (i64.store
            ;;@ ~lib/util/memory.ts:183:21
            (i32.add
             (local.get $dest)
             ;;@ ~lib/util/memory.ts:183:28
             (local.get $n|5)
            )
            ;;@ ~lib/util/memory.ts:183:31
            (i64.load
             ;;@ ~lib/util/memory.ts:183:41
             (i32.add
              (local.get $src|4)
              ;;@ ~lib/util/memory.ts:183:47
              (local.get $n|5)
             )
            )
           )
           (br $while-continue|4)
          )
         )
        )
       )
      )
     )
     ;;@ ~lib/util/memory.ts:187:4
     (block $while-break|5
      (loop $while-continue|5
       (if
        ;;@ ~lib/util/memory.ts:187:11
        (local.get $n|5)
        (block
         ;;@ ~lib/util/memory.ts:188:6
         (i32.store8
          ;;@ ~lib/util/memory.ts:188:16
          (i32.add
           (local.get $dest)
           ;;@ ~lib/util/memory.ts:188:23
           (local.tee $n|5
            (i32.sub
             ;;@ ~lib/util/memory.ts:188:25
             (local.get $n|5)
             (i32.const 1)
            )
           )
          )
          ;;@ ~lib/util/memory.ts:188:28
          (i32.load8_u
           ;;@ ~lib/util/memory.ts:188:37
           (i32.add
            (local.get $src|4)
            ;;@ ~lib/util/memory.ts:188:43
            (local.get $n|5)
           )
          )
         )
         (br $while-continue|5)
        )
       )
      )
     )
    )
   )
  )
 )
 (func $~lib/string/String#concat (param $this i32) (param $other i32) (result i32)
  (local $thisSize i32)
  (local $otherSize i32)
  (local $outSize i32)
  (local $out i32)
  ;;@ ~lib/string.ts:90:4
  (local.set $thisSize
   ;;@ ~lib/string.ts:90:26
   (i32.shl
    (call $~lib/string/String#get:length
     (local.get $this)
    )
    ;;@ ~lib/string.ts:90:41
    (i32.const 1)
   )
  )
  ;;@ ~lib/string.ts:91:4
  (local.set $otherSize
   ;;@ ~lib/string.ts:91:27
   (i32.shl
    (call $~lib/string/String#get:length
     (local.get $other)
    )
    ;;@ ~lib/string.ts:91:43
    (i32.const 1)
   )
  )
  ;;@ ~lib/string.ts:92:4
  (local.set $outSize
   ;;@ ~lib/string.ts:92:25
   (i32.add
    (local.get $thisSize)
    ;;@ ~lib/string.ts:92:36
    (local.get $otherSize)
   )
  )
  ;;@ ~lib/string.ts:93:4
  (if
   ;;@ ~lib/string.ts:93:8
   (i32.eq
    (local.get $outSize)
    ;;@ ~lib/string.ts:93:19
    (i32.const 0)
   )
   ;;@ ~lib/string.ts:93:22
   (return
    ;;@ ~lib/string.ts:93:29
    (i32.const 2176)
   )
  )
  ;;@ ~lib/string.ts:94:4
  (local.set $out
   ;;@ ~lib/string.ts:94:14
   (call $~lib/rt/stub/__new
    ;;@ ~lib/string.ts:94:39
    (local.get $outSize)
    ;;@ ~lib/string.ts:94:48
    (i32.const 2)
   )
  )
  ;;@ ~lib/string.ts:95:4
  (call $~lib/memory/memory.copy
   ;;@ ~lib/string.ts:95:16
   (local.get $out)
   ;;@ ~lib/string.ts:95:40
   (local.get $this)
   ;;@ ~lib/string.ts:95:65
   (local.get $thisSize)
  )
  ;;@ ~lib/string.ts:96:4
  (call $~lib/memory/memory.copy
   ;;@ ~lib/string.ts:96:16
   (i32.add
    (local.get $out)
    ;;@ ~lib/string.ts:96:41
    (local.get $thisSize)
   )
   ;;@ ~lib/string.ts:96:51
   (local.get $other)
   ;;@ ~lib/string.ts:96:77
   (local.get $otherSize)
  )
  ;;@ ~lib/string.ts:97:4
  (return
   ;;@ ~lib/string.ts:97:11
   (local.get $out)
  )
 )
 (func $~lib/string/String.__concat (param $left i32) (param $right i32) (result i32)
  ;;@ ~lib/string.ts:86:4
  (return
   ;;@ ~lib/string.ts:86:11
   (call $~lib/string/String#concat
    (local.get $left)
    ;;@ ~lib/string.ts:86:23
    (local.get $right)
   )
  )
 )
 (func $main/ascii/main
  (local $byte i32)
  ;;@ main/ascii.ts:4:4
  (call $~lib/as-warduino/assembly/index/print
   ;;@ main/ascii.ts:4:10
   (i32.const 32)
  )
  ;;@ main/ascii.ts:5:4
  (local.set $byte
   ;;@ main/ascii.ts:5:20
   (i32.const 33)
  )
  ;;@ main/ascii.ts:7:4
  (block $while-break|0
   (loop $while-continue|0
    (if
     ;;@ main/ascii.ts:7:11
     (i32.ne
      (local.get $byte)
      ;;@ main/ascii.ts:7:20
      (i32.const 126)
     )
     (block
      ;;@ main/ascii.ts:8:8
      (call $~lib/as-warduino/assembly/index/print
       ;;@ main/ascii.ts:8:14
       (call $~lib/string/String.fromCharCode@varargs
        ;;@ main/ascii.ts:8:34
        (local.get $byte)
        (block (result i32)
         (global.set $~argumentsLength
          (i32.const 1)
         )
         (i32.const 0)
        )
       )
      )
      ;;@ main/ascii.ts:10:8
      (call $~lib/as-warduino/assembly/index/print
       ;;@ main/ascii.ts:10:14
       (call $~lib/string/String.__concat
        (i32.const 352)
        ;;@ main/ascii.ts:10:26
        (call $~lib/number/I32#toString
         (local.get $byte)
         ;;@ ~lib/number.ts:78:35
         (i32.const 10)
        )
       )
      )
      ;;@ main/ascii.ts:11:8
      (call $~lib/as-warduino/assembly/index/print
       ;;@ main/ascii.ts:11:14
       (call $~lib/string/String.__concat
        (i32.const 2208)
        ;;@ main/ascii.ts:11:26
        (call $~lib/number/I32#toString
         (local.get $byte)
         ;;@ main/ascii.ts:11:40
         (i32.const 16)
        )
       )
      )
      ;;@ main/ascii.ts:12:8
      (call $~lib/as-warduino/assembly/index/print
       ;;@ main/ascii.ts:12:14
       (call $~lib/string/String.__concat
        (i32.const 2256)
        ;;@ main/ascii.ts:12:26
        (call $~lib/number/I32#toString
         (local.get $byte)
         ;;@ main/ascii.ts:12:40
         (i32.const 8)
        )
       )
      )
      ;;@ main/ascii.ts:13:8
      (call $~lib/as-warduino/assembly/index/print
       ;;@ main/ascii.ts:13:14
       (call $~lib/string/String.__concat
        (call $~lib/string/String.__concat
         (i32.const 2304)
         ;;@ main/ascii.ts:13:26
         (call $~lib/number/I32#toString
          (local.get $byte)
          ;;@ main/ascii.ts:13:40
          (i32.const 2)
         )
        )
        ;;@ main/ascii.ts:13:45
        (i32.const 2352)
       )
      )
      ;;@ main/ascii.ts:15:8
      (if
       ;;@ main/ascii.ts:15:12
       (i32.eq
        (local.get $byte)
        ;;@ main/ascii.ts:15:20
        (i32.const 126)
       )
       ;;@ main/ascii.ts:16:10
       (block $while-break|1
        (loop $while-continue|1
         (if
          ;;@ main/ascii.ts:16:17
          (i32.const 1)
          (block
           ;;@ main/ascii.ts:17:12
           (br $while-continue|1)
           (br $while-continue|1)
          )
         )
        )
        (unreachable)
       )
      )
      ;;@ main/ascii.ts:21:8
      (local.set $byte
       (i32.add
        (local.get $byte)
        (i32.const 1)
       )
      )
      (br $while-continue|0)
     )
    )
   )
  )
 )
 (func $~start
  (global.set $~lib/rt/stub/startOffset
   (i32.sub
    (i32.and
     (i32.add
      (i32.add
       (global.get $~lib/memory/__heap_base)
       (i32.const 4)
      )
      (i32.const 15)
     )
     (i32.xor
      (i32.const 15)
      (i32.const -1)
     )
    )
    (i32.const 4)
   )
  )
  (global.set $~lib/rt/stub/offset
   (global.get $~lib/rt/stub/startOffset)
  )
 )
)
