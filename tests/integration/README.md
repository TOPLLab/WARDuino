# WARDuino integration tests

This folder contains all the integration tests for the WARDuino virtual machine.

## Core spec tests

The `core` folder contains the list of official WebAssembly core specification tests.
The following list keeps track of which tests succeed and which fail:

- [ ] address.wast
- [ ] call.wast
- [ ] f32\_bitwise.wast
- [ ] float\_memory.wast
- [ ] imports.wast
- [ ] local\_set.wast
- [ ] nop.wast
- [ ] token.wast
- [ ] utf8-invalid-encoding.wast
- [ ] align.wast
- [ ] comments.wast
- [ ] f32\_cmp.wast
- [ ] float\_misc.wast
- [ ] inline-module.wast
- [ ] local\_tee.wast
- [ ] return.wast
- [ ] traps.wast
- [ ] binary-leb128.wast
- [ ] const.wastf32.wast
- [ ] forward.wast
- [ ] int\_exprs.wast
- [ ] loop.wast
- [ ] select.wast
- [ ] type.wast
- [ ] binary.wast
- [ ] conversions.wast
- [ ] f64\_bitwise.wast
- [ ] func\_ptrs.wast
- [ ] int\_literals.wast
- [ ] memory\_grow.wast
- [ ] skip-stack-guard-page.wast
- [ ] unreachable.wast
- [ ] block.wast
- [ ] custom.wast
- [ ] f64\_cmp.wast
- [ ] func.wast
- [ ] labels.wast
- [ ] memory\_redundancy.wast
- [ ] stack.wast
- [ ] unreached-invalid.wast
- [ ] br\_if.wast
- [ ] data.wast
- [ ] f64.wast
- [ ] global.wast
- [ ] left-to-right.wast
- [ ] memory\_size.wast
- [ ] start.wast
- [ ] unwind.wast
- [ ] br\_table.wast
- [ ] elem.wast
- [ ] fac.wast   **(fails: [Multi-value](https://github.com/WebAssembly/multi-value) not supported)**
- [ ] i32.wast   **(fails: [Sign-extension operators](https://github.com/WebAssembly/sign-extension-ops) not supported)**
- [ ] linking.wast
- [ ] memory\_trap.wast
- [ ] store.wast
- [ ] utf8-custom-section-id.wast
- [ ] br.wast
- [ ] endianness.wast
- [ ] float\_exprs.wast
- [ ] i64.wast
- [ ] load.wast
- [ ] memory.wast
- [ ] switch.wast
- [ ] utf8-import-field.wast
- [ ] call\_indirect.wast
- [ ] exports.wast
- [ ] float\_literals.wast
- [ ] if.wast
- [ ] local\_get.wast
- [ ] names.wast
- [ ] table.wast
- [ ] utf8-import-module.wast

