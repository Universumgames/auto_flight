OPENSCAD ?= $(shell command -v openscad 2>/dev/null || echo "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD")
BUILD_DIR = build

.PHONY: all base lid plane_mount clean

all: base lid plane_mount

base: $(BUILD_DIR)/base_station_case_base.stl

lid: $(BUILD_DIR)/base_station_case_lid.stl

plane_mount: $(BUILD_DIR)/plane_mount_plate.stl

$(BUILD_DIR)/base_station_case_base.stl: base_station_case.scad base_station_config.scad | $(BUILD_DIR)
	$(OPENSCAD) -D 'part="base"' -o $@ base_station_case.scad

$(BUILD_DIR)/base_station_case_lid.stl: base_station_case.scad base_station_config.scad | $(BUILD_DIR)
	$(OPENSCAD) -D 'part="lid"' -o $@ base_station_case.scad

$(BUILD_DIR)/plane_mount_plate.stl: plane_mount_plate.scad plane_mount_config.scad | $(BUILD_DIR)
	$(OPENSCAD) -o $@ plane_mount_plate.scad

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
