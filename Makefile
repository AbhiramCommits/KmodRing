# SPDX-License-Identifier: GPL-2.0
all: module user

module:
	$(MAKE) -C kernel

user:
	$(MAKE) -C user

kunit:
	@echo "KUnit not yet implemented"

selftest:
	@echo "Selftests not yet implemented"

bench:
	@echo "Benchmarks not yet implemented"

kasan:
	@echo "KASAN test not yet implemented"

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C user clean
	rm -rf results/*.txt results/*.csv results/*.json results/*.md
