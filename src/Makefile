CFLAGS=-I/usr/local/include -O2 -Wall -ansi -g
CXXFLAGS=$(CFLAGS)
LDFLAGS=-L/usr/local/lib
BINDIR=../bin
WEBDIR=../htdocs
LDLIBS_INDEXER=-lsmbclient -lcrypto -lmagic -lz
LDLIBS_WEBQUERY=-lfcgi

BINARIES=indexer webquery

all: $(BINARIES)

install: $(BINARIES)
	install -s indexer $(BINDIR)
	install -s webquery $(WEBDIR)/webquery.fcgi

indexer: indexer.o
	$(CXX) $(LDFLAGS) indexer.o $(LDLIBS_INDEXER) -o indexer

webquery: webquery.o
	$(CC) $(LDFLAGS) webquery.o $(LDLIBS_WEBQUERY) -o webquery

clean:
	-rm $(BINARIES)
	-rm *.o

