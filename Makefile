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
#CF=-O3 -fno-omit-frame-pointer
CF+=-O0 -fpermissive
#CF=-g2 -fpermissive -DLOGLEVEL=9 -D_DEVELOP
CF+=-D_DEBUG -Wno-deprecated-declarations

GTK_LIBS=$(shell $(PKGCONFIG) --libs gtk+-3.0 gmodule-2.0)
LIBS=-lm $(GTK_LIBS) -lpthread -lasound

SUBDIR=cpu gfx audio machine
SOURCES=$(wildcard *.cpp $(foreach fd, $(SUBDIR), $(fd)/*.cpp))
SOURCES+=$(wildcard *.glade $(foreach fd, $(SUBDIR), $(fd)/*.glade))
SOURCES+=$(wildcard *.bin $(foreach fd, $(SUBDIR), $(fd)/*.bin))
OBJDIR=./obj
CPPOBJ=$(patsubst %.cpp,$(OBJDIR)/%.o, $(SOURCES))
OBJ=$(patsubst %.bin,$(OBJDIR)/%.o, $(patsubst %.glade,$(OBJDIR)/%.o, $(CPPOBJ)))
INCLUDE_DIR+=$(patsubst %,-I%, $(SUBDIR))

#OBJ=$(addprefix $(OBJDIR)/, $(SOURCES:glade=o))

DEPS=$(wildcard *.h $(foreach fd, $(SUBDIR), $(fd)/*.h))
#DEPS+=$(wildcard *.inc $(foreach fd, $(SUBDIR), $(fd)/*.inc))
CFLAGS= $(shell $(PKGCONFIG) --cflags gtk+-3.0) $(INCLUDE_DIR) $(LIB_DIR) $(CF)

$(OBJDIR)/%.o: %.c* $(DEPS)
	@echo "Compiling "$<
	@mkdir -p $(@D)
	@$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR)/%.o: %.glade
	@echo "Building "$@
	@objcopy -B i386 -I binary -O elf64-x86-64 $< $@

$(OBJDIR)/%.o: %.bin
	@echo "Building "$@
	@objcopy -B i386 -I binary -O elf64-x86-64 $< $@

$(TARGET): $(OBJ)
	@echo "Linking "$@
	@$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -rdynamic -export-dynamic

.PHONY: clean

clean:
	@echo "Cleaning..."
	@rm -Rf $(OBJDIR)/* $(TARGET) *.ii *.s