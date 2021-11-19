# PD or MAX
EXT_TARGET = PD

# libraries
DEPENDENCIES_BASE = lib

# external sources
EXT_SRC = src/airspyhf~.cpp

# dependencies
# flext
FLEXT_INCLUDE = $(DEPENDENCIES_BASE)/flext/source
# airspyhf
AIRSPYHF_INCLUDE = /usr/local/include/libairspyhf
AIRSPYHF_LIB = /usr/local/lib/libairspyhf.a /usr/local/lib/libusb-1.0.a
AIRSPYHF_DYLIB = -L/usr/local/lib -lairspyhf.1.6.8

# max msp
CYCLING_BASE = /path/to/max-sdk/source/c74support
MAX_INCLUDE = $(CYCLING_BASE)/max-includes
MSP_INCLUDE = $(CYCLING_BASE)/msp-includes

ifeq ($(OS),Windows_NT)
    REMOVE_DEAD =
    FUNCTION_SECTIONS=
    FLEXT_INLINE=
    ADDITIONAL_CFLAGS=-D_WIN32_WINDOWS=0x0601
    MINGW_LDFLAGS=-Wl,--enable-auto-import $(PD_BIN)/pd.dll -Lwin32/lib
    POST_LDFLAGS= -lflext-pd_s.0.6.0
    MACOS_VERSION_MIN=
else
    UNAME_S := $(shell uname -s)
    MINGW_LDFLAGS=
    POST_LDFLAGS =
    ADDITIONAL_CFLAGS=
    FLEXT_INLINE=-DFLEXT_INLINE
    #FUNCTION_SECTIONS=-fdata-sections -ffunction-sections
    ifeq ($(UNAME_S),Linux)
        REMOVE_DEAD = -Wl,--gc-sections
        MACOS_VERSION_MIN=
    endif
    ifeq ($(UNAME_S),Darwin)

        REMOVE_DEAD = -dead_strip

        #ADDITIONAL_CFLAGS += -isysroot /path/to/MacOSX10.13.sdk
        ADDITIONAL_CFLAGS += -mmacosx-version-min=10.15
        ADDITIONAL_CFLAGS += -arch x86_64
        
        ifeq ($(EXT_TARGET),MAX)
            # compile for older maxmsp
            # ADDITIONAL_CFLAGS += -arch i386
            # even ppc?
            # ADDITIONAL_CFLAGS += -arch ppc 
        endif

    endif
endif

FLEXT_CPPFLAGS = $(FLEXT_INLINE)

cflags = -I$(FLEXT_INCLUDE) $(ADDITIONAL_CFLAGS) -I$(AIRSPYHF_INCLUDE) $(FLEXT_CPPFLAGS)
ldflags = $(REMOVE_DEAD) $(MINGW_LDFLAGS)

#################################
# PD
#################################
ifeq ($(EXT_TARGET),PD)

    FLEXT_CPPFLAGS += -DPD
    
    ldflags += $(AIRSPYHF_LIB)

    # pd-lib
    c.flags = $(cflags) $(FLEXT_CPPFLAGS)
    cxx.flags = -std=c++11
    
    class.sources = $(EXT_SRC)
    suppress-wunused = yes
    
    # include pd-lib-builder
    PDINCLUDEDIR = $(DEPENDENCIES_BASE)/pd
    PDLIBBUILDER_DIR=pd-lib-builder/
    include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

endif

#################################
# Max
#################################
ifeq ($(EXT_TARGET),MAX)

    FLEXT_CPPFLAGS += -DMAXMSP
    ODIR=obj
    
    cflags += $(FLEXT_CPPFLAGS) -I$(MAX_INCLUDE) -I$(MSP_INCLUDE) -F$(MAX_INCLUDE) -F$(MSP_INCLUDE)
    ldflags += $(AIRSPYHF_DYLIB) -framework MaxAPI -framework MaxAudioAPI
    
    cppflags = -std=c++11 $(cflags)
    
    _OBJ=$(EXT_SRC:.cpp=.o)   
    OBJ = $(patsubst %,$(ODIR)/%, $(_OBJ))
    
	
$(ODIR)/%.o: %.cpp $(EXT_SRC)
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(cppflags)

ifeq ($(OS),Windows_NT)
    # TODO: windows
else
ifeq ($(UNAME_S),Darwin)
    # darwin
all: $(OBJ)
	$(CXX) -shared -fPIC -o airspyhf~ $^ $(cppflags) $(ldflags)

	rm -rf airspyhf~.mxo
	cp -r package/template.mxo airspyhf~.mxo
    mkdir -p airspyhf~.mxo/Contents/MacOS/
	mv airspyhf~ airspyhf~.mxo/Contents/MacOS/

	# copy libs
	cp /usr/local/lib/libairspyhf.1.6.8.dylib airspyhf~.mxo/Contents/MacOS/
	cp /usr/local/Cellar/libusb/1.0.24/lib/libusb-1.0.0.dylib airspyhf~.mxo/Contents/MacOS/

	install_name_tool -id @loader_path/libairspyhf.1.6.8.dylib airspyhf~.mxo/Contents/MacOS/libairspyhf.1.6.8.dylib
	install_name_tool -id @loader_path/libusb-1.0.0.dylib airspyhf~.mxo/Contents/MacOS/libusb-1.0.0.dylib
	install_name_tool -change /usr/local/opt/libusb/lib/libusb-1.0.0.dylib @loader_path/libusb-1.0.0.dylib airspyhf~.mxo/Contents/MacOS/libairspyhf.1.6.8.dylib
	install_name_tool -change /usr/local/lib/libairspyhf.0.dylib @loader_path/libairspyhf.1.6.8.dylib airspyhf~.mxo/Contents/MacOS/airspyhf~
endif
endif


.PHONY: clean

clean:
	rm -rf $(ODIR)
	rm -rf airspyhf~ airspyhf~.mxo
endif

