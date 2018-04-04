obj := main.o

CURL_CONFIG := /home/menxiao/work/libdash-kpush/curlbin/bin/curl-config
CC := $(shell $(CURL_CONFIG) --cc)
LDFLAGS += $(shell $(CURL_CONFIG) --libs)
LDFLAGS += -lnghttp2
CFLAGS += $(shell $(CURL_CONFIG) --cflags)
CFLAGS += -g

test: main
	./main
	@if [ -s counter.mpd ]; then \
	    echo "test success"; \
	else \
	    echo "test failed"; \
	fi

main: $(obj)
	$(CC) $(obj) $(LDFLAGS) -o $@

serverpush-exp: http2-serverpush.o
	$(CC) $< $(LDFLAGS) -o $@

clean:
	-rm $(obj)
	-rm main
	-rm counter*
	-rm dl
	-rm file
	-rm google.html
