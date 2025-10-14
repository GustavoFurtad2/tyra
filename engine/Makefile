# -------------------------------------------------------------------
# Tyra Engine Library Makefile
# -------------------------------------------------------------------

TARGET      := libtyra.a

# Diretórios
SRCDIR      := src
INCDIR      := inc
BUILDDIR    := obj
TARGETDIR   := bin
RESDIR      := res

SRCEXT      := cpp
OBJEXT      := o

# Flags e Includes
CXX         := mips64r5900el-ps2-elf-g++
CXXFLAGS    := -D_EE -Wall -O3 -I$(INCDIR) -I/tyra/engine/inc \
               -I/usr/local/ps2dev/ps2sdk/ee/include \
               -I/usr/local/ps2dev/ps2sdk/common/include \
               -I/usr/local/ps2dev/ps2sdk/ports/include

AR          := mips64r5900el-ps2-elf-ar

# -------------------------------------------------------------------
# Listagem de todos os arquivos .cpp dentro de SRCDIR recursivamente
SRCS := $(shell find $(SRCDIR) -name '*.cpp')

# Converte os caminhos dos fontes para objetos
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SRCS:%.cpp=%.o))

# -------------------------------------------------------------------
# Regra padrão para compilar .cpp -> .o
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -------------------------------------------------------------------
# Regra para criar a biblioteca estática
$(TARGET): $(OBJECTS)
	@mkdir -p $(TARGETDIR)
	$(AR) rcs $(TARGETDIR)/$(TARGET) $(OBJECTS)
	@echo "==== $(TARGET) criado com sucesso! ===="

# -------------------------------------------------------------------
# Limpeza
.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(TARGETDIR)/$(TARGET)
	@echo "==== Limpeza concluída ===="
