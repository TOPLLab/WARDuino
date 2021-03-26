# WARDuino integration tests

This folder contains all the integration tests for the WARDuino virtual machine.

## Core spec tests

The `core` folder contains the list of official WebAssembly core specification tests.
The following list keeps track of which tests succeed and which fail:

- [x] address.wast
- [ ] align.wast
- [ ] binary-leb128.wast
- [ ] binary.wast
- [ ] block.wast
- [ ] br.wast
- [ ] br_if.wast
- [ ] br_table.wast
- [ ] call.wast
- [ ] call_indirect.wast
- [ ] comments.wast
- [ ] const.wast
- [ ] conversions.wast
- [ ] custom.wast
- [ ] data.wast
- [ ] elem.wast
- [ ] endianness.wast
- [ ] exports.wast
- [ ] f32.wast
- [ ] f32_bitwise.wast
- [ ] f32_cmp.wast
- [ ] f64.wast
- [ ] f64_bitwise.wast
- [ ] f64_cmp.wast
- [ ] fac.wast   **(fails: [Multi-value](https://github.com/WebAssembly/multi-value) not supported)**
- [ ] float_exprs.wast
- [ ] float_literals.wast
- [ ] float_memory.wast
- [ ] float_misc.wast
- [ ] forward.wast
- [ ] func.wast
- [ ] func_ptrs.wast
- [ ] global.wast
- [ ] i32.wast   **(fails: [Sign-extension operators](https://github.com/WebAssembly/sign-extension-ops) not supported)**
- [ ] i64.wast
- [ ] if.wast
- [ ] imports.wast
- [ ] inline-module.wast
- [ ] int_exprs.wast
- [ ] int_literals.wast
- [ ] labels.wast
- [ ] left-to-right.wast
- [ ] linking.wast
- [ ] load.wast
- [ ] local_get.wast
- [ ] local_set.wast
- [ ] local_tee.wast
- [ ] loop.wast
- [ ] memory.wast
- [ ] memory_grow.wast
- [ ] memory_redundancy.wast
- [ ] memory_size.wast
- [ ] memory_trap.wast
- [ ] names.wast
- [ ] nop.wast
- [ ] return.wast
- [ ] select.wast
- [ ] skip-stack-guard-page.wast
- [ ] stack.wast
- [ ] start.wast
- [ ] store.wast
- [ ] switch.wast
- [ ] table.wast
- [ ] token.wast
- [ ] traps.wast
- [ ] type.wast
- [ ] unreachable.wast
- [ ] unreached-invalid.wast
- [ ] unwind.wast
- [ ] utf8-custom-section-id.wast
- [ ] utf8-import-field.wast
- [ ] utf8-import-module.wast
- [ ] utf8-invalid-encoding.wast
