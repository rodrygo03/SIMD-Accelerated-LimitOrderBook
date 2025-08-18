# Test Suite

Unit tests for validating core engine components and functionality.

## Test Files

- **`test_bitset_directory.cpp`**: SIMD bitset directory operations and consistency
- **`test_order_book.cpp`**: Order book functionality across configurations  
- **`test_lob_engine.cpp`**: Message processing and engine integration
- **`run_all_tests.cpp`**: Test runner executing all validation suites

## Running Tests

Execute all tests:
```bash
make run_all_tests && ./run_all_tests
```

Individual test execution available through build system targets.

Tests validate correctness across all seven optimization configurations to ensure consistent behavior regardless of performance optimizations applied.