;; https://github.com/WebAssembly/testsuite/blob/1b45c074228bc47f5f1c718a0929e7a3a2c5bb21/break-drop.wast

(assert_return (invoke "br"))
(assert_return (invoke "br_if"))
(assert_return (invoke "br_table"))