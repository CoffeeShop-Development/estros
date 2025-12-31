all:
	$(MAKE) -C isa $@

build:
	$(MAKE) -C isa $@

test:
	$(MAKE) -C isa $@

clean:
	$(MAKE) -C isa $@

.PHONY: all build test clean