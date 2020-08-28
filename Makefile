CC := g++

CFLAGS := -std=c++17 -Iinclude -fpic -O3
LDFLAGS := -L./lib -lsocketwrapper -lpthread

APPNAME = desk_cast

SRCDIR = src
SRCFILES = $(wildcard $(SRCDIR)/*.cpp)
OBJDIR = src
OBJFILES = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCFILES))

.PHONY: all
all: $(APPNAME)

.PHONY: $(APPNAME)
$(APPNAME): $(OBJFILES)
	$(CC) $^ $(LDFLAGS) $(CFLAGS) -o $(APPNAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@ 

.PHONY: clean
clean:
	rm ./$(APPNAME)
	rm -rf $(OBJDIR)/*.o

