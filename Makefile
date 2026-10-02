# SPDX-License-Identifier: GPL-2.0
all: module user

module:
	$(MAKE) -C kernel

user:
	$(MAKE) -C user

kunit:
	$(CC) -std=c11 -Wall -Wextra -Werror tests/kunit/kmodring_ring_test.c -o tests/kunit/kmodring_ring_test
	./tests/kunit/kmodring_ring_test

selftest:
	bash tests/selftests/run_tests.sh

bench:
	bash scripts/run_bench.sh
	bash scripts/collect_results.sh

kasan:
	bash scripts/run_kasan.sh

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C user clean
	rm -rf results/*.txt results/*.csv results/*.json results/*.md
