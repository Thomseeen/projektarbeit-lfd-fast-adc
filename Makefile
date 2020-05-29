########################################
# Settings
########################################
# Compiler settings
CC = gcc
CXXFLAGS = -Wall
LDFLAGS = -lpruio

# Makefile settings
APPNAME = $(notdir $(abspath .))
EXT = .c
SRCDIR = src
OBJDIR = obj


########################################
# Dynamically set
########################################
SRC = $(wildcard $(SRCDIR)/*$(EXT))
OBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)/%.o)
DEP = $(OBJ:$(OBJDIR)/%.o=%.d)
# UNIX-based OS variables & settings
RM = rm
DELOBJ = $(OBJ)

########################################
# Targets
########################################
all: $(APPNAME)
	@printf "### Finished Make-Build ###\n\n"

# Builds the app
$(APPNAME): $(OBJ)
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Creates the dependecy rules
%.d: $(SRCDIR)/%$(EXT)
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:%.d=$(OBJDIR)/%.o) >$@

# Includes all .h files
-include $(DEP)

# Building rule for .o files and its .c/.cpp in combination with all .h
$(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
	$(CC) $(CXXFLAGS) -o $@ -c $<

################### Cleaning rules for Unix-based OS ###################
# Cleans complete project
.PHONY: clear
clean:
	$(RM) $(DELOBJ) $(DEP) $(APPNAME)
	@printf "### Cleaned ###\n\n"