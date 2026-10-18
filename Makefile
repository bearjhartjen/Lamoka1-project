# Lamoka1 project — common development tasks.
#
# Run `make` or `make help` for the target list. `make test` and `make lint`
# use the Python environment pinned in pyproject.toml (`uv sync` once to create
# .venv/); the other targets check for their tool and stop with an explanatory
# message if it is missing.

SHELL := /bin/bash

# Optional local overrides: copy .env.example to .env to set PORT, SKETCH,
# FQBN, or BUILD_DIR without typing them on every command.
-include .env

# --- Configuration -----------------------------------------------------------

# Current PIE firmware revision. Point this at another revision when needed:
#   make firmware SKETCH="SOFTWARE/Portal Interface Engine/V1.7.ino"
SKETCH ?= SOFTWARE/Portal Interface Engine/V1.8.ino
# Shell-expanded so the space in "Portal Interface Engine" survives.
SKETCH_NAME := $(shell basename "$(SKETCH)" .ino)

# The DEV-Brain's RP2354A builds as a Generic RP2350. If your Arduino IDE build
# uses extra board options (flash layout, CPU speed, ...), append them here,
# e.g. rp2040:rp2040:generic_rp2350:flash=2097152_1048576,freq=150
FQBN ?= rp2040:rp2040:generic_rp2350

# Serial port used by `make upload`.
PORT ?= /dev/ttyACM0

# Scratch directory for staged sketches and exported binaries (gitignored).
BUILD_DIR ?= build

# Tool commands; override to point at a specific installation.
ARDUINO_CLI ?= arduino-cli
KICAD_CLI ?= kicad-cli
MAGICK ?= magick
CPPCHECK ?= cppcheck
PYTHON ?= python3
PYTEST ?= $(PYTHON) -m pytest
RUFF ?= ruff

BOARD_DIR := HARDWARE/Kicadfiles/Lamoka1-half
DEV_BRAIN_DIR := Lamoka1-DEV-Brain/Kicad files
MECHANICS_DIR := HARDWARE/Mechanical
SKETCH_DIR := $(BUILD_DIR)/$(SKETCH_NAME)

.PHONY: help firmware upload test lint check erc drc static step images clean

help:
	@echo "Lamoka1 development tasks"
	@echo ""
	@echo "  make firmware   Compile the current PIE sketch (SKETCH=... to pick a revision)"
	@echo "  make upload     Compile and flash to a connected DEV-Brain (PORT=... to pick a port)"
	@echo "  make test       Run the repository checks (pytest; 'uv sync' first)"
	@echo "  make lint       Run ruff check and format check over the Python tooling"
	@echo "  make check      Run 'test' plus the KiCad ERC and DRC reports"
	@echo "  make static     Run cppcheck over the current PIE sketch"
	@echo "  make erc        Regenerate the schematic ERC report"
	@echo "  make drc        Regenerate the PCB DRC report (with schematic parity)"
	@echo "  make step       Regenerate the board STEP models in HARDWARE/Mechanical/"
	@echo "  make images     Regenerate the board images in Images/ (3D renders + routing plot)"
	@echo "  make clean      Remove build/ (staged sketches and exported binaries)"

# --- Firmware ----------------------------------------------------------------

# Arduino requires each sketch to live in a folder of the same name, and it
# compiles every .ino in that folder together. The PIE revisions deliberately
# share one folder, so stage the chosen revision as its own single-file sketch
# under build/ before compiling.
firmware:
	@test -f "$(SKETCH)" || { echo "make firmware: no such sketch: $(SKETCH)"; exit 1; }
	@mkdir -p "$(SKETCH_DIR)"
	@cp "$(SKETCH)" "$(SKETCH_DIR)/$(SKETCH_NAME).ino"
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" --warnings all \
	  --export-binaries "$(SKETCH_DIR)"
	@echo ""
	@echo "Exported binaries:"
	@find "$(SKETCH_DIR)/build" \( -name "*.uf2" -o -name "*.bin" -o -name "*.elf" \) 2>/dev/null | sed 's/^/  /'

upload: firmware
	$(ARDUINO_CLI) upload --fqbn "$(FQBN)" -p "$(PORT)" "$(SKETCH_DIR)"

# --- Checks ------------------------------------------------------------------

test:
	@$(PYTEST) --version >/dev/null 2>&1 || { echo "make test: pytest is not installed — run 'uv sync' first (see CONTRIBUTING.md), or install pytest with pip."; exit 1; }
	$(PYTEST) -v

lint:
	@$(RUFF) --version >/dev/null 2>&1 || { echo "make lint: ruff is not installed — run 'uv sync' first (see CONTRIBUTING.md), or install ruff with pip."; exit 1; }
	$(RUFF) check .
	$(RUFF) format --check .

check: test erc drc

erc:
	@command -v $(KICAD_CLI) >/dev/null 2>&1 || { echo "make erc: kicad-cli not found — install KiCad 10, or run 'make test' alone."; exit 1; }
	$(KICAD_CLI) sch erc --output "$(BOARD_DIR)/ERC.rpt" "$(BOARD_DIR)/Lamoka1-half.kicad_sch"
	@echo "Wrote $(BOARD_DIR)/ERC.rpt"

drc:
	@command -v $(KICAD_CLI) >/dev/null 2>&1 || { echo "make drc: kicad-cli not found — install KiCad 10, or run 'make test' alone."; exit 1; }
	$(KICAD_CLI) pcb drc --schematic-parity \
	  --output "$(BOARD_DIR)/DRC.rpt" "$(BOARD_DIR)/Lamoka1-half.kicad_pcb"
	@echo "Wrote $(BOARD_DIR)/DRC.rpt"

static:
	@command -v $(CPPCHECK) >/dev/null 2>&1 || { echo "make static: cppcheck not found — install cppcheck (CI runs this check too)."; exit 1; }
	$(CPPCHECK) --quiet --error-exitcode=1 --inline-suppr \
	  --enable=warning,performance,portability \
	  --language=c++ --std=c++17 \
	  --suppress=missingInclude --suppress=missingIncludeSystem \
	  --suppress=uninitMemberVarNoCtor \
	  "$(SKETCH)"
# The uninitMemberVarNoCtor suppression covers ModeFire::heat[] in PIE V1.8:
# enter() memset()s the array before any read, so the finding is a robustness
# note, not an exploitable defect (see CONTRIBUTING.md, "Static analysis").
# Drop the suppression when the next revision initializes the member in-class.

# --- Exports -----------------------------------------------------------------

step:
	@command -v $(KICAD_CLI) >/dev/null 2>&1 || { echo "make step: kicad-cli not found — install KiCad 10."; exit 1; }
	$(KICAD_CLI) pcb export step --force \
	  --output "$(MECHANICS_DIR)/Lamoka1-half.step" "$(BOARD_DIR)/Lamoka1-half.kicad_pcb"
	$(KICAD_CLI) pcb export step --force \
	  --output "$(MECHANICS_DIR)/Lamoka1-DEV-Brain.step" "$(DEV_BRAIN_DIR)/Lamoka1-DEV-board.kicad_pcb"
	@echo "Wrote $(MECHANICS_DIR)/Lamoka1-half.step and Lamoka1-DEV-Brain.step"

# The board pictures in Images/ (README renders and the routing plot). KiCad
# renders them; ImageMagick trims the transparent border and rasterises the
# routing plot. The DEV-Brain renders need that board's local 3D models and are
# skipped with a message when those are absent. See CONTRIBUTING.md, "Board
# images".
images:
	KICAD_CLI="$(KICAD_CLI)" MAGICK="$(MAGICK)" tools/make_images.sh

clean:
	rm -rf "$(BUILD_DIR)"
