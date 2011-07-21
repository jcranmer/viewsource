# The native-tools makefile autobuilder

# A list of all of the tools we can build
TOOLS := dumpclang gen

define makeProgram
ifndef $(1)_FLAGS
	$(1)_FLAGS :=
endif
ifndef $(1)_LDFLAGS
	$(1)_LDFLAGS :=
endif

lib$(1).so: $$(patsubst %.cpp,%.o,$$(wildcard $(1)-*.cpp))
		$$(CXX) $$($(1)_LDFLAGS) -o $$@ $$<

$(1)-%.o: $(1)-%.cpp $$(wildcard $(1)-*.h)
	$$(CXX) -c $$(CXXFLAGS) $$($(1)_FLAGS) -o $$@ $$<
endef

dumpclang_FLAGS := -Wall -std=c++0x -I$(shell llvm-config --includedir) \
	-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -fPIC -fno-exceptions -fno-rtti
dumpclang_LDFLAGS := -fPIC -Wl,-R -Wl,'$$ORIGIN' -shared \
	-L$(shell llvm-config --libdir)
gen_FLAGS := $(dumpclang_FLAGS)
gen_LDFLAGS := $(dumpclang_LDFLAGS)

$(foreach tool,$(TOOLS),$(eval $(call makeProgram,$(tool))))

dumpclang-gen-info.h: libgen.so $(shell which clang++)
	clang++ -cc1 -load $(shell pwd)/libgen.so -plugin gen-file \
		-I$(shell llvm-config --includedir) -D__STDC_LIMIT_MACROS \
		-D__STDC_CONSTANT_MACROS -std=c++0x gen-clang.cpp > $@
dumpclang-roots.o: dumpclang-gen-info.h

clean:
	rm -rf *.o $(foreach tool,$(TOOLS),lib$(tool).so) dumpclang-gen-info.h

.PHONY: clean
