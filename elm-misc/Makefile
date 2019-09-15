all:
	for t in $(TARGET); do \
		cd $$t && make $@ TARGET=$$t && cd -; \
	done

install:
	for t in $(TARGET); do \
		cd $$t && make $@ TARGET=$$t && cd -; \
	done

uninstall:
	for t in $(TARGET); do \
		cd $$t && make $@ TARGET=$$t && cd -; \
	done

clean:
	for t in $(TARGET); do \
		cd $$t && make $@ TARGET=$$t && cd -; \
	done

distclean: clean

.PHONY:
	$(TARGET)
