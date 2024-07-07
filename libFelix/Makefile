TOP:=..

all: libFelix.a

CPPSRC=ActionQueue.cpp
CPPSRC+=AudioChannel.cpp
CPPSRC+=BootROMTraps.cpp
CPPSRC+=CPU.cpp
CPPSRC+=CPUState.cpp
CPPSRC+=CartBank.cpp
CPPSRC+=Cartridge.cpp
CPPSRC+=ColOperator.cpp
CPPSRC+=ComLynx.cpp
CPPSRC+=Core.cpp
CPPSRC+=DisplayGenerator.cpp
CPPSRC+=EEPROM.cpp
CPPSRC+=Encryption.cpp
CPPSRC+=GameDrive.cpp
CPPSRC+=ImageBS93.cpp
CPPSRC+=ImageCart.cpp
CPPSRC+=ImageProperties.cpp
CPPSRC+=ImageROM.cpp
CPPSRC+=InputFile.cpp
CPPSRC+=Log.cpp
CPPSRC+=Mikey.cpp
CPPSRC+=ParallelPort.cpp
CPPSRC+=Suzy.cpp
CPPSRC+=SuzyMath.cpp
CPPSRC+=SuzyProcess.cpp
CPPSRC+=SymbolSource.cpp
CPPSRC+=TimerCore.cpp
CPPSRC+=TraceHelper.cpp
CPPSRC+=Utility.cpp
CPPSRC+=VGMWriter.cpp
CPPSRC+=VidOperator.cpp
CPPSRC+=pch.cpp

INC=-I$(TOP)/libextern/multiprecision/include
INC+=-I$(TOP)/libextern/fmt/include

CPPFLAGS=-DNDEBUG -D_LIB -std=c++20 -Wno-deprecated
OBJ=$(CPPSRC:.cpp=.o)

libFelix.a: $(OBJ)
	ar -crLsu $@ $(OBJ)

%.o : %.cpp
	clang++ -c $(CPPFLAGS) $(INC) $< -o $@


clean:
	rm -f $(OBJ)
