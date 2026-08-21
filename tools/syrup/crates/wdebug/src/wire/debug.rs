// Generated from src/Debug/debug.proto. Public conversion lives in codec.rs.

#[derive(Clone, PartialEq, ::prost::Message)]
pub struct CodeLocation {
    #[prost(uint32, tag = "1")]
    pub module_index: u32,
    #[prost(uint32, tag = "2")]
    pub program_counter: u32,
}

#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Breakpoint {
    #[prost(message, optional, tag = "1")]
    pub location: Option<CodeLocation>,
}

#[derive(Clone, PartialEq, ::prost::Message)]
pub struct HitBreakpoint {
    #[prost(message, optional, tag = "1")]
    pub location: Option<CodeLocation>,
}

#[derive(Clone, PartialEq, ::prost::Message)]
pub struct ContinueFor {
    #[prost(uint32, tag = "1")]
    pub count: u32,
}

#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Inspect {
    #[prost(bytes = "vec", tag = "1")]
    pub state: Vec<u8>,
}

#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Snapshot {
    #[prost(uint32, tag = "1")]
    pub program_counter: u32,
    #[prost(enumeration = "State", tag = "2")]
    pub state: i32,
    #[prost(uint32, repeated, tag = "3")]
    pub breakpoints: Vec<u32>,
}

#[derive(Clone, PartialEq, ::prost::Message)]
pub struct OperationResult {
    #[prost(int32, tag = "1")]
    pub command: i32,
    #[prost(bool, tag = "2")]
    pub success: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, ::prost::Enumeration)]
#[repr(i32)]
pub enum State {
    Running = 0,
    Paused = 1,
    Stepping = 2,
    ProxyRunning = 3,
    ProxyHalted = 4,
}
