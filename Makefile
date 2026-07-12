# Automatically find all YAML files and exclude special/secrets files
ALL_YAML := $(wildcard *.yaml)
EXCLUDE_YAML := secrets.yaml btproxy.yaml
DEVICE_YAML := $(filter-out $(EXCLUDE_YAML), $(ALL_YAML))
DEVICES := $(DEVICE_YAML:.yaml=)

BTPROXY_REPLICAS := c25bac c2577c c25b74

STAMP_DIR := .esphome/stamps
MAKEFLAGS += --keep-going
FLASH_TARGETS := $(addprefix $(STAMP_DIR)/.stamp-flash-, $(DEVICES)) $(addprefix $(STAMP_DIR)/.stamp-flash-btproxy-, $(BTPROXY_REPLICAS))
COMPILE_TARGETS := $(addprefix $(STAMP_DIR)/.stamp-compile-, $(DEVICES)) $(addprefix $(STAMP_DIR)/.stamp-compile-btproxy-, $(BTPROXY_REPLICAS))

ESPHOME_BIN := $(shell command -v esphome)
$(shell mkdir -p $(STAMP_DIR) && echo "$(ESPHOME_BIN)" > $(STAMP_DIR)/.esphome-bin.tmp && cmp -s $(STAMP_DIR)/.esphome-bin.tmp $(STAMP_DIR)/.esphome-bin || mv $(STAMP_DIR)/.esphome-bin.tmp $(STAMP_DIR)/.esphome-bin; rm -f $(STAMP_DIR)/.esphome-bin.tmp)
all: compile-all

flash-all: $(FLASH_TARGETS)
compile-all: $(COMPILE_TARGETS)

clean:
	rm -rf $(STAMP_DIR) .Makefile.deps

# Convenience targets
flash-%: $(STAMP_DIR)/.stamp-flash-%
	@:
compile-%: $(STAMP_DIR)/.stamp-compile-%
	@:

# Normal devices: compile
$(STAMP_DIR)/.stamp-compile-%: %.yaml $(STAMP_DIR)/.esphome-bin
	@mkdir -p $(STAMP_DIR)
	esphome compile $<
	@touch $@

# Normal devices: flash
$(STAMP_DIR)/.stamp-flash-%: %.yaml $(STAMP_DIR)/.esphome-bin
	@mkdir -p $(STAMP_DIR)
	esphome run --no-logs --device OTA $<
	@touch $@

# Special btproxy devices: compile
$(STAMP_DIR)/.stamp-compile-btproxy-%: btproxy.yaml $(STAMP_DIR)/.esphome-bin
	@mkdir -p $(STAMP_DIR)
	esphome -s name btproxy-$* compile $<
	@touch $@

# Special btproxy devices: flash
$(STAMP_DIR)/.stamp-flash-btproxy-%: btproxy.yaml $(STAMP_DIR)/.esphome-bin
	@mkdir -p $(STAMP_DIR)
	esphome -s name btproxy-$* run --no-logs --device OTA $<
	@touch $@

# Include generated dependencies
-include .Makefile.deps

# Rule to update dependencies
.Makefile.deps: $(wildcard *.yaml) $(wildcard templates/*.yaml) generate_deps.py
	@echo "Updating dependencies..."
	@python3 generate_deps.py "$(DEVICES)" "$(BTPROXY_REPLICAS)" > $@

.PHONY: all flash-all compile-all clean
