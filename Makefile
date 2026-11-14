# Makefile for Papwick firmware
# Wraps PlatformIO commands for convenience

.PHONY: all build upload flash clean size erase monitor log format check compiledb \
        backup restore release tag changelog gh-release test test-build test-run test-clean help

# Prefer the project environment when it provides PlatformIO.
ifneq ($(wildcard $(CURDIR)/.venv/bin/pio),)
export PATH := $(CURDIR)/.venv/bin:$(PATH)
endif

ENV ?= fitness
PORT ?= /dev/ttyACM0
LOG ?= device.log
VERSION ?= $(shell sed -n 's/^version = //p' platformio.ini)
BOOT_APP0 ?= $(HOME)/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin

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

log: ## Append the serial log to LOG (default: device.log), with reconnect
	python scripts/capture_serial.py $(PORT) $(LOG)

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

## Release:

release: ## Build the release images into dist/
	pio run -e fitness
	mkdir -p dist
	cp .pio/build/fitness/firmware.bin dist/papwick-$(VERSION).bin
	esptool --chip esp32s3 merge-bin -o dist/papwick-$(VERSION)-full.bin --flash-mode dio --flash-freq 80m --flash-size 32MB 0x0 .pio/build/fitness/bootloader.bin 0x8000 .pio/build/fitness/partitions.bin 0xe000 $(BOOT_APP0) 0x10000 .pio/build/fitness/firmware.bin

tag: ## Create and push the tag v$(VERSION)
	git tag -a v$(VERSION) -m "v$(VERSION)"
	git push origin v$(VERSION)

changelog: ## Generate CHANGELOG.md from git tags
	@echo "Generating CHANGELOG.md..."
	@echo "" > CHANGELOG.md; \
	previous_tag=0; \
	for current_tag in $$(git tag --sort=-creatordate); do \
		if [ "$$previous_tag" != 0 ]; then \
			tag_date=$$(git log -1 --pretty=format:'%ad' --date=short $${previous_tag}); \
			printf "\n## $${previous_tag} ($${tag_date})\n\n" >> CHANGELOG.md; \
			git log $${current_tag}...$${previous_tag} --pretty=format:'*  %s [[%an](mailto:%ae)]' --reverse | grep -v Merge >> CHANGELOG.md; \
			printf "\n" >> CHANGELOG.md; \
		fi; \
		previous_tag=$${current_tag}; \
	done; \
	if [ "$$previous_tag" != 0 ]; then \
		tag_date=$$(git log -1 --pretty=format:'%ad' --date=short $${previous_tag}); \
		printf "\n## $${previous_tag} ($${tag_date})\n\n" >> CHANGELOG.md; \
		git log $${previous_tag} --pretty=format:'*  %s [[%an](mailto:%ae)]' --reverse | grep -v Merge >> CHANGELOG.md; \
		printf "\n" >> CHANGELOG.md; \
	fi
	@echo "CHANGELOG.md generated successfully."

gh-release: release ## Create the GitHub release v$(VERSION) from the pushed tag (NOTES="..." optional)
ifdef NOTES
	gh release create v$(VERSION) dist/papwick-$(VERSION).bin dist/papwick-$(VERSION)-full.bin \
		--repo bigbag/papwick --verify-tag \
		--title "Papwick v$(VERSION)" \
		--notes "$(NOTES)"
else
	gh release create v$(VERSION) dist/papwick-$(VERSION).bin dist/papwick-$(VERSION)-full.bin \
		--repo bigbag/papwick --verify-tag \
		--title "Papwick v$(VERSION)" \
		--generate-notes
endif

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
