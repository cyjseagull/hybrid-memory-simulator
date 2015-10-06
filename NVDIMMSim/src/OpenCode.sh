#!/bin/sh

emacs Util.cpp TraceBasedSim.cpp SimObj.cpp Plane.cpp NVDIMM.cpp Init.cpp P8PGCLogger.cpp P8PLogger.cpp GCLogger.cpp Logger.cpp GCFtl.cpp Ftl.cpp FrontBuffer.cpp FlashTransaction.cpp  Die.cpp Controller.cpp ChannelPacket.cpp Channel.cpp Buffer.cpp Block.cpp --eval '(delete-other-windows)'&

emacs Util.h TraceBasedSim.h SimObj.h Plane.h NVDIMM.h Init.h P8PGCLogger.h P8PLogger.h GCLogger.h Logger.h GCFtl.h Ftl.h FrontBuffer.h FlashTransaction.h FlashConfiguration.h Die.h Controller.h ChannelPacket.h Channel.h Callbacks.h Buffer.h Block.h --eval '(delete-other-windows)'&

echo opening files
