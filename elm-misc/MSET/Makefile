all:
	for v in $(VERSION); do \
		cd $$v && make $@ VERSION=$$v && cd -; \
	done

install:
	for v in $(VERSION); do \
		cd $$v && make $@ VERSION=$$v && cd -; \
	done

uninstall:
	for v in $(VERSION); do \
		cd $$v && make $@ VERSION=$$v && cd -; \
	done

clean:
	for v in $(VERSION); do \
		cd $$v && make $@ VERSION=$$v && cd -; \
	done

distclean: clean

.PHONY:
	$(TARGET)
