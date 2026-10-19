# Automatically find all YAML files and exclude special/secrets files
ALL_YAML := $(wildcard *.yaml)
EXCLUDE_YAML := secrets.yaml btproxy.yaml
DEVICE_YAML := $(filter-out $(EXCLUDE_YAML), $(ALL_YAML))
DEVICES := $(DEVICE_YAML:.yaml=)

BTPROXY_REPLICAS := c25bac c2577c c25b74 c257a8

STAMP_DIR := .esphome/stamps
BTPROXY_FLASH_TARGETS := $(addprefix $(STAMP_DIR)/.stamp-flash-btproxy-, $(BTPROXY_REPLICAS))
BTPROXY_COMPILE_TARGETS := $(addprefix $(STAMP_DIR)/.stamp-compile-btproxy-, $(BTPROXY_REPLICAS))
MAKEFLAGS += --keep-going
FLASH_TARGETS := $(addprefix $(STAMP_DIR)/.stamp-flash-, $(DEVICES)) $(BTPROXY_FLASH_TARGETS)
COMPILE_TARGETS := $(addprefix $(STAMP_DIR)/.stamp-compile-, $(DEVICES)) $(BTPROXY_COMPILE_TARGETS)

ESPHOME_BIN := $(shell command -v esphome)
$(shell mkdir -p $(STAMP_DIR) && echo "$(ESPHOME_BIN)" > $(STAMP_DIR)/.esphome-bin.tmp && cmp -s $(STAMP_DIR)/.esphome-bin.tmp $(STAMP_DIR)/.esphome-bin || mv $(STAMP_DIR)/.esphome-bin.tmp $(STAMP_DIR)/.esphome-bin; rm -f $(STAMP_DIR)/.esphome-bin.tmp)
all: compile-all

flash-all: $(FLASH_TARGETS)
compile-all: $(COMPILE_TARGETS)
flash-btproxies: $(BTPROXY_FLASH_TARGETS)
compile-btproxies: $(BTPROXY_COMPILE_TARGETS)

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
# btproxy.yaml hardcodes `name: btproxy` plus `name_add_mac_suffix: true`, so a
# replica is addressed by its own MAC suffix (the BTPROXY_REPLICAS ids).
# `--device OTA` cannot pick one: it mDNS-browses every `btproxy-<mac>.local` and
# would flash all proxies at once, or none where mDNS is not reachable.
$(STAMP_DIR)/.stamp-flash-btproxy-%: btproxy.yaml $(STAMP_DIR)/.esphome-bin
	@mkdir -p $(STAMP_DIR)
	esphome run --no-logs --device btproxy-$* $<
	@touch $@

# Include generated dependencies
-include .Makefile.deps

# Rule to update dependencies
.Makefile.deps: $(wildcard *.yaml) $(wildcard templates/*.yaml) generate_deps.py
	@echo "Updating dependencies..."
	@python3 generate_deps.py "$(DEVICES)" "$(BTPROXY_REPLICAS)" > $@

.PHONY: all flash-all compile-all flash-btproxies compile-btproxies clean
