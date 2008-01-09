/* automatically generated from ntoskernel.c */
#ifdef CONFIG_X86_64

WIN_FUNC_DECL(__chkstk, 0)
WIN_FUNC_DECL(__C_specific_handler, 0)
WIN_FUNC_DECL(DbgBreakPoint, 0)
WIN_FUNC_DECL(DbgPrint, 12)
WIN_FUNC_DECL(ExAllocatePoolWithTag, 3)
WIN_FUNC_DECL(_except_handler3, 0)
WIN_FUNC_DECL(ExCreateCallback, 4)
WIN_FUNC_DECL(ExDeleteNPagedLookasideList, 1)
WIN_FUNC_DECL(ExfInterlockedInsertHeadList, 3)
WIN_FUNC_DECL(ExfInterlockedInsertTailList, 3)
WIN_FUNC_DECL(ExfInterlockedRemoveHeadList, 2)
WIN_FUNC_DECL(ExfInterlockedRemoveTailList, 2)
WIN_FUNC_DECL(ExFreePool, 1)
WIN_FUNC_DECL(ExFreePoolWithTag, 2)
WIN_FUNC_DECL(ExInitializeNPagedLookasideList, 7)
WIN_FUNC_DECL(ExInterlockedAddLargeStatistic, 2)
WIN_FUNC_DECL(ExInterlockedInsertHeadList, 3)
WIN_FUNC_DECL(ExInterlockedInsertTailList, 3)
WIN_FUNC_DECL(ExInterlockedPopEntrySList, 2)
WIN_FUNC_DECL(ExInterlockedPushEntrySList, 3)
WIN_FUNC_DECL(ExInterlockedRemoveHeadList, 2)
WIN_FUNC_DECL(ExInterlockedRemoveTailList, 2)
WIN_FUNC_DECL(ExNotifyCallback, 3)
WIN_FUNC_DECL(ExpInterlockedPopEntrySList, 1)
WIN_FUNC_DECL(ExpInterlockedPushEntrySList, 2)
WIN_FUNC_DECL(ExQueryDepthSList, 1)
WIN_FUNC_DECL(ExRegisterCallback, 3)
WIN_FUNC_DECL(ExSetTimerResolution, 2)
WIN_FUNC_DECL(ExSystemTimeToLocalTime, 2)
WIN_FUNC_DECL(ExUnregisterCallback, 1)
WIN_FUNC_DECL(InitializeSListHead, 1)
WIN_FUNC_DECL(InterlockedCompareExchange, 3)
WIN_FUNC_DECL(InterlockedDecrement, 1)
WIN_FUNC_DECL(InterlockedExchange, 2)
WIN_FUNC_DECL(InterlockedIncrement, 1)
WIN_FUNC_DECL(InterlockedPopEntrySList, 1)
WIN_FUNC_DECL(InterlockedPushEntrySList, 2)
WIN_FUNC_DECL(IoBuildPartialMdl, 4)
WIN_FUNC_DECL(KeAcquireSpinLock, 2)
WIN_FUNC_DECL(KeAcquireSpinLockAtDpcLevel, 1)
WIN_FUNC_DECL(KeAcquireSpinLockRaiseToDpc, 1)
WIN_FUNC_DECL(KeBugCheckEx, 5)
WIN_FUNC_DECL(KeCancelTimer, 1)
WIN_FUNC_DECL(KeClearEvent, 1)
WIN_FUNC_DECL(KeDelayExecutionThread, 3)
WIN_FUNC_DECL(KeFlushQueuedDpcs, 0)
WIN_FUNC_DECL(KeGetCurrentThread, 0)
WIN_FUNC_DECL(KeInitializeDpc, 3)
WIN_FUNC_DECL(KeInitializeEvent, 3)
WIN_FUNC_DECL(KeInitializeMutex, 2)
WIN_FUNC_DECL(KeInitializeSemaphore, 3)
WIN_FUNC_DECL(KeInitializeSpinLock, 1)
WIN_FUNC_DECL(KeInitializeTimer, 1)
WIN_FUNC_DECL(KeInitializeTimerEx, 2)
WIN_FUNC_DECL(KeInsertQueueDpc, 3)
WIN_FUNC_DECL(KeLowerIrql, 1)
WIN_FUNC_DECL(KeQueryActiveProcessors, 0)
WIN_FUNC_DECL(KeQueryInterruptTime, 0)
WIN_FUNC_DECL(KeQueryPerformanceCounter, 1)
WIN_FUNC_DECL(KeQueryPriorityThread, 1)
WIN_FUNC_DECL(KeQuerySystemTime, 1)
WIN_FUNC_DECL(KeQueryTickCount, 1)
WIN_FUNC_DECL(KeQueryTimeIncrement, 0)
WIN_FUNC_DECL(KeRaiseIrql, 2)
WIN_FUNC_DECL(KeRaiseIrqlToDpcLevel, 0)
WIN_FUNC_DECL(KeReadStateEvent, 1)
WIN_FUNC_DECL(KeReadStateTimer, 1)
WIN_FUNC_DECL(KeReleaseMutex, 2)
WIN_FUNC_DECL(KeReleaseSemaphore, 4)
WIN_FUNC_DECL(KeReleaseSpinLock, 2)
WIN_FUNC_DECL(KeReleaseSpinLockFromDpcLevel, 1)
WIN_FUNC_DECL(KeRemoveEntryDeviceQueue, 2)
WIN_FUNC_DECL(KeRemoveQueueDpc, 1)
WIN_FUNC_DECL(KeResetEvent, 1)
WIN_FUNC_DECL(KeSetEvent, 3)
WIN_FUNC_DECL(KeSetImportanceDpc, 2)
WIN_FUNC_DECL(KeSetPriorityThread, 2)
WIN_FUNC_DECL(KeSetTimer, 3)
WIN_FUNC_DECL(KeSetTimerEx, 4)
WIN_FUNC_DECL(KeSynchronizeExecution, 3)
WIN_FUNC_DECL(KeWaitForMultipleObjects, 8)
WIN_FUNC_DECL(KeWaitForSingleObject, 5)
WIN_FUNC_DECL(MmAllocateContiguousMemorySpecifyCache, 5)
WIN_FUNC_DECL(MmBuildMdlForNonPagedPool, 1)
WIN_FUNC_DECL(MmFreeContiguousMemorySpecifyCache, 3)
WIN_FUNC_DECL(MmGetPhysicalAddress, 1)
WIN_FUNC_DECL(MmIsAddressValid, 1)
WIN_FUNC_DECL(MmLockPagableDataSection, 1)
WIN_FUNC_DECL(MmMapIoSpace, 3)
WIN_FUNC_DECL(MmMapLockedPages, 2)
WIN_FUNC_DECL(MmMapLockedPagesSpecifyCache, 6)
WIN_FUNC_DECL(MmProbeAndLockPages, 3)
WIN_FUNC_DECL(MmSizeOfMdl, 2)
WIN_FUNC_DECL(MmUnlockPagableImageSection, 1)
WIN_FUNC_DECL(MmUnlockPages, 1)
WIN_FUNC_DECL(MmUnmapIoSpace, 2)
WIN_FUNC_DECL(MmUnmapLockedPages, 2)
WIN_FUNC_DECL(ObfDereferenceObject, 1)
WIN_FUNC_DECL(ObfReferenceObject, 1)
WIN_FUNC_DECL(ObReferenceObjectByHandle, 6)
WIN_FUNC_DECL(PsCreateSystemThread, 7)
WIN_FUNC_DECL(PsTerminateSystemThread, 1)
WIN_FUNC_DECL(_purecall, 0)
WIN_FUNC_DECL(WmiCompleteRequest, 5)
WIN_FUNC_DECL(WmiQueryTraceInformation, 4)
WIN_FUNC_DECL(WmiSystemControl, 4)
WIN_FUNC_DECL(WmiTraceMessage, 12)
WIN_FUNC_DECL(ZwClose, 1)
WIN_FUNC_DECL(ZwCreateFile, 11)
WIN_FUNC_DECL(ZwCreateKey, 7)
WIN_FUNC_DECL(ZwDeleteKey, 1)
WIN_FUNC_DECL(ZwMapViewOfSection, 10)
WIN_FUNC_DECL(ZwOpenFile, 6)
WIN_FUNC_DECL(ZwOpenKey, 3)
WIN_FUNC_DECL(ZwOpenSection, 3)
WIN_FUNC_DECL(ZwPowerInformation, 4)
WIN_FUNC_DECL(ZwQueryInformationFile, 5)
WIN_FUNC_DECL(ZwQueryValueKey, 6)
WIN_FUNC_DECL(ZwReadFile, 9)
WIN_FUNC_DECL(ZwSetValueKey, 6)
WIN_FUNC_DECL(ZwUnmapViewOfSection, 2)
WIN_FUNC_DECL(ZwWriteFile, 9)
#endif
struct wrap_export ntoskernel_exports[] = {
   
   {"KeTickCount",(generic_func)&jiffies},
   {"NlsMbCodePageTag",(generic_func)FALSE},
   WIN_SYMBOL(__chkstk,0),
   WIN_SYMBOL(__C_specific_handler,0),
   WIN_SYMBOL(DbgBreakPoint,0),
   WIN_SYMBOL(DbgPrint,12),
   WIN_SYMBOL(ExAllocatePoolWithTag,3),
   WIN_SYMBOL(_except_handler3,0),
   WIN_SYMBOL(ExCreateCallback,4),
   WIN_SYMBOL(ExDeleteNPagedLookasideList,1),
   WIN_SYMBOL(ExfInterlockedInsertHeadList,3),
   WIN_SYMBOL(ExfInterlockedInsertTailList,3),
   WIN_SYMBOL(ExfInterlockedRemoveHeadList,2),
   WIN_SYMBOL(ExfInterlockedRemoveTailList,2),
   WIN_SYMBOL(ExFreePool,1),
   WIN_SYMBOL(ExFreePoolWithTag,2),
   WIN_SYMBOL(ExInitializeNPagedLookasideList,7),
   WIN_SYMBOL(ExInterlockedAddLargeStatistic,2),
   WIN_SYMBOL(ExInterlockedInsertHeadList,3),
   WIN_SYMBOL(ExInterlockedInsertTailList,3),
   WIN_SYMBOL(ExInterlockedPopEntrySList,2),
   WIN_SYMBOL(ExInterlockedPushEntrySList,3),
   WIN_SYMBOL(ExInterlockedRemoveHeadList,2),
   WIN_SYMBOL(ExInterlockedRemoveTailList,2),
   WIN_SYMBOL(ExNotifyCallback,3),
   WIN_SYMBOL(ExpInterlockedPopEntrySList,1),
   WIN_SYMBOL(ExpInterlockedPushEntrySList,2),
   WIN_SYMBOL(ExQueryDepthSList,1),
   WIN_SYMBOL(ExRegisterCallback,3),
   WIN_SYMBOL(ExSetTimerResolution,2),
   WIN_SYMBOL(ExSystemTimeToLocalTime,2),
   WIN_SYMBOL(ExUnregisterCallback,1),
   WIN_SYMBOL(InitializeSListHead,1),
   WIN_SYMBOL(InterlockedCompareExchange,3),
   WIN_SYMBOL(InterlockedDecrement,1),
   WIN_SYMBOL(InterlockedExchange,2),
   WIN_SYMBOL(InterlockedIncrement,1),
   WIN_SYMBOL(InterlockedPopEntrySList,1),
   WIN_SYMBOL(InterlockedPushEntrySList,2),
   WIN_SYMBOL(IoBuildPartialMdl,4),
   WIN_SYMBOL(KeAcquireSpinLock,2),
   WIN_SYMBOL(KeAcquireSpinLockAtDpcLevel,1),
   WIN_SYMBOL(KeAcquireSpinLockRaiseToDpc,1),
   WIN_SYMBOL(KeBugCheckEx,5),
   WIN_SYMBOL(KeCancelTimer,1),
   WIN_SYMBOL(KeClearEvent,1),
   WIN_SYMBOL(KeDelayExecutionThread,3),
   WIN_SYMBOL(KeFlushQueuedDpcs,0),
   WIN_SYMBOL(KeGetCurrentThread,0),
   WIN_SYMBOL(KeInitializeDpc,3),
   WIN_SYMBOL(KeInitializeEvent,3),
   WIN_SYMBOL(KeInitializeMutex,2),
   WIN_SYMBOL(KeInitializeSemaphore,3),
   WIN_SYMBOL(KeInitializeSpinLock,1),
   WIN_SYMBOL(KeInitializeTimer,1),
   WIN_SYMBOL(KeInitializeTimerEx,2),
   WIN_SYMBOL(KeInsertQueueDpc,3),
   WIN_SYMBOL(KeLowerIrql,1),
   WIN_SYMBOL(KeQueryActiveProcessors,0),
   WIN_SYMBOL(KeQueryInterruptTime,0),
   WIN_SYMBOL(KeQueryPerformanceCounter,1),
   WIN_SYMBOL(KeQueryPriorityThread,1),
   WIN_SYMBOL(KeQuerySystemTime,1),
   WIN_SYMBOL(KeQueryTickCount,1),
   WIN_SYMBOL(KeQueryTimeIncrement,0),
   WIN_SYMBOL(KeRaiseIrql,2),
   WIN_SYMBOL(KeRaiseIrqlToDpcLevel,0),
   WIN_SYMBOL(KeReadStateEvent,1),
   WIN_SYMBOL(KeReadStateTimer,1),
   WIN_SYMBOL(KeReleaseMutex,2),
   WIN_SYMBOL(KeReleaseSemaphore,4),
   WIN_SYMBOL(KeReleaseSpinLock,2),
   WIN_SYMBOL(KeReleaseSpinLockFromDpcLevel,1),
   WIN_SYMBOL(KeRemoveEntryDeviceQueue,2),
   WIN_SYMBOL(KeRemoveQueueDpc,1),
   WIN_SYMBOL(KeResetEvent,1),
   WIN_SYMBOL(KeSetEvent,3),
   WIN_SYMBOL(KeSetImportanceDpc,2),
   WIN_SYMBOL(KeSetPriorityThread,2),
   WIN_SYMBOL(KeSetTimer,3),
   WIN_SYMBOL(KeSetTimerEx,4),
   WIN_SYMBOL(KeSynchronizeExecution,3),
   WIN_SYMBOL(KeWaitForMultipleObjects,8),
   WIN_SYMBOL(KeWaitForSingleObject,5),
   WIN_SYMBOL(MmAllocateContiguousMemorySpecifyCache,5),
   WIN_SYMBOL(MmBuildMdlForNonPagedPool,1),
   WIN_SYMBOL(MmFreeContiguousMemorySpecifyCache,3),
   WIN_SYMBOL(MmGetPhysicalAddress,1),
   WIN_SYMBOL(MmIsAddressValid,1),
   WIN_SYMBOL(MmLockPagableDataSection,1),
   WIN_SYMBOL(MmMapIoSpace,3),
   WIN_SYMBOL(MmMapLockedPages,2),
   WIN_SYMBOL(MmMapLockedPagesSpecifyCache,6),
   WIN_SYMBOL(MmProbeAndLockPages,3),
   WIN_SYMBOL(MmSizeOfMdl,2),
   WIN_SYMBOL(MmUnlockPagableImageSection,1),
   WIN_SYMBOL(MmUnlockPages,1),
   WIN_SYMBOL(MmUnmapIoSpace,2),
   WIN_SYMBOL(MmUnmapLockedPages,2),
   WIN_SYMBOL(ObfDereferenceObject,1),
   WIN_SYMBOL(ObfReferenceObject,1),
   WIN_SYMBOL(ObReferenceObjectByHandle,6),
   WIN_SYMBOL(PsCreateSystemThread,7),
   WIN_SYMBOL(PsTerminateSystemThread,1),
   WIN_SYMBOL(_purecall,0),
   WIN_SYMBOL(WmiCompleteRequest,5),
   WIN_SYMBOL(WmiQueryTraceInformation,4),
   WIN_SYMBOL(WmiSystemControl,4),
   WIN_SYMBOL(WmiTraceMessage,12),
   WIN_SYMBOL(ZwClose,1),
   WIN_SYMBOL(ZwCreateFile,11),
   WIN_SYMBOL(ZwCreateKey,7),
   WIN_SYMBOL(ZwDeleteKey,1),
   WIN_SYMBOL(ZwMapViewOfSection,10),
   WIN_SYMBOL(ZwOpenFile,6),
   WIN_SYMBOL(ZwOpenKey,3),
   WIN_SYMBOL(ZwOpenSection,3),
   WIN_SYMBOL(ZwPowerInformation,4),
   WIN_SYMBOL(ZwQueryInformationFile,5),
   WIN_SYMBOL(ZwQueryValueKey,6),
   WIN_SYMBOL(ZwReadFile,9),
   WIN_SYMBOL(ZwSetValueKey,6),
   WIN_SYMBOL(ZwUnmapViewOfSection,2),
   WIN_SYMBOL(ZwWriteFile,9),
   {NULL, NULL}
};
