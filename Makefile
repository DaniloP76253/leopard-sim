RUST_DIR = rust
GODOT_DIR = godot
GODOT_BIN_DIR = $(GODOT_DIR)/bin
LIB_NAME = libleopard_sim.so
CARGO = source "$$HOME/.cargo/env" && cd $(RUST_DIR) && cargo

.PHONY: all build-rust test-rust import-godot run-godot editor clean

all: build-rust

build-rust:
	@mkdir -p $(GODOT_BIN_DIR)
	@bash -c '$(CARGO) build'
	@cp $(RUST_DIR)/target/debug/$(LIB_NAME) $(GODOT_BIN_DIR)/$(LIB_NAME)
	@echo "Build complete: $(GODOT_BIN_DIR)/$(LIB_NAME)"

test-rust:
	@bash -c '$(CARGO) test'

import-godot:
	flatpak-spawn --host flatpak run org.godotengine.Godot --headless --editor --path $(CURDIR)/$(GODOT_DIR) --import --quit

run-godot: build-rust import-godot
	flatpak-spawn --host flatpak run org.godotengine.Godot --path $(CURDIR)/$(GODOT_DIR)

editor: build-rust
	flatpak-spawn --host flatpak run org.godotengine.Godot --editor --path $(CURDIR)/$(GODOT_DIR)

clean:
	@bash -c '$(CARGO) clean'
	@rm -rf $(GODOT_BIN_DIR)/$(LIB_NAME)
