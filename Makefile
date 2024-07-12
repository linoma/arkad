CC=g++
PKGCONFIG = $(shell which pkg-config)
INCLUDE_DIR=-I.
LIB_DIR=
TARGET=arkad

CF=-fauto-inc-dec -fbranch-count-reg -fcombine-stack-adjustments -fcompare-elim -fcprop-registers -fdce -fdefer-pop
CF+=-fdse -fforward-propagate -fguess-branch-probability -fif-conversion -fif-conversion2 -finline-functions-called-once
CF+=-fipa-profile -fipa-pure-const -fipa-reference -fmerge-constants -fmove-loop-invariants
CF+=-freorder-blocks -fshrink-wrap -fshrink-wrap-separate -fsplit-wide-types -fssa-backprop -fssa-phiopt -ftree-bit-ccp -ftree-ccp -ftree-ch -ftree-coalesce-vars
CF+=-ftree-copy-prop -ftree-dce -ftree-dominator-opts -ftree-dse -ftree-forwprop -ftree-fre -ftree-phiprop -ftree-pta -ftree-scev-cprop -ftree-sink
CF+=-ftree-slsr -ftree-sra -ftree-ter -funit-at-a-time -falign-functions -falign-jumps -falign-labels  -falign-loops -finline-functions -finline-small-functions
CF+=-findirect-inlining -fcaller-saves -fcode-hoisting -fcrossjumping -fcse-follow-jumps  -fcse-skip-blocks -fdelete-null-pointer-checks -fdevirtualize
CF+=-fdevirtualize-speculatively -fexpensive-optimizations

#CFLAGS+=-fno-delayed-branch
#CFLAGS+=-fipa-modref
#CFLAGS+=-fno-move-loop-stores
#CFLAGS+=-fno-omit-frame-pointer
#CFLAGS+=-fno-move-loop-stores
#CF+=-fno-omit-frame-pointer
CF=-O2 -fno-omit-frame-pointer
CF+= -fpermissive
#CF=-ggdb -fpermissive -DLOGLEVEL=9 -Wunused
CF+=-D_DEBUG -Wno-deprecated-declarations -std=c++11

GTK_LIBS=$(shell $(PKGCONFIG) --libs gtk+-3.0 gmodule-2.0)
LIBS=-lm $(GTK_LIBS) -lpthread -lasound -lGL

SUBDIR=cpu gfx audio machine res device
CPPSRC=$(shell find $(foreach fd, $(SUBDIR), $(fd)) -regex '.*\.c[pp]*')
SOURCES=$(wildcard *.cpp $(CPPSRC))
OBJDIR=./obj
CPPOBJ=$(patsubst %.cpp,$(OBJDIR)/%.o, $(SOURCES))
BINS=xpm bin glade
OBJBIN=$(wildcard $(foreach fd, $(BINS), *.$(fd)))
OBJ=$(foreach fd, $(BINS), $(patsubst %.$(fd), $(OBJDIR)/%.o, $(filter %.$(fd), $(OBJBIN))))
OBJ+=$(CPPOBJ)

INCLUDE_DIR+=$(patsubst %,-I%, $(shell find $(foreach fd, $(SUBDIR), $(fd)) -type d))

#OBJ=$(addprefix $(OBJDIR)/, $(SOURCES:glade=o))

DEPS=$(wildcard *.h $(shell find $(foreach fd, $(SUBDIR), $(fd)) -regex '.*\.[hinc]+'))
#DEPS+=$(wildcard *.inc $(foreach fd, $(SUBDIR), $(fd)/*.inc))
#DEPS+=$(wildcard *.inc $(foreach fd, $(SUBDIR), $(fd)/*.inc))
CFLAGS= $(shell $(PKGCONFIG) --cflags gtk+-3.0) $(INCLUDE_DIR) $(LIB_DIR) $(CF)

$(OBJDIR)/%.o: %.c* $(DEPS)
	@echo "Compiling "$<
	@mkdir -p $(@D)
	@$(CC) -c -o $@ $< $(CFLAGS)

define BINARY =
	@echo "Building binary "$(2)
	@mkdir -p $(3)
	@echo -n "\0\0" >> $(1)
	@objcopy -B i386 -I binary -O elf64-x86-64 $(1) $(2)
endef

$(OBJDIR)/%.o: %.glade
	$(call BINARY,$<,$@,$(@D))

$(OBJDIR)/%.o: %.xpm
	$(call BINARY,$<,$@,$(@D))

$(OBJDIR)/%.o: %.bin
	$(call BINARY,$<,$@,$(@D))

$(TARGET): $(OBJ)
	@echo "Linking "$@
	@$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -rdynamic -export-dynamic

.PHONY: clean

clean:
	@echo "Cleaning..."
	@rm -Rf $(OBJDIR)/* $(TARGET) *.ii *.s