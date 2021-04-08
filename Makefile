CC := g++
# CC := clang++
PROTOC := protoc

CFLAGS := -std=c++17 -I./include -fpic -O3 -Werror -Wall -Wextra -pedantic -Wno-unused
LDFLAGS := -std=c++17 -L./lib -lpthread -lcrypto -lssl -lprotobuf -lfmt -lavcodec -lavformat -lavdevice -lavutil

APPNAME = desk_cast

SRCDIR = src
SRCFILES = $(shell find ./$(SRCDIR) -name '*.cpp')
OBJDIR = src
OBJFILES = ${SRCFILES:.cpp=.o}

PROTODIR = protos
PROTOFILES = $(wildcard $(PROTODIR)/*.proto)
PROTOOBJ = $(patsubst $(PROTODIR)/%.proto, $(PROTODIR)/%.pb.o, $(PROTOFILES))

.PHONY: all
all: $(APPNAME)

.PHONY: $(APPNAME)
$(APPNAME): $(PROTOOBJ) $(OBJFILES)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $(APPNAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(PROTODIR)/%.pb.o: $(PROTODIR)/%.pb.cc
	$(CC) $(CFLAGS) -c $< -o $@

$(PROTODIR)/%.pb.cc: $(PROTODIR)/%.proto
	$(PROTOC) -I=./protos --cpp_out=./protos $<

.PHONY: clean
clean:
	rm -rf $(OBJDIR)/*.o
	rm ./$(APPNAME)
