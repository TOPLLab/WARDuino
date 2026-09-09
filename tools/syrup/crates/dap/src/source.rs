//! Immutable WAT images and the small source map needed by the limited DAP adapter.

use std::{fs, path::Path};

use wasmparser::{Parser, Payload, TypeRef};
use wast::{
    Wat,
    core::{FuncKind, ModuleField},
    parser::{self, ParseBuffer},
};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SourceLocation {
    pub line: u32,
    pub column: u32,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MappedFrame {
    pub function: String,
    pub location: SourceLocation,
}

#[derive(Clone, Debug)]
struct PcLocation {
    start: u32,
    end: u32,
    location: SourceLocation,
}

#[derive(Clone, Debug)]
struct FunctionRange {
    index: u32,
    name: String,
    start: u32,
    end: u32,
}

/// The exact text and binary sent to the VM during attach.
#[derive(Clone, Debug)]
pub struct ProgramImage {
    source_name: String,
    source: String,
    wasm: Vec<u8>,
    functions: Vec<FunctionRange>,
    locations: Vec<PcLocation>,
}

impl ProgramImage {
    pub fn from_path(path: &Path) -> Result<Self, String> {
        let source = fs::read_to_string(path)
            .map_err(|error| format!("cannot read WAT program {}: {error}", path.display()))?;
        let name = path
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or("module.wat")
            .to_owned();
        Self::from_wat(name, source)
    }

    pub fn from_wat(source_name: impl Into<String>, source: String) -> Result<Self, String> {
        let mut buffer = ParseBuffer::new(&source)
            .map_err(|error| format!("cannot parse WAT source: {error}"))?;
        buffer.track_instr_spans(true);
        let mut wat: Wat<'_> =
            parser::parse(&buffer).map_err(|error| format!("cannot parse WAT module: {error}"))?;
        let wasm = wat
            .encode()
            .map_err(|error| format!("cannot compile WAT module: {error}"))?;
        let Wat::Module(module) = &wat else {
            return Err("WAT components are not supported by WARDuino".into());
        };

        let mut imported_function_count = 0_u32;
        for payload in Parser::new(0).parse_all(&wasm) {
            let payload =
                payload.map_err(|error| format!("cannot parse generated WASM: {error}"))?;
            let Payload::ImportSection(imports) = payload else {
                continue;
            };
            for import in imports {
                let import = import
                    .map_err(|error| format!("cannot read generated WASM import: {error}"))?;
                for import in import {
                    let (_offset, import) = import
                        .map_err(|error| format!("cannot read generated WASM import: {error}"))?;
                    if matches!(import.ty, TypeRef::Func(_) | TypeRef::FuncExact(_)) {
                        imported_function_count += 1;
                    }
                }
            }
        }

        let mut source_functions = Vec::new();
        if let wast::core::ModuleKind::Text(fields) = &module.kind {
            let mut index = imported_function_count;
            for field in fields {
                let ModuleField::Func(function) = field else {
                    continue;
                };
                let name = function
                    .name
                    .map(|name| name.name.to_owned())
                    .or_else(|| function.id.map(|id| id.name().to_owned()))
                    .unwrap_or_else(|| format!("func[{index}]"));
                if let FuncKind::Inline { expression, .. } = &function.kind {
                    let spans = expression
                        .instr_spans
                        .as_deref()
                        .ok_or_else(|| "WAT parser did not retain instruction spans".to_owned())?;
                    source_functions.push((index, name, function.span, spans.to_vec()));
                }
                index += 1;
            }
        }

        let mut functions = Vec::with_capacity(source_functions.len());
        let mut locations = Vec::new();
        let mut body_index = 0_usize;
        for payload in Parser::new(0).parse_all(&wasm) {
            let payload =
                payload.map_err(|error| format!("cannot parse generated WASM: {error}"))?;
            let Payload::CodeSectionEntry(body) = payload else {
                continue;
            };
            let (_index, name, function_span, spans) = source_functions
                .get(body_index)
                .ok_or_else(|| "generated WASM has more function bodies than WAT".to_owned())?;
            body_index += 1;
            let mut reader = body
                .get_operators_reader()
                .map_err(|error| format!("cannot read generated WASM operators: {error}"))?;
            let mut offsets = Vec::new();
            while !reader.eof() {
                let offset = reader.original_position() as u32;
                reader
                    .read()
                    .map_err(|error| format!("cannot read generated WASM operator: {error}"))?;
                offsets.push(offset);
            }
            let start = offsets
                .first()
                .copied()
                .unwrap_or(body.range().start as u32);
            let end = body.range().end as u32;
            functions.push(FunctionRange {
                index: *_index,
                name: name.clone(),
                start,
                end,
            });
            for (operator_index, start) in offsets.into_iter().enumerate() {
                let span = spans.get(operator_index).copied().unwrap_or(*function_span);
                let (line, column) = span.linecol_in(&source);
                locations.push(PcLocation {
                    start,
                    end,
                    location: SourceLocation {
                        line: line as u32 + 1,
                        column: column as u32 + 1,
                    },
                });
            }
        }
        if body_index != source_functions.len() {
            return Err("generated WASM has fewer function bodies than WAT".into());
        }
        locations.sort_by_key(|location| location.start);
        for index in 0..locations.len().saturating_sub(1) {
            locations[index].end = locations[index + 1].start;
        }
        for location in &mut locations {
            if location.end == 0 {
                location.end = location.start.saturating_add(1);
            }
        }

        Ok(Self {
            source_name: source_name.into(),
            source,
            wasm,
            functions,
            locations,
        })
    }

    pub fn wasm(&self) -> &[u8] {
        &self.wasm
    }

    pub fn source(&self) -> &str {
        &self.source
    }

    pub fn source_name(&self) -> &str {
        &self.source_name
    }

    pub fn instruction_pcs(&self) -> impl Iterator<Item = u32> + '_ {
        self.locations.iter().map(|location| location.start)
    }

    pub fn frame_at(&self, pc: u32) -> Option<MappedFrame> {
        let function = self
            .functions
            .iter()
            .find(|function| function.start <= pc && pc < function.end)?;
        let location = self
            .locations
            .iter()
            .find(|location| location.start <= pc && pc < location.end)?
            .location
            .clone();
        Some(MappedFrame {
            function: function.name.clone(),
            location,
        })
    }

    /// Looks up the WAT name for a VM function index when a return address has
    /// no source location to map.
    pub fn function_name(&self, index: u32) -> Option<&str> {
        self.functions
            .iter()
            .find(|function| function.index == index)
            .map(|function| function.name.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn function_name_fallback_uses_wat_function_indexes() {
        let image = ProgramImage::from_wat(
            "callstack.wat",
            include_str!("../tests/fixtures/callstack.wat").into(),
        )
        .unwrap();
        assert_eq!(image.function_name(0), Some("callee"));
        assert_eq!(image.function_name(1), Some("caller"));
        assert_eq!(image.function_name(2), None);
    }

    #[test]
    fn function_name_fallback_accounts_for_imported_functions() {
        let image = ProgramImage::from_wat(
            "imports.wat",
            "(module\n  (import \"env\" \"one\" (func))\n  (import \"env\" \"two\" (func))\n  (func $fac)\n  (func $main))\n".into(),
        )
        .unwrap();
        assert_eq!(image.function_name(0), None);
        assert_eq!(image.function_name(1), None);
        assert_eq!(image.function_name(2), Some("fac"));
        assert_eq!(image.function_name(3), Some("main"));
    }

    #[test]
    fn maps_operator_offsets_to_named_wat_source_lines() {
        let image = ProgramImage::from_wat(
            "example.wat",
            "(module\n  (func $sum (result i32)\n    i32.const 1\n    i32.const 2\n    i32.add))\n"
                .into(),
        )
        .unwrap();
        let sum = image
            .functions
            .iter()
            .find(|function| function.name == "sum")
            .unwrap();
        let first = image.frame_at(sum.start).unwrap();
        assert_eq!(first.function, "sum");
        assert_eq!(first.location.line, 3);
        assert_eq!(image.wasm()[0..4], [0, 0x61, 0x73, 0x6d]);
    }
}
