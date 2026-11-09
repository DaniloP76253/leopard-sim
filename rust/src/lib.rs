use godot::prelude::*;

pub mod godot_nodes;
pub mod sim;

struct LeopardSimExtension;

#[gdextension]
unsafe impl ExtensionLibrary for LeopardSimExtension {}
