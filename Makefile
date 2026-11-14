# Makefile for Papwick firmware
# Wraps PlatformIO commands for convenience

.PHONY: all build upload flash clean size erase monitor format check compiledb \
        backup restore test test-build test-run test-clean help

# Prefer the project environment when it provides PlatformIO.
ifneq ($(wildcard $(CURDIR)/.venv/bin/pio),)
export PATH := $(CURDIR)/.venv/bin:$(PATH)
endif

ENV ?= fitness
PORT ?= /dev/ttyACM0

# Default target
all: help

## Firmware:

build: ## Build firmware (ENV=fitness|probe)
	pio run -e $(ENV)

upload: ## Build and flash to device (ENV=..., PORT=...)
	pio run -e $(ENV) --target upload --upload-port $(PORT)

flash: upload ## Alias for upload

clean: ## Clean build artifacts
	pio run -e $(ENV) --target clean

size: ## Show firmware size
	pio run -e $(ENV) --target size

erase: ## Erase device flash, including the saved settings and app data
	pio run -e $(ENV) --target erase --upload-port $(PORT)

monitor: ## Open serial monitor
	pio device monitor --port $(PORT) --dtr 0 --rts 0

## Code quality:

format: ## Format code with clang-format
	clang-format -i $$(find src lib examples test -path test/build -prune -o \( -name '*.cpp' -o -name '*.h' \) -print)

check: ## Run static analysis (cppcheck)
	pio check -e $(ENV)

compiledb: ## Generate compile_commands.json for clangd
	pio run -e $(ENV) --target compiledb

## Stock firmware:

backup: ## Read the full 32 MB flash into backup/ and verify it
	scripts/backup_flash.sh $(PORT)

restore: ## Write the stock firmware backup back to the device
	cd backup && sha256sum -c SHA256SUMS
	esptool --chip esp32s3 -p $(PORT) write-flash 0 backup/triviapod_original_32MB.bin

## Unit Tests:

test: test-build test-run ## Build and run C++ unit tests

test-build: ## Build unit tests
	@mkdir -p test/build
	@cd test/build && cmake .. -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build . --parallel $(shell nproc | awk '{print ($$1 > 1 ? int($$1 / 2) : 1)}')

test-run: ## Run unit tests (builds if needed)
	@if [ ! -d test/build/bin ]; then $(MAKE) test-build; fi
	@test/scripts/run_tests.sh

test-clean: ## Clean test build artifacts
	@rm -rf test/build

## Help:

help: ## Show this help
	@echo "Papwick - Build System"
	@echo ""
	@echo "Usage: make [target] [ENV=fitness] [PORT=/dev/ttyACM0]"
	@awk 'BEGIN {FS = ":.*##"; section=""} \
		/^##/ { section=substr($$0, 4); next } \
		/^[a-zA-Z0-9_-]+:.*##/ { \
			if (section != "") { printf "\n\033[1m%s\033[0m\n", section; section="" } \
			printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 \
		}' $(MAKEFILE_LIST)
	@echo ""
