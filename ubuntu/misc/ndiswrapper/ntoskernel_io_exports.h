/* automatically generated from ntoskernel_io.c */
#ifdef CONFIG_X86_64

WIN_FUNC_DECL(ExQueueWorkItem, 2)
WIN_FUNC_DECL(IoAcquireCancelSpinLock, 1)
WIN_FUNC_DECL(IoAcquireRemoveLockEx, 5)
WIN_FUNC_DECL(IoAllocateDriverObjectExtension, 4)
WIN_FUNC_DECL(IoAllocateErrorLogEntry, 2)
WIN_FUNC_DECL(IoAllocateIrp, 2)
WIN_FUNC_DECL(IoAllocateMdl, 5)
WIN_FUNC_DECL(IoAllocateWorkItem, 1)
WIN_FUNC_DECL(IoAttachDeviceToDeviceStack, 2)
WIN_FUNC_DECL(IoBuildAsynchronousFsdRequest, 6)
WIN_FUNC_DECL(IoBuildDeviceIoControlRequest, 9)
WIN_FUNC_DECL(IoBuildSynchronousFsdRequest, 7)
WIN_FUNC_DECL(IoCancelIrp, 1)
WIN_FUNC_DECL(IoConnectInterrupt, 11)
WIN_FUNC_DECL(IoCreateDevice, 7)
WIN_FUNC_DECL(IoCreateSymbolicLink, 2)
WIN_FUNC_DECL(IoCreateUnprotectedSymbolicLink, 2)
WIN_FUNC_DECL(IoDeleteDevice, 1)
WIN_FUNC_DECL(IoDeleteSymbolicLink, 1)
WIN_FUNC_DECL(IoDetachDevice, 1)
WIN_FUNC_DECL(IoDisconnectInterrupt, 1)
WIN_FUNC_DECL(IofCallDriver, 2)
WIN_FUNC_DECL(IofCompleteRequest, 2)
WIN_FUNC_DECL(IoFreeErrorLogEntry, 1)
WIN_FUNC_DECL(IoFreeIrp, 1)
WIN_FUNC_DECL(IoFreeMdl, 1)
WIN_FUNC_DECL(IoFreeWorkItem, 1)
WIN_FUNC_DECL(IoGetAttachedDevice, 1)
WIN_FUNC_DECL(IoGetAttachedDeviceReference, 1)
WIN_FUNC_DECL(IoGetDeviceObjectPointer, 4)
WIN_FUNC_DECL(IoGetDeviceProperty, 5)
WIN_FUNC_DECL(IoGetDriverObjectExtension, 2)
WIN_FUNC_DECL(IoInitializeIrp, 3)
WIN_FUNC_DECL(IoInitializeRemoveLockEx, 5)
WIN_FUNC_DECL(IoInvalidateDeviceRelations, 2)
WIN_FUNC_DECL(IoInvalidateDeviceState, 1)
WIN_FUNC_DECL(IoIrpSyncComplete, 3)
WIN_FUNC_DECL(IoIs32bitProcess, 1)
WIN_FUNC_DECL(IoIsWdmVersionAvailable, 2)
WIN_FUNC_DECL(IoOpenDeviceRegistryKey, 4)
WIN_FUNC_DECL(IoQueueWorkItem, 4)
WIN_FUNC_DECL(IoRegisterDeviceInterface, 4)
WIN_FUNC_DECL(IoReleaseCancelSpinLock, 1)
WIN_FUNC_DECL(IoReleaseRemoveLockEx, 3)
WIN_FUNC_DECL(IoReuseIrp, 2)
WIN_FUNC_DECL(IoSetDeviceInterfaceState, 2)
WIN_FUNC_DECL(IoWMIRegistrationControl, 2)
WIN_FUNC_DECL(IoWriteErrorLogEntry, 1)
WIN_FUNC_DECL(PoCallDriver, 2)
WIN_FUNC_DECL(PoRequestPowerIrp, 6)
WIN_FUNC_DECL(PoSetPowerState, 3)
WIN_FUNC_DECL(PoStartNextPowerIrp, 1)
#endif
struct wrap_export ntoskernel_io_exports[] = {
   
   WIN_SYMBOL(ExQueueWorkItem,2),
   WIN_SYMBOL(IoAcquireCancelSpinLock,1),
   WIN_SYMBOL(IoAcquireRemoveLockEx,5),
   WIN_SYMBOL(IoAllocateDriverObjectExtension,4),
   WIN_SYMBOL(IoAllocateErrorLogEntry,2),
   WIN_SYMBOL(IoAllocateIrp,2),
   WIN_SYMBOL(IoAllocateMdl,5),
   WIN_SYMBOL(IoAllocateWorkItem,1),
   WIN_SYMBOL(IoAttachDeviceToDeviceStack,2),
   WIN_SYMBOL(IoBuildAsynchronousFsdRequest,6),
   WIN_SYMBOL(IoBuildDeviceIoControlRequest,9),
   WIN_SYMBOL(IoBuildSynchronousFsdRequest,7),
   WIN_SYMBOL(IoCancelIrp,1),
   WIN_SYMBOL(IoConnectInterrupt,11),
   WIN_SYMBOL(IoCreateDevice,7),
   WIN_SYMBOL(IoCreateSymbolicLink,2),
   WIN_SYMBOL(IoCreateUnprotectedSymbolicLink,2),
   WIN_SYMBOL(IoDeleteDevice,1),
   WIN_SYMBOL(IoDeleteSymbolicLink,1),
   WIN_SYMBOL(IoDetachDevice,1),
   WIN_SYMBOL(IoDisconnectInterrupt,1),
   WIN_SYMBOL(IofCallDriver,2),
   WIN_SYMBOL(IofCompleteRequest,2),
   WIN_SYMBOL(IoFreeErrorLogEntry,1),
   WIN_SYMBOL(IoFreeIrp,1),
   WIN_SYMBOL(IoFreeMdl,1),
   WIN_SYMBOL(IoFreeWorkItem,1),
   WIN_SYMBOL(IoGetAttachedDevice,1),
   WIN_SYMBOL(IoGetAttachedDeviceReference,1),
   WIN_SYMBOL(IoGetDeviceObjectPointer,4),
   WIN_SYMBOL(IoGetDeviceProperty,5),
   WIN_SYMBOL(IoGetDriverObjectExtension,2),
   WIN_SYMBOL(IoInitializeIrp,3),
   WIN_SYMBOL(IoInitializeRemoveLockEx,5),
   WIN_SYMBOL(IoInvalidateDeviceRelations,2),
   WIN_SYMBOL(IoInvalidateDeviceState,1),
   WIN_SYMBOL(IoIs32bitProcess,1),
   WIN_SYMBOL(IoIsWdmVersionAvailable,2),
   WIN_SYMBOL(IoOpenDeviceRegistryKey,4),
   WIN_SYMBOL(IoQueueWorkItem,4),
   WIN_SYMBOL(IoRegisterDeviceInterface,4),
   WIN_SYMBOL(IoReleaseCancelSpinLock,1),
   WIN_SYMBOL(IoReleaseRemoveLockEx,3),
   WIN_SYMBOL(IoReuseIrp,2),
   WIN_SYMBOL(IoSetDeviceInterfaceState,2),
   WIN_SYMBOL(IoWMIRegistrationControl,2),
   WIN_SYMBOL(IoWriteErrorLogEntry,1),
   WIN_SYMBOL(PoCallDriver,2),
   WIN_SYMBOL(PoRequestPowerIrp,6),
   WIN_SYMBOL(PoSetPowerState,3),
   WIN_SYMBOL(PoStartNextPowerIrp,1),
   {NULL, NULL}
};
