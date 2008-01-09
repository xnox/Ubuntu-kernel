/* automatically generated from ndis.c */
#ifdef CONFIG_X86_64

WIN_FUNC_DECL(alloc_shared_memory_async, 2)
WIN_FUNC_DECL(deserialized_irq_handler, 4)
WIN_FUNC_DECL(EthRxComplete, 1)
WIN_FUNC_DECL(EthRxIndicateHandler, 8)
WIN_FUNC_DECL(mp_timer_dpc, 4)
WIN_FUNC_DECL(NdisAcquireReadWriteLock, 3)
WIN_FUNC_DECL(NdisAcquireSpinLock, 1)
WIN_FUNC_DECL(NdisAdjustBufferLength, 2)
WIN_FUNC_DECL(NdisAllocateBuffer, 5)
WIN_FUNC_DECL(NdisAllocateBufferPool, 3)
WIN_FUNC_DECL(NdisAllocateMemory, 4)
WIN_FUNC_DECL(NdisAllocateMemoryWithTag, 3)
WIN_FUNC_DECL(NdisAllocatePacket, 3)
WIN_FUNC_DECL(NdisAllocatePacketPool, 4)
WIN_FUNC_DECL(NdisAllocatePacketPoolEx, 5)
WIN_FUNC_DECL(NdisAllocateSpinLock, 1)
WIN_FUNC_DECL(NdisAnsiStringToUnicodeString, 2)
WIN_FUNC_DECL(NdisBufferLength, 1)
WIN_FUNC_DECL(NDIS_BUFFER_TO_SPAN_PAGES, 1)
WIN_FUNC_DECL(NdisBufferVirtualAddress, 1)
WIN_FUNC_DECL(NdisCancelTimer, 2)
WIN_FUNC_DECL(NdisCloseConfiguration, 1)
WIN_FUNC_DECL(NdisCloseFile, 1)
WIN_FUNC_DECL(NdisCompletePnPEvent, 2)
WIN_FUNC_DECL(NdisCopyFromPacketToPacket, 6)
WIN_FUNC_DECL(NdisCopyFromPacketToPacketSafe, 7)
WIN_FUNC_DECL(NdisDprAcquireSpinLock, 1)
WIN_FUNC_DECL(NdisDprAllocatePacket, 3)
WIN_FUNC_DECL(NdisDprReleaseSpinLock, 1)
WIN_FUNC_DECL(NdisFreeBuffer, 1)
WIN_FUNC_DECL(NdisFreeBufferPool, 1)
WIN_FUNC_DECL(NdisFreeMemory, 3)
WIN_FUNC_DECL(NdisFreePacket, 1)
WIN_FUNC_DECL(NdisFreePacketPool, 1)
WIN_FUNC_DECL(NdisFreeSpinLock, 1)
WIN_FUNC_DECL(NdisGetBufferPhysicalArraySize, 2)
WIN_FUNC_DECL(NdisGetCurrentProcessorCounts, 3)
WIN_FUNC_DECL(NdisGetCurrentSystemTime, 1)
WIN_FUNC_DECL(NdisGetFirstBufferFromPacket, 6)
WIN_FUNC_DECL(NdisGetFirstBufferFromPacketSafe, 6)
WIN_FUNC_DECL(NdisGetRoutineAddress, 1)
WIN_FUNC_DECL(NdisGetSystemUpTime, 1)
WIN_FUNC_DECL(NdisGetVersion, 0)
WIN_FUNC_DECL(NdisIMCopySendPerPacketInfo, 2)
WIN_FUNC_DECL(NdisIMGetCurrentPacketStack, 2)
WIN_FUNC_DECL(NdisImmediateReadPciSlotInformation, 5)
WIN_FUNC_DECL(NdisImmediateReadPortUchar, 3)
WIN_FUNC_DECL(NdisImmediateWritePortUchar, 3)
WIN_FUNC_DECL(NdisIMNotifiyPnPEvent, 2)
WIN_FUNC_DECL(NdisInitAnsiString, 2)
WIN_FUNC_DECL(NdisInitializeEvent, 1)
WIN_FUNC_DECL(NdisInitializeReadWriteLock, 1)
WIN_FUNC_DECL(NdisInitializeString, 2)
WIN_FUNC_DECL(NdisInitializeTimer, 3)
WIN_FUNC_DECL(NdisInitializeWrapper, 4)
WIN_FUNC_DECL(NdisInitUnicodeString, 2)
WIN_FUNC_DECL(NdisInterlockedDecrement, 1)
WIN_FUNC_DECL(NdisInterlockedIncrement, 1)
WIN_FUNC_DECL(NdisInterlockedInsertHeadList, 3)
WIN_FUNC_DECL(NdisInterlockedInsertTailList, 3)
WIN_FUNC_DECL(NdisInterlockedRemoveHeadList, 2)
WIN_FUNC_DECL(ndis_isr, 2)
WIN_FUNC_DECL(NdisMAllocateMapRegisters, 5)
WIN_FUNC_DECL(NdisMAllocateSharedMemory, 5)
WIN_FUNC_DECL(NdisMAllocateSharedMemoryAsync, 4)
WIN_FUNC_DECL(NdisMapFile, 3)
WIN_FUNC_DECL(NdisMCancelTimer, 2)
WIN_FUNC_DECL(NdisMCoActivateVcComplete, 3)
WIN_FUNC_DECL(NdisMCoDeactivateVcComplete, 2)
WIN_FUNC_DECL(NdisMCoIndicateReceivePacket, 3)
WIN_FUNC_DECL(NdisMCompleteBufferPhysicalMapping, 3)
WIN_FUNC_DECL(NdisMCoRequestComplete, 3)
WIN_FUNC_DECL(NdisMCoSendComplete, 3)
WIN_FUNC_DECL(NdisMDeregisterAdapterShutdownHandler, 1)
WIN_FUNC_DECL(NdisMDeregisterDevice, 1)
WIN_FUNC_DECL(NdisMDeregisterInterrupt, 1)
WIN_FUNC_DECL(NdisMDeregisterIoPortRange, 4)
WIN_FUNC_DECL(NdisMFreeMapRegisters, 1)
WIN_FUNC_DECL(NdisMFreeSharedMemory, 5)
WIN_FUNC_DECL(NdisMGetDeviceProperty, 6)
WIN_FUNC_DECL(NdisMGetDmaAlignment, 1)
WIN_FUNC_DECL(NdisMIndicateReceivePacket, 3)
WIN_FUNC_DECL(NdisMIndicateStatus, 4)
WIN_FUNC_DECL(NdisMIndicateStatusComplete, 1)
WIN_FUNC_DECL(NdisMInitializeScatterGatherDma, 3)
WIN_FUNC_DECL(NdisMInitializeTimer, 4)
WIN_FUNC_DECL(NdisMMapIoSpace, 4)
WIN_FUNC_DECL(NdisMPciAssignResources, 3)
WIN_FUNC_DECL(NdisMPromoteMiniport, 1)
WIN_FUNC_DECL(NdisMQueryAdapterInstanceName, 2)
WIN_FUNC_DECL(NdisMQueryAdapterResources, 4)
WIN_FUNC_DECL(NdisMQueryInformationComplete, 2)
WIN_FUNC_DECL(NdisMRegisterAdapterShutdownHandler, 3)
WIN_FUNC_DECL(NdisMRegisterDevice, 6)
WIN_FUNC_DECL(NdisMRegisterInterrupt, 7)
WIN_FUNC_DECL(NdisMRegisterIoPortRange, 4)
WIN_FUNC_DECL(NdisMRegisterMiniport, 3)
WIN_FUNC_DECL(NdisMRegisterUnloadHandler, 2)
WIN_FUNC_DECL(NdisMRemoveMiniport, 1)
WIN_FUNC_DECL(NdisMResetComplete, 3)
WIN_FUNC_DECL(NdisMSendComplete, 3)
WIN_FUNC_DECL(NdisMSendResourcesAvailable, 1)
WIN_FUNC_DECL(NdisMSetAttributesEx, 5)
WIN_FUNC_DECL(NdisMSetInformationComplete, 2)
WIN_FUNC_DECL(NdisMSetMiniportSecondary, 2)
WIN_FUNC_DECL(NdisMSetPeriodicTimer, 2)
WIN_FUNC_DECL(NdisMSleep, 1)
WIN_FUNC_DECL(NdisMStartBufferPhysicalMapping, 6)
WIN_FUNC_DECL(NdisMSynchronizeWithInterrupt, 3)
WIN_FUNC_DECL(NdisMTransferDataComplete, 4)
WIN_FUNC_DECL(NdisMUnmapIoSpace, 3)
WIN_FUNC_DECL(NdisOpenConfiguration, 3)
WIN_FUNC_DECL(NdisOpenConfigurationKeyByIndex, 5)
WIN_FUNC_DECL(NdisOpenConfigurationKeyByName, 4)
WIN_FUNC_DECL(NdisOpenFile, 5)
WIN_FUNC_DECL(NdisOpenProtocolConfiguration, 3)
WIN_FUNC_DECL(NdisPacketPoolUsage, 1)
WIN_FUNC_DECL(NdisQueryBuffer, 3)
WIN_FUNC_DECL(NdisQueryBufferOffset, 3)
WIN_FUNC_DECL(NdisQueryBufferSafe, 4)
WIN_FUNC_DECL(NdisReadConfiguration, 5)
WIN_FUNC_DECL(NdisReadNetworkAddress, 4)
WIN_FUNC_DECL(NdisReadPciSlotInformation, 5)
WIN_FUNC_DECL(NdisReadPcmciaAttributeMemory, 4)
WIN_FUNC_DECL(NdisReadPortUchar, 3)
WIN_FUNC_DECL(NdisReleaseReadWriteLock, 2)
WIN_FUNC_DECL(NdisReleaseSpinLock, 1)
WIN_FUNC_DECL(NdisResetEvent, 1)
WIN_FUNC_DECL(NdisScheduleWorkItem, 1)
WIN_FUNC_DECL(NdisSend, 3)
WIN_FUNC_DECL(NdisSetEvent, 1)
WIN_FUNC_DECL(NdisSetTimer, 2)
WIN_FUNC_DECL(NdisSystemProcessorCount, 0)
WIN_FUNC_DECL(NdisTerminateWrapper, 2)
WIN_FUNC_DECL(NdisUnchainBufferAtBack, 2)
WIN_FUNC_DECL(NdisUnchainBufferAtFront, 2)
WIN_FUNC_DECL(NdisUnicodeStringToAnsiString, 2)
WIN_FUNC_DECL(NdisUnmapFile, 1)
WIN_FUNC_DECL(NdisUpcaseUnicodeString, 2)
WIN_FUNC_DECL(NdisWaitEvent, 2)
WIN_FUNC_DECL(NdisWriteConfiguration, 4)
WIN_FUNC_DECL(NdisWriteErrorLogEntry, 12)
WIN_FUNC_DECL(NdisWriteEventLogEntry, 7)
WIN_FUNC_DECL(NdisWritePciSlotInformation, 5)
WIN_FUNC_DECL(NdisWritePcmciaAttributeMemory, 4)
WIN_FUNC_DECL(NdisWritePortUchar, 3)
WIN_FUNC_DECL(return_packet, 2)
WIN_FUNC_DECL(serialized_irq_handler, 4)
#endif
struct wrap_export ndis_exports[] = {
   
   WIN_SYMBOL(NdisAcquireReadWriteLock,3),
   WIN_SYMBOL(NdisAcquireSpinLock,1),
   WIN_SYMBOL(NdisAdjustBufferLength,2),
   WIN_SYMBOL(NdisAllocateBuffer,5),
   WIN_SYMBOL(NdisAllocateBufferPool,3),
   WIN_SYMBOL(NdisAllocateMemory,4),
   WIN_SYMBOL(NdisAllocateMemoryWithTag,3),
   WIN_SYMBOL(NdisAllocatePacket,3),
   WIN_SYMBOL(NdisAllocatePacketPool,4),
   WIN_SYMBOL(NdisAllocatePacketPoolEx,5),
   WIN_SYMBOL(NdisAllocateSpinLock,1),
   WIN_SYMBOL(NdisAnsiStringToUnicodeString,2),
   WIN_SYMBOL(NdisBufferLength,1),
   WIN_SYMBOL(NDIS_BUFFER_TO_SPAN_PAGES,1),
   WIN_SYMBOL(NdisBufferVirtualAddress,1),
   WIN_SYMBOL(NdisCancelTimer,2),
   WIN_SYMBOL(NdisCloseConfiguration,1),
   WIN_SYMBOL(NdisCloseFile,1),
   WIN_SYMBOL(NdisCompletePnPEvent,2),
   WIN_SYMBOL(NdisCopyFromPacketToPacket,6),
   WIN_SYMBOL(NdisCopyFromPacketToPacketSafe,7),
   WIN_SYMBOL(NdisDprAcquireSpinLock,1),
   WIN_SYMBOL(NdisDprAllocatePacket,3),
   WIN_SYMBOL(NdisDprReleaseSpinLock,1),
   WIN_SYMBOL(NdisFreeBuffer,1),
   WIN_SYMBOL(NdisFreeBufferPool,1),
   WIN_SYMBOL(NdisFreeMemory,3),
   WIN_SYMBOL(NdisFreePacket,1),
   WIN_SYMBOL(NdisFreePacketPool,1),
   WIN_SYMBOL(NdisFreeSpinLock,1),
   WIN_SYMBOL(NdisGetBufferPhysicalArraySize,2),
   WIN_SYMBOL(NdisGetCurrentProcessorCounts,3),
   WIN_SYMBOL(NdisGetCurrentSystemTime,1),
   WIN_SYMBOL(NdisGetFirstBufferFromPacket,6),
   WIN_SYMBOL(NdisGetFirstBufferFromPacketSafe,6),
   WIN_SYMBOL(NdisGetRoutineAddress,1),
   WIN_SYMBOL(NdisGetSystemUpTime,1),
   WIN_SYMBOL(NdisGetVersion,0),
   WIN_SYMBOL(NdisIMCopySendPerPacketInfo,2),
   WIN_SYMBOL(NdisIMGetCurrentPacketStack,2),
   WIN_SYMBOL(NdisImmediateReadPciSlotInformation,5),
   WIN_SYMBOL(NdisImmediateReadPortUchar,3),
   WIN_SYMBOL(NdisImmediateWritePortUchar,3),
   WIN_SYMBOL(NdisIMNotifiyPnPEvent,2),
   WIN_SYMBOL(NdisInitAnsiString,2),
   WIN_SYMBOL(NdisInitializeEvent,1),
   WIN_SYMBOL(NdisInitializeReadWriteLock,1),
   WIN_SYMBOL(NdisInitializeString,2),
   WIN_SYMBOL(NdisInitializeTimer,3),
   WIN_SYMBOL(NdisInitializeWrapper,4),
   WIN_SYMBOL(NdisInitUnicodeString,2),
   WIN_SYMBOL(NdisInterlockedDecrement,1),
   WIN_SYMBOL(NdisInterlockedIncrement,1),
   WIN_SYMBOL(NdisInterlockedInsertHeadList,3),
   WIN_SYMBOL(NdisInterlockedInsertTailList,3),
   WIN_SYMBOL(NdisInterlockedRemoveHeadList,2),
   WIN_SYMBOL(NdisMAllocateMapRegisters,5),
   WIN_SYMBOL(NdisMAllocateSharedMemory,5),
   WIN_SYMBOL(NdisMAllocateSharedMemoryAsync,4),
   WIN_SYMBOL(NdisMapFile,3),
   WIN_SYMBOL(NdisMCancelTimer,2),
   WIN_SYMBOL(NdisMCoActivateVcComplete,3),
   WIN_SYMBOL(NdisMCoDeactivateVcComplete,2),
   WIN_SYMBOL(NdisMCoIndicateReceivePacket,3),
   WIN_SYMBOL(NdisMCompleteBufferPhysicalMapping,3),
   WIN_SYMBOL(NdisMCoRequestComplete,3),
   WIN_SYMBOL(NdisMCoSendComplete,3),
   WIN_SYMBOL(NdisMDeregisterAdapterShutdownHandler,1),
   WIN_SYMBOL(NdisMDeregisterDevice,1),
   WIN_SYMBOL(NdisMDeregisterInterrupt,1),
   WIN_SYMBOL(NdisMDeregisterIoPortRange,4),
   WIN_SYMBOL(NdisMFreeMapRegisters,1),
   WIN_SYMBOL(NdisMFreeSharedMemory,5),
   WIN_SYMBOL(NdisMGetDeviceProperty,6),
   WIN_SYMBOL(NdisMGetDmaAlignment,1),
   WIN_SYMBOL(NdisMIndicateStatus,4),
   WIN_SYMBOL(NdisMIndicateStatusComplete,1),
   WIN_SYMBOL(NdisMInitializeScatterGatherDma,3),
   WIN_SYMBOL(NdisMInitializeTimer,4),
   WIN_SYMBOL(NdisMMapIoSpace,4),
   WIN_SYMBOL(NdisMPciAssignResources,3),
   WIN_SYMBOL(NdisMPromoteMiniport,1),
   WIN_SYMBOL(NdisMQueryAdapterInstanceName,2),
   WIN_SYMBOL(NdisMQueryAdapterResources,4),
   WIN_SYMBOL(NdisMRegisterAdapterShutdownHandler,3),
   WIN_SYMBOL(NdisMRegisterDevice,6),
   WIN_SYMBOL(NdisMRegisterInterrupt,7),
   WIN_SYMBOL(NdisMRegisterIoPortRange,4),
   WIN_SYMBOL(NdisMRegisterMiniport,3),
   WIN_SYMBOL(NdisMRegisterUnloadHandler,2),
   WIN_SYMBOL(NdisMRemoveMiniport,1),
   WIN_SYMBOL(NdisMSetAttributesEx,5),
   WIN_SYMBOL(NdisMSetMiniportSecondary,2),
   WIN_SYMBOL(NdisMSetPeriodicTimer,2),
   WIN_SYMBOL(NdisMSleep,1),
   WIN_SYMBOL(NdisMStartBufferPhysicalMapping,6),
   WIN_SYMBOL(NdisMSynchronizeWithInterrupt,3),
   WIN_SYMBOL(NdisMUnmapIoSpace,3),
   WIN_SYMBOL(NdisOpenConfiguration,3),
   WIN_SYMBOL(NdisOpenConfigurationKeyByIndex,5),
   WIN_SYMBOL(NdisOpenConfigurationKeyByName,4),
   WIN_SYMBOL(NdisOpenFile,5),
   WIN_SYMBOL(NdisOpenProtocolConfiguration,3),
   WIN_SYMBOL(NdisPacketPoolUsage,1),
   WIN_SYMBOL(NdisQueryBuffer,3),
   WIN_SYMBOL(NdisQueryBufferOffset,3),
   WIN_SYMBOL(NdisQueryBufferSafe,4),
   WIN_SYMBOL(NdisReadConfiguration,5),
   WIN_SYMBOL(NdisReadNetworkAddress,4),
   WIN_SYMBOL(NdisReadPciSlotInformation,5),
   WIN_SYMBOL(NdisReadPcmciaAttributeMemory,4),
   WIN_SYMBOL(NdisReadPortUchar,3),
   WIN_SYMBOL(NdisReleaseReadWriteLock,2),
   WIN_SYMBOL(NdisReleaseSpinLock,1),
   WIN_SYMBOL(NdisResetEvent,1),
   WIN_SYMBOL(NdisScheduleWorkItem,1),
   WIN_SYMBOL(NdisSend,3),
   WIN_SYMBOL(NdisSetEvent,1),
   WIN_SYMBOL(NdisSetTimer,2),
   WIN_SYMBOL(NdisSystemProcessorCount,0),
   WIN_SYMBOL(NdisTerminateWrapper,2),
   WIN_SYMBOL(NdisUnchainBufferAtBack,2),
   WIN_SYMBOL(NdisUnchainBufferAtFront,2),
   WIN_SYMBOL(NdisUnicodeStringToAnsiString,2),
   WIN_SYMBOL(NdisUnmapFile,1),
   WIN_SYMBOL(NdisUpcaseUnicodeString,2),
   WIN_SYMBOL(NdisWaitEvent,2),
   WIN_SYMBOL(NdisWriteConfiguration,4),
   WIN_SYMBOL(NdisWriteErrorLogEntry,12),
   WIN_SYMBOL(NdisWriteEventLogEntry,7),
   WIN_SYMBOL(NdisWritePciSlotInformation,5),
   WIN_SYMBOL(NdisWritePcmciaAttributeMemory,4),
   WIN_SYMBOL(NdisWritePortUchar,3),
   {NULL, NULL}
};
