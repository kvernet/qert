# Default target
.DEFAULT_GOAL := help

.PHONY: help \
        dev-install build \
        clang-format clang-format-check clang-tidy test \
        clean ci ci-act coverage

help:  ## Show available commands
	@echo "Available commands:"
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-22s\033[0m %s\n", $$1, $$2}'

# --------------------------------------------------------------------
# Installation
# --------------------------------------------------------------------

dev-install:  ## Install development dependencies
	sudo apt-get update -qq
	sudo apt-get install -y -qq \
		build-essential \
		cmake \
		clang-format-18 \
		clang-tidy-18 \
		lcov \
		ninja-build

# --------------------------------------------------------------------
# Build
# --------------------------------------------------------------------

build:  ## Build with tests in Release mode (PAPI + Eigen enabled)
	cmake -B build -G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DBUILD_TESTING=ON \
		-DENABLE_PAPI=ON \
		-DPAPI_ROOT=$(PAPI_DIR)
	cmake --build build --parallel $$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

build-no-papi:  ## Build without PAPI (Eigen only)
	cmake -B build -G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DBUILD_TESTING=ON
	cmake --build build --parallel $$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

build-debug:  ## Build with tests in Debug mode (PAPI + sanitizers)
	cmake -B build -G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DBUILD_TESTING=ON \
		-DENABLE_PAPI=ON \
		-DPAPI_ROOT=$(PAPI_DIR) \
		-DENABLE_SANITIZERS=ON
	cmake --build build

# --------------------------------------------------------------------
# Quality checks
# --------------------------------------------------------------------

clang-format:  ## Format all source files in-place
	find app core tests -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
		! -name "catch_amalgamated.*" \
		-print0 | xargs -0 clang-format-18 -i

clang-format-check:  ## Check formatting (CI)
	find app core tests -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
		! -name "catch_amalgamated.*" \
		-print0 | xargs -0 clang-format-18 --dry-run --Werror

clang-tidy: build  ## Run static analysis on source files
	find core/src app -name '*.cpp' -print0 | \
		xargs -0 -I {} clang-tidy-18 -p=build {} \
			--header-filter='core/include/.*\.hpp$$' \
			--warnings-as-errors='*'
	find tests -name 'test_*.cpp' -print0 | \
		xargs -0 -I {} clang-tidy-18 -p=build {} \
			--header-filter='core/include/.*\.hpp$$' \
			--warnings-as-errors='*'

test: build  ## Run tests
	ctest --test-dir build --output-on-failure

# --------------------------------------------------------------------
# Coverage
# --------------------------------------------------------------------

coverage:  ## Generate code coverage report
	cmake -B build -G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DBUILD_TESTING=ON \
		-DCMAKE_CXX_FLAGS="-g -O0 --coverage" \
		-DCMAKE_EXE_LINKER_FLAGS="--coverage"
	cmake --build build
	ctest --test-dir build --output-on-failure
	lcov --capture \
		--directory build \
		--output-file coverage.info \
		--ignore-errors unused,empty,gcov,source \
		--rc lcov_branch_coverage=0
	lcov --extract coverage.info \
		"$(CURDIR)/core/src/*.cpp" \
		--output-file coverage_filtered.info \
		--ignore-errors empty
	lcov --list coverage_filtered.info --ignore-errors empty
	@echo "Coverage report: coverage_filtered.info"

# --------------------------------------------------------------------
# Housekeeping
# --------------------------------------------------------------------

clean:  ## Remove build directory
	rm -rf build/

distclean: clean  ## Remove build directory and coverage artifacts
	rm -f coverage.info
	rm -f coverage_filtered.info
	rm -rf coverage-report/

# --------------------------------------------------------------------
# CI
# --------------------------------------------------------------------

ci: clang-format-check clang-tidy test  ## Run all CI checks locally

ci-act:  ## Run CI with act
	act -P ubuntu-latest=catthehacker/ubuntu:full-latest \
		--artifact-server-path /tmp/act-artifacts