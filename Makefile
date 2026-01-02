all:
	$(MAKE) -C cc $@
	$(MAKE) -C isa $@

build:
	$(MAKE) -C cc $@
	$(MAKE) -C isa $@

test:
	$(MAKE) -C cc $@
	$(MAKE) -C isa $@

clean:
	$(MAKE) -C cc $@
	$(MAKE) -C isa $@

.PHONY: all build test clean
